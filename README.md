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

The current repository contains CI/CD configuration only. Add `CMakeLists.txt`,
source files, and GTest targets before expecting the workflow to pass.

## Required GitHub Configuration

Configure these repository secrets:

- `DISCORD_WEBHOOK_URL`: Discord webhook URL for CI notifications.
- `SONAR_TOKEN`: SonarCloud token used by the scanner.

Configure these repository variables:

- `SONAR_ORGANIZATION`: SonarCloud organization key.
- `SONAR_PROJECT_KEY`: SonarCloud project key.

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