"""Tkinter simulator that drives the RVC controller autonomously."""

from __future__ import annotations

import json
import tkinter as tk
from pathlib import Path
from tkinter import filedialog, messagebox

from rvc_client import RvcClient


CELL_SIZE = 32
GRID_SIZE = 12

# Animation tick used between auto-drive steps. A small value keeps the robot
# moving briskly, but still leaves time for the user to watch the simulation.
TICK_MS = 350

# Demo-only acceleration for the 5-second cleaner power-up timer so the
# PowerUpCleaning visualization does not block the user for too long.
POWER_UP_MS = 1500

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
            self.status.set(
                "Connected to controller\nPress Power Button to start auto-drive."
            )
        except OSError as error:
            messagebox.showerror("Connection failed", str(error))

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
            self.cancel_auto_drive()
            messagebox.showerror("Controller error", str(error))
            return None

        self.apply_motion_from_snapshot()
        self.status.set(self.format_snapshot(self.snapshot))
        self.draw()

        if self.snapshot.get("powerState") == "On":
            self.schedule_tick()
        else:
            self.cancel_auto_drive()

        return self.snapshot

    # ------------------------------------------------------------------
    # Auto-drive loop
    # ------------------------------------------------------------------

    def schedule_tick(self, delay: int = TICK_MS) -> None:
        self.cancel_auto_drive()
        self.auto_drive_after_id = self.root.after(delay, self.tick)

    def cancel_auto_drive(self) -> None:
        if self.auto_drive_after_id is not None:
            try:
                self.root.after_cancel(self.auto_drive_after_id)
            except tk.TclError:
                pass
            self.auto_drive_after_id = None

    def tick(self) -> None:
        self.auto_drive_after_id = None
        if not self.snapshot or self.snapshot.get("powerState") != "On":
            return

        new_timer = self.consume_events(
            "timerEvents", "applied_timer_event_count"
        )
        new_front = self.consume_events(
            "frontSensorEvents", "applied_front_sensor_event_count"
        )
        new_side = self.consume_events(
            "sideSensorEvents", "applied_side_sensor_event_count"
        )

        # 1) Ack any motion the simulator animated but has not yet reported
        #    back. The controller only listens to MotionCompleted for
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

        # 4) A timer Start/Restart means we're in PowerUpCleaning. Wait for
        #    the accelerated demo duration before reporting timer expired.
        if any(e.startswith("Start") or e.startswith("Restart") for e in new_timer):
            if self.snapshot.get("cleaningState") == "PowerUpCleaning":
                self.auto_drive_after_id = self.root.after(
                    POWER_UP_MS, self.fire_power_up_timer
                )
                return

        # 5) In CleaningForward, keep driving: detect dust at the current
        #    cell first; otherwise ask the front sensor about the next cell
        #    so the controller can issue the next Forward step.
        if self.snapshot.get("cleaningState") == "CleaningForward":
            if self.robot in self.dust:
                self.dust.discard(self.robot)
                self.send("dustDetected")
                return
            self.send_front_sensor_from_map()
            return

    def fire_power_up_timer(self) -> None:
        if self.snapshot.get("powerState") != "On":
            return
        if self.snapshot.get("cleaningState") != "PowerUpCleaning":
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
            "obstacles": [list(cell) for cell in sorted(self.obstacles)],
            "dust": [list(cell) for cell in sorted(self.dust)],
        }
        Path(path).write_text(json.dumps(data, indent=2), encoding="utf-8")

    def load_map(self) -> None:
        path = filedialog.askopenfilename(filetypes=[("JSON", "*.json")])
        if not path:
            return
        data = json.loads(Path(path).read_text(encoding="utf-8"))
        self.robot = tuple(data.get("robot", self.robot))  # type: ignore[assignment]
        self.obstacles = {tuple(item) for item in data.get("obstacles", [])}
        self.dust = {tuple(item) for item in data.get("dust", [])}
        self.draw()

    def run(self) -> None:
        try:
            self.root.mainloop()
        finally:
            self.cancel_auto_drive()
            self.client.close()


if __name__ == "__main__":
    SimulatorUi().run()
