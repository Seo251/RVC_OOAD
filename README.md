# RVC_OOAD

RVC controller software developed in C++ with an OOAD/UP process.

## CI/CD

The repository uses GitHub Actions for continuous integration. The workflow is
defined in `.github/workflows/ci.yml` and is designed to run on every pull
request to `main`, every push to `main`, and manual dispatch.

The CI pipeline performs:

- CMake configure/build with coverage flags enabled.
- GTest execution through CTest.
- Static analysis with Cppcheck.
- Static analysis with clang-tidy.
- Static analysis and quality reporting with SonarCloud.
- gcov/lcov coverage capture and HTML report upload as a GitHub Actions artifact.
- Discord notification for CI result visibility.

The current repository contains a minimal CMake/GTest controller scaffold, so
the workflow can build the controller library and run unit tests.

## Required GitHub Configuration

Configure these repository secrets:

- `DISCORD_WEBHOOK_URL`: Discord webhook URL for CI notifications.
- `SONAR_TOKEN`: SonarCloud token used by the scanner.

Configure these repository variables:

- `SONAR_ORGANIZATION`: SonarCloud organization key.
- `SONAR_PROJECT_KEY`: SonarCloud project key.

SonarCloud reads the repository analysis settings from
`sonar-project.properties`. The GitHub Actions workflow passes only the
organization and project key because those values differ per SonarCloud
workspace.

## SonarCloud Setup

1. Create or import the `Seo251/RVC_OOAD` repository in SonarCloud.
2. Create a SonarCloud token and save it as the GitHub secret `SONAR_TOKEN`.
3. Save the SonarCloud organization key as the GitHub variable
   `SONAR_ORGANIZATION`.
4. Save the SonarCloud project key as the GitHub variable `SONAR_PROJECT_KEY`.
5. Enable the SonarCloud quality gate as a required branch protection check
   after the first successful analysis.

The CI job generates `build/compile_commands.json` for C++ analysis and
`coverage/sonarqube.xml` for coverage import.

## Running the Controller and Simulator

Build the C++ controller app:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build
```

Start the TCP bridge:

```powershell
.\build\Debug\rvc_controller_app.exe 5050
```

In another terminal, start the Python UI simulator:

```powershell
python simulator\simulator_ui.py
```

The simulator uses left-click to toggle obstacles and right-click to toggle
dust. It communicates with the C++ controller through line-delimited JSON over
TCP.

### Autonomous behavior

After `Power Button` is pressed the simulator drives the controller
automatically. On every controller response the UI inspects the snapshot, then
answers requests that the C++ controller raises (front/side sensor checks,
power-up timer, motion completion) by reading the editable grid map. The result
is a real-time animation: the robot moves forward through clean cells, switches
to **PowerUpCleaning** (the robot turns orange-red and the status shows
"BOOSTER UP CLEANING") when it enters a dust cell, and enters
**AvoidingObstacle** (turns purple) when its front cell is blocked, choosing
to turn left/right or back up based on the surrounding cells.

Timing:

- One simulation tick is one second, so a Forward / Backward / TurnLeft /
  TurnRight motion advances by one cell or one heading-step per second.
- PowerUpCleaning lasts three seconds (three ticks). During those three seconds
  the robot keeps moving forward; only `frontObstacleDetected` interrupts the
  booster and triggers obstacle avoidance.
- If a new dust cell is entered while PowerUpCleaning is already active, the
  three-second timer is discarded and restarted from that moment.

You can keep editing the map while the robot is running - newly placed
obstacles and dust will affect the next sensor check.

`Connect` also synchronizes the simulator's internal event counters with the
controller's current snapshot, so restarting only the simulator UI no longer
makes the robot jump to a strange position when you press Power Button.

### Buttons

- `Connect`: connect to the C++ controller app.
- Left-click a cell: toggle an obstacle.
- Right-click a cell: toggle dust.
- `Power Button`: start (or stop) the autonomous cleaning loop.
- `Front Clear` / `Front Obstacle`: manual sensor overrides for debugging.
- `Left Clear` / `Left Blocked`: manual left-sensor overrides (CHG-001: right sensor removed).
- `Dust Detected`: manually inject a dust detection event.
- `Timer Expired`: manually fire the 3-second cleaner power-up timer.
- `Motion Done`: manually confirm the current turn/backward motion.
- `Load 30 Suite`: load `system_tests/suites/rvc_30_system_tests.json`.
- `Run Visual Suite`: replay the 30 scenario suite in the simulator UI, one
  step per second, and stop with a failure message if an expected state does
  not match.
- `Save Map` / `Load Map`: save or load obstacle/dust maps.

### Visualization

- Blue circle: cleaning forward.
- Orange-red circle: PowerUpCleaning (booster up mode).
- Purple circle: AvoidingObstacle.
- Gray circle: powered off.
- White arrow inside the circle: current heading.
- Gold cell: dust. Dark gray cell: obstacle.

Controller motion events drive the robot: `Forward` moves one cell forward,
`Backward` moves one cell backward, and `TurnLeft`/`TurnRight` rotate the arrow.

### Quick demo

```text
1. Start rvc_controller_app.exe 5050.
2. Start simulator_ui.py.
3. Click Connect.
4. Left-click cells to add a few obstacles. Right-click cells to drop dust in
   the robot's expected path.
5. Click Power Button - the robot should now drive forward by itself,
   power-up clean any dust it crosses, and turn or back up around obstacles.
6. Click Power Button again to stop the robot.
```

## Testing

Run unit tests:

```powershell
ctest --test-dir build --output-on-failure
```

### System Tests

System tests run through the same TCP bridge used by the Python simulator.
Start the C++ controller app first:

```powershell
.\build\Debug\rvc_controller_app.exe 5050
```

In another terminal, run the default system test cases:

```powershell
python system_tests\run_system_tests.py --port 5050
```

Run the 30-scenario simulator system test suite:

```powershell
python system_tests\run_system_tests.py --suite system_tests\suites\rvc_30_system_tests.json --port 5050
```

The runner prints each case name. If every final and step-level expectation
matches the controller snapshot, it ends with:

```text
Passed 30 system test cases
```

If a case fails, the runner stops and prints the case name, expected value, and
actual value.

To inspect the same 30 scenarios visually:

```powershell
.\build\Debug\rvc_controller_app.exe 5050
python simulator\simulator_ui.py
```

Then click `Connect`, `Load 30 Suite`, and `Run Visual Suite`. The simulator
applies each case map and replays the test steps at one-second intervals. If an
expected state does not match, the visual replay stops and shows the failed
case.

Unit tests target the C++ controller/core only. The Python simulator is excluded
from unit coverage.

## Jira FR Management

Jira is the source of truth for feature requests. GitHub issues and pull
requests are used to connect implementation work, code review, and CI evidence
back to the Jira FR.

Use this naming convention:

- Jira key format: `RVC-123`
- Feature branch: `feature/RVC-123-short-description`
- Bug branch: `fix/RVC-124-short-description`
- Commit title: `RVC-123 Add dust power-up behavior`
- Pull request title: `RVC-123 Add dust power-up behavior`

For every FR:

- Create or update the Jira FR first.
- Use the GitHub feature request template when a GitHub issue is useful for
  developer tracking.
- Include the Jira key and Jira link in every PR.
- Keep acceptance criteria aligned between Jira, the GitHub issue, and tests.
- Confirm GTest, Cppcheck, clang-tidy, SonarCloud, and coverage results before
  requesting review.

## Pull Request Protection

Configure `main` branch protection in GitHub repository settings:

- Require a pull request before merging.
- Require at least 3 approving reviews.
- Dismiss stale pull request approvals when new commits are pushed.
- Require status checks to pass before merging.
- Require the `Build, Test, Static Analysis, Coverage` GitHub Actions check.
- Require the SonarCloud quality gate check after SonarCloud is connected.

`CODEOWNERS` is configured with `@Seo251` as the default owner. Add more GitHub
users or teams there when the review group is finalized.