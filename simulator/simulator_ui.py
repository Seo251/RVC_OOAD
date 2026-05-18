"""Tkinter simulator that drives the RVC controller autonomously."""

from __future__ import annotations

import json
import tkinter as tk
from pathlib import Path
from tkinter import filedialog, messagebox

from rvc_client import RvcClient


CELL_SIZE = 32
GRID_SIZE = 12

# Every simulation tick equals one second. A Forward / Backward / TurnLeft /
# TurnRight motion is animated as exactly one tick, so the user sees the robot
# move at one cell per second.
TICK_MS = 1000

# Cleaner power-up booster lasts exactly three ticks. This matches the
# controller-side 3-second timer so the visualization shows the booster for the
# same duration as the underlying cleaning logic.
POWER_UP_MS = 3 * TICK_MS

# Op name used at connect time to fetch the current snapshot without changing
# any state. The C++ bridge ignores unknown ops and just returns the snapshot,
# which lets us seed the event counters and avoid replaying events that the
# server accumulated during a previous simulator session.
SYNC_OP = "__sync__"

LEFT_TURN = {
    (0, -1): (-1, 0),
    (-1, 0): (0, 1),
    (0, 1): (1, 0),
    (1, 0): (0, -1),
}
RIGHT_TURN = {value: key for key, value in LEFT_TURN.items()}
HEADING_NAMES = {
    (0, -1): "North",
    (1, 0): "East",
    (0, 1): "South",
    (-1, 0): "West",
}


class SimulatorUi:
    def __init__(self) -> None:
        self.root = tk.Tk()
        self.root.title("RVC Controller Simulator")
        self.client = RvcClient()
        self.obstacles: set[tuple[int, int]] = set()
        self.dust: set[tuple[int, int]] = set()
        self.robot = (GRID_SIZE // 2, GRID_SIZE // 2)
        self.heading = (0, -1)
        self.snapshot: dict[str, object] = {}

        # Counters track how many controller-side events the UI has already
        # consumed, so that each new RequestFrontCheck / RequestSideCheck /
        # timer event triggers exactly one automated response.
        self.applied_motor_event_count = 0
        self.applied_front_sensor_event_count = 0
        self.applied_side_sensor_event_count = 0
        self.applied_timer_event_count = 0

        self.pending_motion_complete: str | None = None
        self.auto_drive_after_id: str | None = None
        self.power_up_after_id: str | None = None
        self.visual_suite: dict[str, object] | None = None
        self.visual_case_index = 0
        self.visual_step_index = 0
        self.visual_replay_running = False

        self.canvas = tk.Canvas(
            self.root, width=GRID_SIZE * CELL_SIZE, height=GRID_SIZE * CELL_SIZE
        )
        self.canvas.grid(row=0, column=0, rowspan=14)
        self.canvas.bind("<Button-1>", self.toggle_obstacle)
        self.canvas.bind("<Button-3>", self.toggle_dust)

        self.status = tk.StringVar(value="Disconnected")
        tk.Label(
            self.root,
            textvariable=self.status,
            justify="left",
            anchor="nw",
            width=44,
            wraplength=340,
        ).grid(row=0, column=1, sticky="nw")

        buttons = [
            ("Connect", self.connect),
            ("Power Button", self.press_power_button),
            ("Front Clear", lambda: self.send("frontPathClear")),
            ("Front Obstacle", lambda: self.send("frontObstacleDetected")),
            ("Left Clear", lambda: self.send(
                "sideSensorUpdated", leftBlocked=False, rightBlocked=True)),
            ("Right Clear", lambda: self.send(
                "sideSensorUpdated", leftBlocked=True, rightBlocked=False)),
            ("Both Blocked", lambda: self.send(
                "sideSensorUpdated", leftBlocked=True, rightBlocked=True)),
            ("Dust Detected", lambda: self.send("dustDetected")),
            ("Timer Expired", lambda: self.send("powerUpTimerExpired")),
            ("Motion Done", self.complete_current_motion),
            ("Load 30 Suite", self.load_visual_suite),
            ("Run Visual Suite", self.start_visual_suite),
            ("Save Map", self.save_map),
            ("Load Map", self.load_map),
        ]

        for index, (label, command) in enumerate(buttons, start=1):
            tk.Button(self.root, text=label, command=command, width=18).grid(
                row=index, column=1, sticky="ew"
            )

        self.draw()

    # ------------------------------------------------------------------
    # Connection / power
    # ------------------------------------------------------------------

    def connect(self) -> None:
        try:
            self.client.connect()
        except OSError as error:
            messagebox.showerror("Connection failed", str(error))
            return

        # Pull the current controller snapshot via a no-op so we can seed the
        # event counters. Without this, any motor / sensor events that the
        # server accumulated during a previous simulator session would be
        # replayed and the robot would teleport when the user first presses
        # Power Button.
        try:
            self.snapshot = self.client.send(SYNC_OP)
        except (OSError, ConnectionError, json.JSONDecodeError) as error:
            messagebox.showerror("Connection failed", str(error))
            return

        self.sync_counters_to_snapshot()
        self.pending_motion_complete = None
        self.cancel_auto_drive()
        self.status.set(
            "Connected to controller\nPress Power Button to start auto-drive."
        )
        self.draw()

    def sync_counters_to_snapshot(self) -> None:
        """Mark every event in the current snapshot as already consumed."""

        def _len(key: str) -> int:
            events = self.snapshot.get(key, [])
            return len(events) if isinstance(events, list) else 0

        self.applied_motor_event_count = _len("motorEvents")
        self.applied_front_sensor_event_count = _len("frontSensorEvents")
        self.applied_side_sensor_event_count = _len("sideSensorEvents")
        self.applied_timer_event_count = _len("timerEvents")

    def press_power_button(self) -> None:
        self.send("pressPowerButton")

    def complete_current_motion(self) -> None:
        motion = self.pending_motion_complete or str(
            self.snapshot.get("lastMotion", "TurnLeft")
        )
        if motion not in {"Backward", "TurnLeft", "TurnRight"}:
            motion = "TurnLeft"
        self.pending_motion_complete = None
        self.send("motionCompleted", motion=motion)

    # ------------------------------------------------------------------
    # Communication helper
    # ------------------------------------------------------------------

    def send(self, op: str, **payload: object) -> dict[str, object] | None:
        try:
            self.snapshot = self.client.send(op, **payload)
        except (OSError, ConnectionError, json.JSONDecodeError) as error:
            self.stop_all_timers()
            messagebox.showerror("Controller error", str(error))
            return None

        # Process snapshot side-effects in a fixed order:
        #   1) animate any new motor events on the visual robot
        #   2) honor timer Start / Restart / Cancel events from the cleaner
        #      side. This runs in send() (not tick()) so a Start coming from a
        #      cleaner-only event such as dustDetected immediately arms the
        #      booster-expiry callback, independently of the drive loop.
        self.apply_motion_from_snapshot()
        self.handle_new_timer_events()
        self.status.set(self.format_snapshot(self.snapshot))
        self.draw()

        if self.snapshot.get("powerState") == "On" and not self.visual_replay_running:
            self.schedule_tick()
        else:
            self.cancel_tick()
            if self.snapshot.get("powerState") != "On":
                self.cancel_power_up_timer()

        return self.snapshot

    def handle_new_timer_events(self) -> None:
        """Arm or cancel the booster-expiry callback from any new timer events."""
        for event in self.consume_events(
            "timerEvents", "applied_timer_event_count"
        ):
            if event == "Cancel":
                self.cancel_power_up_timer()
            elif event.startswith("Start") or event.startswith("Restart"):
                self.cancel_power_up_timer()
                self.power_up_after_id = self.root.after(
                    POWER_UP_MS, self.fire_power_up_timer
                )

    # ------------------------------------------------------------------
    # Auto-drive loop
    # ------------------------------------------------------------------

    def schedule_tick(self, delay: int = TICK_MS) -> None:
        # NFR-011: the motor tick cadence must NOT be reset by cleaner-only
        # events. Concretely, when fire_power_up_timer() sends
        # powerUpTimerExpired (a cleaner state change), the resulting send()
        # would otherwise cancel the already-queued motor tick and push the
        # robot's next forward step out by another TICK_MS, causing a visible
        # pause whenever the booster expires. We keep the existing schedule
        # if a tick is already pending; the in-flight tick will reschedule
        # itself when it runs.
        if self.auto_drive_after_id is not None:
            return
        self.auto_drive_after_id = self.root.after(delay, self.tick)

    def cancel_tick(self) -> None:
        if self.auto_drive_after_id is not None:
            try:
                self.root.after_cancel(self.auto_drive_after_id)
            except tk.TclError:
                pass
            self.auto_drive_after_id = None

    def cancel_power_up_timer(self) -> None:
        if self.power_up_after_id is not None:
            try:
                self.root.after_cancel(self.power_up_after_id)
            except tk.TclError:
                pass
            self.power_up_after_id = None

    def stop_all_timers(self) -> None:
        """Cancel both the drive tick and the booster timer (used on power off / error)."""
        self.cancel_tick()
        self.cancel_power_up_timer()

    # Backwards-compatible alias for callers that previously cancelled
    # everything through cancel_auto_drive().
    cancel_auto_drive = stop_all_timers

    def tick(self) -> None:
        self.auto_drive_after_id = None
        if not self.snapshot or self.snapshot.get("powerState") != "On":
            return

        new_front = self.consume_events(
            "frontSensorEvents", "applied_front_sensor_event_count"
        )
        new_side = self.consume_events(
            "sideSensorEvents", "applied_side_sensor_event_count"
        )

        # 1) Ack any motion the simulator already animated but has not yet
        #    reported back. The controller only listens to MotionCompleted for
        #    Backward / TurnLeft / TurnRight.
        if self.pending_motion_complete is not None:
            motion = self.pending_motion_complete
            self.pending_motion_complete = None
            self.send("motionCompleted", motion=motion)
            return

        # 2) Side-sensor request -> read the cells to the robot's left/right.
        if "RequestSideCheck" in new_side:
            self.send_side_sensor_from_map()
            return

        # 3) Front-sensor request -> read the cell directly in front.
        if "RequestFrontCheck" in new_front:
            self.send_front_sensor_from_map()
            return

        # 4) Keep driving forward in cleaning states. NFR-011: cleaner-only
        #    events MUST NOT pause motor motion, so when the current cell has
        #    dust we notify the controller AND immediately continue with the
        #    next forward step in the same tick. The robot therefore advances
        #    one cell per tick regardless of whether dust was detected.
        cleaning = self.snapshot.get("cleaningState")
        if cleaning in ("CleaningForward", "PowerUpCleaning"):
            if self.robot in self.dust:
                self.dust.discard(self.robot)
                if self.send("dustDetected") is None:
                    return
            self.send_front_sensor_from_map()
            return

    def fire_power_up_timer(self) -> None:
        self.power_up_after_id = None
        if self.snapshot.get("powerState") != "On":
            return
        if self.snapshot.get("cleaningState") != "PowerUpCleaning":
            return
        if self.visual_replay_running:
            return
        self.send("powerUpTimerExpired")

    def consume_events(self, snapshot_key: str, counter_attr: str) -> list[str]:
        events = self.snapshot.get(snapshot_key, [])
        if not isinstance(events, list):
            return []
        applied = getattr(self, counter_attr)
        if len(events) <= applied:
            return []
        new_events = [str(item) for item in events[applied:]]
        setattr(self, counter_attr, len(events))
        return new_events

    # ------------------------------------------------------------------
    # Automatic sensor responses derived from the editable map
    # ------------------------------------------------------------------

    def send_front_sensor_from_map(self) -> None:
        if self.is_cell_blocked(self.front_cell()):
            self.send("frontObstacleDetected")
        else:
            self.send("frontPathClear")

    def send_side_sensor_from_map(self) -> None:
        left_blocked = self.is_cell_blocked(self.left_cell())
        right_blocked = self.is_cell_blocked(self.right_cell())
        self.send(
            "sideSensorUpdated",
            leftBlocked=left_blocked,
            rightBlocked=right_blocked,
        )

    def is_cell_blocked(self, cell: tuple[int, int]) -> bool:
        if not self.is_inside_grid(cell):
            return True
        return cell in self.obstacles

    def is_inside_grid(self, cell: tuple[int, int]) -> bool:
        return 0 <= cell[0] < GRID_SIZE and 0 <= cell[1] < GRID_SIZE

    def front_cell(self) -> tuple[int, int]:
        return (self.robot[0] + self.heading[0], self.robot[1] + self.heading[1])

    def left_cell(self) -> tuple[int, int]:
        left = LEFT_TURN[self.heading]
        return (self.robot[0] + left[0], self.robot[1] + left[1])

    def right_cell(self) -> tuple[int, int]:
        right = RIGHT_TURN[self.heading]
        return (self.robot[0] + right[0], self.robot[1] + right[1])

    # ------------------------------------------------------------------
    # Apply controller motor events to the visual robot
    # ------------------------------------------------------------------

    def apply_motion_from_snapshot(self) -> None:
        new_events = self.consume_events(
            "motorEvents", "applied_motor_event_count"
        )
        for motion in new_events:
            self.apply_motion(motion)

    def apply_motion(self, motion: str) -> None:
        if motion == "TurnLeft":
            self.heading = LEFT_TURN[self.heading]
            self.pending_motion_complete = "TurnLeft"
        elif motion == "TurnRight":
            self.heading = RIGHT_TURN[self.heading]
            self.pending_motion_complete = "TurnRight"
        elif motion == "Forward":
            self.move_robot(self.heading)
        elif motion == "Backward":
            self.move_robot((-self.heading[0], -self.heading[1]))
            self.pending_motion_complete = "Backward"

    def move_robot(self, delta: tuple[int, int]) -> None:
        next_cell = (self.robot[0] + delta[0], self.robot[1] + delta[1])
        if not self.is_inside_grid(next_cell):
            return
        if next_cell in self.obstacles:
            return
        self.robot = next_cell

    # ------------------------------------------------------------------
    # Status / drawing
    # ------------------------------------------------------------------

    def format_snapshot(self, snapshot: dict[str, object]) -> str:
        if not snapshot:
            return "No controller state"

        lines = [
            f"Power: {snapshot.get('powerState', '-')}",
            f"State: {snapshot.get('cleaningState', '-')}",
            f"Motion: {snapshot.get('lastMotion', '-')}",
            f"Cleaner: {snapshot.get('lastCleaner', '-')}",
        ]

        front_cell = self.front_cell()
        lines.append("")
        lines.append(f"Robot: {self.robot}, Front: {front_cell}")
        lines.append(f"Heading: {HEADING_NAMES.get(self.heading, '?')}")
        lines.append(
            f"Front blocked: {self.is_cell_blocked(front_cell)}"
        )
        lines.append(f"Front dust: {front_cell in self.dust}")
        if snapshot.get("cleaningState") == "PowerUpCleaning":
            lines.append("** BOOSTER UP CLEANING **")

        return "\n".join(lines)

    def toggle_obstacle(self, event: tk.Event) -> None:
        cell = (event.x // CELL_SIZE, event.y // CELL_SIZE)
        if cell in self.obstacles:
            self.obstacles.remove(cell)
        else:
            self.obstacles.add(cell)
            self.dust.discard(cell)
        self.draw()

    def toggle_dust(self, event: tk.Event) -> None:
        cell = (event.x // CELL_SIZE, event.y // CELL_SIZE)
        if cell in self.dust:
            self.dust.remove(cell)
        else:
            self.dust.add(cell)
            self.obstacles.discard(cell)
        self.draw()

    def draw(self) -> None:
        self.canvas.delete("all")
        for x in range(GRID_SIZE):
            for y in range(GRID_SIZE):
                left = x * CELL_SIZE
                top = y * CELL_SIZE
                fill = "white"
                if (x, y) in self.obstacles:
                    fill = "gray30"
                elif (x, y) in self.dust:
                    fill = "gold"
                self.canvas.create_rectangle(
                    left, top, left + CELL_SIZE, top + CELL_SIZE, fill=fill
                )

        rx, ry = self.robot
        robot_color = "dodgerblue"
        if self.snapshot.get("cleaningState") == "PowerUpCleaning":
            robot_color = "orangered"
        elif self.snapshot.get("cleaningState") == "AvoidingObstacle":
            robot_color = "purple"
        elif self.snapshot.get("powerState") != "On":
            robot_color = "gray60"

        self.canvas.create_oval(
            rx * CELL_SIZE + 4,
            ry * CELL_SIZE + 4,
            (rx + 1) * CELL_SIZE - 4,
            (ry + 1) * CELL_SIZE - 4,
            fill=robot_color,
        )
        center_x = rx * CELL_SIZE + CELL_SIZE // 2
        center_y = ry * CELL_SIZE + CELL_SIZE // 2
        arrow_x = center_x + self.heading[0] * (CELL_SIZE // 2 - 4)
        arrow_y = center_y + self.heading[1] * (CELL_SIZE // 2 - 4)
        self.canvas.create_line(
            center_x,
            center_y,
            arrow_x,
            arrow_y,
            arrow=tk.LAST,
            width=3,
            fill="white",
        )

    # ------------------------------------------------------------------
    # Map persistence
    # ------------------------------------------------------------------

    def save_map(self) -> None:
        path = filedialog.asksaveasfilename(
            defaultextension=".json", filetypes=[("JSON", "*.json")]
        )
        if not path:
            return
        data = {
            "size": GRID_SIZE,
            "robot": list(self.robot),
            "heading": list(self.heading),
            "obstacles": [list(cell) for cell in sorted(self.obstacles)],
            "dust": [list(cell) for cell in sorted(self.dust)],
        }
        Path(path).write_text(json.dumps(data, indent=2), encoding="utf-8")

    def load_map(self) -> None:
        path = filedialog.askopenfilename(filetypes=[("JSON", "*.json")])
        if not path:
            return
        data = json.loads(Path(path).read_text(encoding="utf-8"))
        self.apply_map_data(data)

    def apply_map_data(self, data: dict[str, object]) -> None:
        self.robot = tuple(data.get("robot", self.robot))  # type: ignore[assignment]
        heading = tuple(data.get("heading", (0, -1)))  # type: ignore[arg-type]
        self.heading = heading if heading in HEADING_NAMES else (0, -1)
        self.obstacles = {tuple(item) for item in data.get("obstacles", [])}
        self.dust = {tuple(item) for item in data.get("dust", [])}
        self.pending_motion_complete = None
        self.draw()

    # ------------------------------------------------------------------
    # Visual system-test replay
    # ------------------------------------------------------------------

    def load_visual_suite(self) -> None:
        default_path = Path("system_tests/suites/rvc_30_system_tests.json")
        if default_path.exists():
            path = str(default_path)
        else:
            selected = filedialog.askopenfilename(filetypes=[("JSON", "*.json")])
            if not selected:
                return
            path = selected

        try:
            self.visual_suite = json.loads(Path(path).read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as error:
            messagebox.showerror("Suite load failed", str(error))
            return

        maps = self.visual_suite.get("maps", {})
        cases = self.visual_suite.get("cases", [])
        self.status.set(
            f"Loaded visual suite\nMaps: {len(maps)}\nCases: {len(cases)}"
        )

    def start_visual_suite(self) -> None:
        if self.visual_suite is None:
            self.load_visual_suite()
        if self.visual_suite is None:
            return

        self.connect()
        self.visual_replay_running = True
        self.stop_all_timers()
        self.visual_case_index = 0
        self.visual_step_index = 0
        self.root.after(0, self.start_next_visual_case)

    def start_next_visual_case(self) -> None:
        if self.visual_suite is None:
            return
        cases = self.visual_suite.get("cases", [])
        if not isinstance(cases, list):
            messagebox.showerror("Suite error", "Suite cases must be a list")
            self.visual_replay_running = False
            return
        if self.visual_case_index >= len(cases):
            self.visual_replay_running = False
            self.status.set(f"Visual suite complete\nPassed {len(cases)} cases")
            return

        case = cases[self.visual_case_index]
        if not isinstance(case, dict):
            self.visual_replay_running = False
            messagebox.showerror("Suite error", "Case must be an object")
            return

        if not self.prepare_visual_case(case):
            self.visual_replay_running = False
            return

        self.visual_step_index = 0
        self.status.set(f"Visual case {self.visual_case_index + 1}: {case['name']}")
        self.root.after(TICK_MS, self.run_next_visual_step)

    def prepare_visual_case(self, case: dict[str, object]) -> bool:
        maps = self.visual_suite.get("maps", {}) if self.visual_suite else {}
        if not isinstance(maps, dict):
            messagebox.showerror("Suite error", "Suite maps must be an object")
            return False

        map_name = str(case.get("map", ""))
        map_data = maps.get(map_name)
        if not isinstance(map_data, dict):
            messagebox.showerror("Suite error", f"Missing map: {map_name}")
            return False

        self.stop_all_timers()
        self.pending_motion_complete = None

        # Reset controller state between visual cases, then sync event counters
        # so events from previous cases are not replayed on the new map.
        if self.snapshot.get("powerState") == "On":
            self.send("pressPowerButton")
        try:
            self.snapshot = self.client.send(SYNC_OP)
        except (OSError, ConnectionError, json.JSONDecodeError) as error:
            messagebox.showerror("Controller error", str(error))
            return False
        self.sync_counters_to_snapshot()
        self.apply_map_data(map_data)
        return True

    def run_next_visual_step(self) -> None:
        if not self.visual_replay_running or self.visual_suite is None:
            return

        cases = self.visual_suite.get("cases", [])
        case = cases[self.visual_case_index]
        steps = case.get("steps", [])
        if not isinstance(steps, list):
            self.fail_visual_case("steps must be a list")
            return

        if self.visual_step_index >= len(steps):
            expected = case.get("expected", {})
            if not isinstance(expected, dict):
                self.fail_visual_case("expected must be an object")
                return
            if not self.snapshot_matches(expected):
                self.fail_visual_case(
                    f"final expected {expected}, got {self.snapshot}"
                )
                return
            self.visual_case_index += 1
            self.root.after(TICK_MS, self.start_next_visual_case)
            return

        step = dict(steps[self.visual_step_index])
        self.visual_step_index += 1
        op = step.pop("op")
        expected = step.pop("expect", None)
        delay = int(step.pop("_visualDelayMs", 0))
        if op == "powerUpTimerExpired" and delay == 0:
            delay = POWER_UP_MS

        if delay > 0:
            case_name = str(case.get("name", "unknown"))
            self.status.set(
                f"Visual case {self.visual_case_index + 1}: {case_name}\n"
                f"Waiting {delay // 1000}s before {op}\n\n"
                f"{self.format_snapshot(self.snapshot)}"
            )
            self.root.after(
                delay,
                lambda: self.execute_visual_step(str(op), step, expected, len(steps)),
            )
            return

        self.execute_visual_step(str(op), step, expected, len(steps))

    def execute_visual_step(
        self,
        op: str,
        step: dict[str, object],
        expected: object,
        total_steps: int,
    ) -> None:
        if not self.visual_replay_running or self.visual_suite is None:
            return

        case = self.visual_suite.get("cases", [])[self.visual_case_index]
        snapshot = self.send_visual_step(str(op), step)
        if snapshot is None:
            self.visual_replay_running = False
            return
        if isinstance(expected, dict) and not self.snapshot_matches(expected):
            self.fail_visual_case(
                f"step {self.visual_step_index} expected {expected}, got {snapshot}"
            )
            return

        case_name = str(case.get("name", "unknown"))
        self.status.set(
            f"Visual case {self.visual_case_index + 1}: {case_name}\n"
            f"Step {self.visual_step_index}/{total_steps}\n\n"
            f"{self.format_snapshot(self.snapshot)}"
        )
        self.root.after(TICK_MS, self.run_next_visual_step)

    def send_visual_step(self, op: str, payload: dict[str, object]) -> dict[str, object] | None:
        self.apply_visual_map_updates(payload)

        if op in {"frontPathClear", "frontObstacleDetected"}:
            return self.send_visual_front_sensor_from_map()
        if op == "sideSensorUpdated":
            return self.send_visual_side_sensor_from_map()
        if op == "dustDetected":
            return self.send_visual_dust_sensor_from_map()
        if op == "motionCompleted":
            motion = str(payload.get("motion", self.pending_motion_complete or "TurnLeft"))
            return self.send("motionCompleted", motion=motion)
        return self.send(op, **payload)

    def apply_visual_map_updates(self, payload: dict[str, object]) -> None:
        for cell in payload.pop("_visualRemoveObstacles", []):
            self.obstacles.discard(tuple(cell))  # type: ignore[arg-type]
        for cell in payload.pop("_visualAddObstacles", []):
            obstacle = tuple(cell)  # type: ignore[arg-type]
            self.obstacles.add(obstacle)
            self.dust.discard(obstacle)
        for cell in payload.pop("_visualAddDust", []):
            dust = tuple(cell)  # type: ignore[arg-type]
            self.dust.add(dust)
            self.obstacles.discard(dust)
        for cell in payload.pop("_visualRemoveDust", []):
            self.dust.discard(tuple(cell))  # type: ignore[arg-type]
        self.draw()

    def send_visual_front_sensor_from_map(self) -> dict[str, object] | None:
        if self.is_cell_blocked(self.front_cell()):
            return self.send("frontObstacleDetected")
        return self.send("frontPathClear")

    def send_visual_side_sensor_from_map(self) -> dict[str, object] | None:
        left_blocked = self.is_cell_blocked(self.left_cell())
        right_blocked = self.is_cell_blocked(self.right_cell())
        return self.send(
            "sideSensorUpdated",
            leftBlocked=left_blocked,
            rightBlocked=right_blocked,
        )

    def send_visual_dust_sensor_from_map(self) -> dict[str, object] | None:
        if self.robot not in self.dust:
            self.fail_visual_case(
                f"DustDetected requested but robot is not on dust at {self.robot}"
            )
            return None
        return self.send("dustDetected")

    def snapshot_matches(self, expected: dict[str, object]) -> bool:
        return all(self.snapshot.get(key) == value for key, value in expected.items())

    def fail_visual_case(self, message: str) -> None:
        self.visual_replay_running = False
        case_number = self.visual_case_index + 1
        self.status.set(f"Visual suite FAILED at case {case_number}\n{message}")
        messagebox.showerror("Visual suite failed", f"Case {case_number}: {message}")

    def run(self) -> None:
        try:
            self.root.mainloop()
        finally:
            self.cancel_auto_drive()
            self.client.close()


if __name__ == "__main__":
    SimulatorUi().run()
