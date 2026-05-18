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


def assert_expected(
    case_name: str,
    snapshot: dict[str, Any],
    expected: dict[str, Any],
    context: str = "final",
) -> None:
    for key, expected_value in expected.items():
        actual = snapshot.get(key)
        if actual != expected_value:
            raise AssertionError(
                f"{case_name} {context} expected {key}={expected_value}, got {actual}"
            )


def validate_map_reference(case: dict[str, Any], suite_maps: dict[str, Any] | None) -> None:
    map_ref = case.get("map")
    if not map_ref:
        return

    if suite_maps is not None:
        if map_ref not in suite_maps:
            raise KeyError(f"{case['name']} references unknown map '{map_ref}'")
        return

    base_dir = Path(case.get("_base_dir", Path(__file__).resolve().parent))
    map_path = (base_dir / map_ref).resolve()
    if not map_path.exists():
        raise FileNotFoundError(f"{case['name']} references missing map {map_path}")


def run_case(
    client: RvcClient,
    case: dict[str, Any],
    suite_maps: dict[str, Any] | None = None,
) -> dict[str, Any]:
    print(f"Running {case['name']}")
    validate_map_reference(case, suite_maps)
    last_snapshot: dict[str, Any] = {}

    for index, step in enumerate(case["steps"], start=1):
        payload = dict(step)
        op = payload.pop("op")
        expected = payload.pop("expect", None)
        last_snapshot = client.send(op, **payload)
        if expected is not None:
            assert_expected(case["name"], last_snapshot, expected, f"step {index}")

    assert_expected(case["name"], last_snapshot, case["expected"])
    return last_snapshot


def load_cases_from_directory(cases_dir: str) -> tuple[list[dict[str, Any]], dict[str, Any] | None]:
    case_paths = sorted(Path(cases_dir).glob("*.json"))
    if not case_paths:
        raise FileNotFoundError(f"No system test cases found in {cases_dir}")
    cases = []
    for path in case_paths:
        case = json.loads(path.read_text(encoding="utf-8"))
        case["_base_dir"] = str(path.parent)
        cases.append(case)
    return cases, None


def load_cases_from_suite(suite_path: str) -> tuple[list[dict[str, Any]], dict[str, Any]]:
    suite = json.loads(Path(suite_path).read_text(encoding="utf-8"))
    maps = suite.get("maps", {})
    cases = suite.get("cases", [])
    if len(maps) != 30 or len(cases) != 30:
        raise AssertionError(
            f"{suite_path} must contain exactly 30 maps and 30 cases "
            f"(got {len(maps)} maps, {len(cases)} cases)"
        )
    return cases, maps


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=5050)
    parser.add_argument("--cases", default="system_tests/cases")
    parser.add_argument("--suite", help="JSON suite containing maps and cases")
    args = parser.parse_args()

    if args.suite:
        cases, suite_maps = load_cases_from_suite(args.suite)
    else:
        cases, suite_maps = load_cases_from_directory(args.cases)

    with RvcClient(args.host, args.port) as client:
        last_snapshot: dict[str, Any] = {}
        for case in cases:
            last_snapshot = power_off_if_needed(client, last_snapshot)
            last_snapshot = run_case(client, case, suite_maps)

    print(f"Passed {len(cases)} system test cases")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
