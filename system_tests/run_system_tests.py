"""System test runner for the RVC controller TCP bridge.

Start the C++ app first, for example:
  build/apps/Debug/rvc_controller_app.exe 5050
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "simulator"))
from rvc_client import RvcClient  # noqa: E402


def power_off_if_needed(client: RvcClient, snapshot: dict[str, Any]) -> dict[str, Any]:
    if snapshot.get("powerState") == "On":
        return client.send("pressPowerButton")
    return snapshot


def run_case(client: RvcClient, case_path: Path) -> dict[str, Any]:
    case = json.loads(case_path.read_text(encoding="utf-8"))
    print(f"Running {case['name']}")
    last_snapshot: dict[str, Any] = {}

    for step in case["steps"]:
        payload = dict(step)
        op = payload.pop("op")
        last_snapshot = client.send(op, **payload)

    for key, expected in case["expected"].items():
        actual = last_snapshot.get(key)
        if actual != expected:
            raise AssertionError(
                f"{case['name']} expected {key}={expected}, got {actual}"
            )
    return last_snapshot


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=5050)
    parser.add_argument("--cases", default="system_tests/cases")
    args = parser.parse_args()

    case_paths = sorted(Path(args.cases).glob("*.json"))
    if not case_paths:
        raise FileNotFoundError(f"No system test cases found in {args.cases}")

    with RvcClient(args.host, args.port) as client:
        last_snapshot: dict[str, Any] = {}
        for case_path in case_paths:
            last_snapshot = power_off_if_needed(client, last_snapshot)
            last_snapshot = run_case(client, case_path)

    print(f"Passed {len(case_paths)} system test cases")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
