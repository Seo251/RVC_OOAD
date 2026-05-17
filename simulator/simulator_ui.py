"""Tkinter simulator for manually building RVC maps and driving the controller."""

from __future__ import annotations

import json
import tkinter as tk
from pathlib import Path
from tkinter import filedialog, messagebox

from rvc_client import RvcClient


CELL_SIZE = 32
GRID_SIZE = 12


class SimulatorUi:
    def __init__(self) -> None:
        self.root = tk.Tk()
        self.root.title("RVC Controller Simulator")
        self.client = RvcClient()
        self.obstacles: set[tuple[int, int]] = set()
        self.dust: set[tuple[int, int]] = set()
        self.robot = (GRID_SIZE // 2, GRID_SIZE // 2)
        self.snapshot: dict[str, object] = {}

        self.canvas = tk.Canvas(
            self.root, width=GRID_SIZE * CELL_SIZE, height=GRID_SIZE * CELL_SIZE
        )
        self.canvas.grid(row=0, column=0, rowspan=12)
        self.canvas.bind("<Button-1>", self.toggle_obstacle)
        self.canvas.bind("<Button-3>", self.toggle_dust)

        self.status = tk.StringVar(value="Disconnected")
        tk.Label(self.root, textvariable=self.status, justify="left").grid(
            row=0, column=1, sticky="nw"
        )

        buttons = [
            ("Connect", self.connect),
            ("Power Button", lambda: self.send("pressPowerButton")),
            ("Front Clear", lambda: self.send("frontPathClear")),
            ("Front Obstacle", lambda: self.send("frontObstacleDetected")),
            ("Left Clear", lambda: self.send("sideSensorUpdated", leftBlocked=False, rightBlocked=True)),
            ("Right Clear", lambda: self.send("sideSensorUpdated", leftBlocked=True, rightBlocked=False)),
            ("Both Blocked", lambda: self.send("sideSensorUpdated", leftBlocked=True, rightBlocked=True)),
            ("Dust Detected", lambda: self.send("dustDetected")),
            ("Timer Expired", lambda: self.send("powerUpTimerExpired")),
            ("Motion Done", lambda: self.send("motionCompleted", motion="TurnLeft")),
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
            self.status.set("Connected to controller")
        except OSError as error:
            messagebox.showerror("Connection failed", str(error))

    def send(self, op: str, **payload: object) -> None:
        try:
            self.snapshot = self.client.send(op, **payload)
            self.status.set(json.dumps(self.snapshot, indent=2))
            self.draw()
        except (OSError, ConnectionError, json.JSONDecodeError) as error:
            messagebox.showerror("Controller error", str(error))

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
        self.root.mainloop()
        self.client.close()


if __name__ == "__main__":
    SimulatorUi().run()
