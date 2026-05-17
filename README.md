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

Simulator controls:

- `Connect`: connect to the C++ controller app.
- Left-click a cell: toggle an obstacle.
- Right-click a cell: toggle dust.
- `Power Button`: send the power button event. When the controller enters
  `CheckingFront`, the simulator automatically checks the front cell in the map
  and sends either `Front Clear` or `Front Obstacle`.
- `Front From Map`: manually re-check the current front cell from the map.
- `Front Clear`: tell the controller that the front cell is clear.
- `Front Obstacle`: tell the controller that the front cell is blocked.
- `Left Clear`: send a side-sensor update where the left side is clear.
- `Right Clear`: send a side-sensor update where the right side is clear.
- `Both Blocked`: send a side-sensor update where both sides are blocked.
- `Dust Detected`: send a dust detection event.
- `Timer Expired`: simulate the 5-second cleaner power-up timer expiring.
- `Motion Done`: tell the controller that the current turn/backward motion
  completed.
- `Save Map` / `Load Map`: save or load manually edited obstacle/dust maps.

The robot is drawn as a blue circle. The white arrow inside the robot shows the
current front direction. Controller motion events update the simulator pose:
`Forward` moves one cell forward, `Backward` moves one cell backward, and
`TurnLeft`/`TurnRight` rotate the arrow.

Basic manual scenario:

```text
1. Start rvc_controller_app.exe 5050.
2. Start simulator_ui.py.
3. Click Connect.
4. Place or remove obstacles/dust on the grid.
5. Click Power Button.
6. Use Motion Done after a turn/backward command to finish the motion.
```

## Testing

Run unit tests:

```powershell
ctest --test-dir build --output-on-failure
```

Run system tests after starting the controller app:

```powershell
python system_tests\run_system_tests.py --port 5050
```

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