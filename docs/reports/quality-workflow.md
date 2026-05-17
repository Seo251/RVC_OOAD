# Quality Workflow

## Local Verification

Use these commands before opening or updating a pull request:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

If the controller app is running, execute the Python system tests:

```powershell
python system_tests\run_system_tests.py --port 5050
```

## Static Analysis

GitHub Actions runs:

- Cppcheck for production source filtering
- clang-tidy for C++ source and tests
- SonarCloud for aggregated quality reporting

On a local machine, run Cppcheck and clang-tidy only if the tools are installed.
This repository's CI remains the authoritative automated quality gate.

## Coverage

Coverage is collected from C++ controller/core source under `src`. Simulator,
system tests, apps, and unit test files are excluded from line coverage.

Target:

```text
Line coverage >= 90% for controller/core source
```

## SonarCloud

SonarCloud reads `sonar-project.properties` and the GitHub Actions generated
coverage report:

```text
coverage/sonarqube.xml
```

Review the SonarCloud dashboard after each CI run for:

- Bugs
- Code smells
- Coverage
- Duplications
- Quality gate status
