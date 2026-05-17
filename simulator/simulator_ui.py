"""Tkinter simulator for manually building RVC maps and driving the controller."""

from __future__ import annotations

import json
import tkinter as tk
from pathlib import Path
from tkinter import filedialog, messagebox

from rvc_client import RvcClient


CELL_SIZE = 32
GRID_SIZE = 12
LEFT_TURN = {
    (0, -1): (-1, 0),
    (-1, 0): (0, 1),
    (0, 1): (1, 0),
    (1, 0): (0, -1),
}
RIGHT_TURN = {value: key for key, value in LEFT_TURN.items()}


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
        self.applied_motor_event_count = 0

        self.canvas = tk.Canvas(
            self.root, width=GRID_SIZE * CELL_SIZE, height=GRID_SIZE * CELL_SIZE
        )
        self.canvas.grid(row=0, column=0, rowspan=12)
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
        ).grid(
            row=0, column=1, sticky="nw"
        )

        buttons = [
            ("Connect", self.connect),
            ("Power Button", self.press_power_button),
            ("Front From Map", self.apply_front_sensor_from_map),
            ("Front Clear", lambda: self.send("frontPathClear")),
            ("Front Obstacle", lambda: self.send("frontObstacleDetected")),
            ("Left Clear", lambda: self.send("sideSensorUpdated", leftBlocked=False, rightBlocked=True)),
            ("Right Clear", lambda: self.send("sideSensorUpdated", leftBlocked=True, rightBlocked=False)),
            ("Both Blocked", lambda: self.send("sideSensorUpdated", leftBlocked=True, rightBlocked=True)),
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

    def connect(self) -> None:
        try:
            self.client.connect()
            self.status.set("Connected to controller\nPress Power Button to start.")
        except OSError as error:
            messagebox.showerror("Connection failed", str(error))

    def press_power_button(self) -> None:
        snapshot = self.send("pressPowerButton")
        if (
            snapshot is not None
            and snapshot.get("powerState") == "On"
            and snapshot.get("cleaningState") == "CheckingFront"
        ):
            self.apply_front_sensor_from_map()

    def send(self, op: str, **payload: object) -> dict[str, object] | None:
        try:
            self.snapshot = self.client.send(op, **payload)
            self.apply_motion_from_snapshot()
            self.status.set(self.format_snapshot(self.snapshot))
            self.draw()
            return self.snapshot
        except (OSError, ConnectionError, json.JSONDecodeError) as error:
            messagebox.showerror("Controller error", str(error))
            return None

    def apply_front_sensor_from_map(self) -> None:
        front_cell = self.front_cell()
        if front_cell in self.obstacles:
            self.send("frontObstacleDetected")
            return
        self.send("frontPathClear")

    def complete_current_motion(self) -> None:
        motion = str(self.snapshot.get("lastMotion", "TurnLeft"))
        if motion not in {"Backward", "TurnLeft", "TurnRight"}:
            motion = "TurnLeft"
        self.send("motionCompleted", motion=motion)

    def front_cell(self) -> tuple[int, int]:
        return (self.robot[0] + self.heading[0], self.robot[1] + self.heading[1])

    def apply_motion_from_snapshot(self) -> None:
        motor_events = self.snapshot.get("motorEvents", [])
        if not isinstance(motor_events, list):
            return
        if len(motor_events) <= self.applied_motor_event_count:
            return

        new_events = motor_events[self.applied_motor_event_count :]
        self.applied_motor_event_count = len(motor_events)

        for motion in new_events:
            self.apply_motion(str(motion))

    def apply_motion(self, motion: str) -> None:
        if motion == "TurnLeft":
            self.heading = LEFT_TURN[self.heading]
        elif motion == "TurnRight":
            self.heading = RIGHT_TURN[self.heading]
        elif motion == "Forward":
            self.move_robot(self.heading)
        elif motion == "Backward":
            self.move_robot((-self.heading[0], -self.heading[1]))

    def move_robot(self, delta: tuple[int, int]) -> None:
        next_cell = (self.robot[0] + delta[0], self.robot[1] + delta[1])
        if not self.is_inside_grid(next_cell):
            return
        if next_cell in self.obstacles:
            return
        self.robot = next_cell
        self.dust.discard(next_cell)

    def is_inside_grid(self, cell: tuple[int, int]) -> bool:
        return 0 <= cell[0] < GRID_SIZE and 0 <= cell[1] < GRID_SIZE

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
        lines.append(f"Heading: {self.heading_name()}")
        lines.append(f"Front blocked: {front_cell in self.obstacles}")
        lines.append(f"Front dust: {front_cell in self.dust}")

        return "\n".join(lines)

    def heading_name(self) -> str:
        names = {
            (0, -1): "North",
            (1, 0): "East",
            (0, 1): "South",
            (-1, 0): "West",
        }
        return names.get(self.heading, "Unknown")

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
        self.canvas.create_oval(
            rx * CELL_SIZE + 4,
            ry * CELL_SIZE + 4,
            (rx + 1) * CELL_SIZE - 4,
            (ry + 1) * CELL_SIZE - 4,
            fill="dodgerblue",
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
        self.applied_motor_event_count = 0
        self.draw()

    def run(self) -> None:
        self.root.mainloop()
        self.client.close()


if __name__ == "__main__":
    SimulatorUi().run()
