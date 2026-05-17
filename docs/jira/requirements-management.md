# Jira Requirements Management

## Purpose

이 문서는 RVC Controller 학생 프로젝트에서 Jira를 요구사항 추적과 상태 관리 용도로만 사용하기 위한 가벼운 agile 운영 규칙을 정의한다. 설계 문서, 코드, 테스트, CI 결과는 GitHub에서 관리하고, Jira는 현재 어떤 요구사항이 진행 중인지와 어떤 PR이 연결되어 있는지 추적하는 역할만 맡는다.

## Jira Project Setup

| Item | Value |
| --- | --- |
| Project name | RVC Controller |
| Project key | RVC |
| Board | Kanban |
| Sprint | 사용하지 않음. 필요 시 1주 단위만 사용 |
| Primary use | Requirement tracking and status management |

학생 프로젝트에서는 Scrum보다 Kanban이 단순하다. Backlog를 복잡하게 운영하지 않고, 요구사항을 `To Do`, `In Progress`, `Review`, `Done`으로 이동시키며 진행 상태를 확인한다.

## Issue Types

사용하는 이슈 타입은 3개로 제한한다.

| Issue Type | Purpose |
| --- | --- |
| Requirement | 기능 요구사항 또는 비기능 요구사항 |
| Task | Requirement를 구현, 테스트, 문서화하기 위한 작업 |
| Bug | 테스트 실패, CI 실패, 요구사항 불일치, 결함 |

Jira 프로젝트에서 custom issue type 생성이 어렵다면 `Requirement` 대신 Jira 기본 `Story`를 사용하고 label에 `requirement`를 붙인다. 그래도 프로젝트 운영 규칙에서는 이 항목을 Requirement로 취급한다.

## Workflow

최소 상태만 사용한다.

```text
To Do -> In Progress -> Review -> Done
```

| Status | Meaning |
| --- | --- |
| To Do | 아직 시작하지 않은 요구사항 또는 작업 |
| In Progress | 분석, 설계, 구현, 테스트 진행 중 |
| Review | PR 리뷰, 요구사항 확인, CI 결과 확인 중 |
| Done | 구현, 테스트, 리뷰 완료 |

## Requirement Naming Rules

Jira Summary는 다음 형식을 사용한다.

```text
[FR] RVC-REQ-001 Power button start and stop
[NFR] RVC-NFR-001 CI must run build and tests on PR
```

ID 체계는 다음과 같다.

```text
RVC-REQ-001, RVC-REQ-002, ...
RVC-NFR-001, RVC-NFR-002, ...
RVC-BUG-001, RVC-BUG-002, ...
```

초기 `requirements.md`의 FR 20개를 Jira에 그대로 20개 올리지 않고, 기능 단위 Requirement로 묶어서 관리한다.

| Jira Requirement | Covered Requirements |
| --- | --- |
| RVC-REQ-001 Power button start and stop | FR-001, FR-002, FR-003 |
| RVC-REQ-002 Basic forward cleaning | FR-004, FR-005, FR-018, FR-019, FR-020 |
| RVC-REQ-003 Obstacle avoidance | FR-006, FR-007, FR-008, FR-009, FR-010, FR-011, FR-012, FR-013, FR-014 |
| RVC-REQ-004 Dust detection and cleaner power-up | FR-015, FR-016, FR-017 |
| RVC-NFR-001 CI and quality checks | NFR-004, NFR-005, NFR-006 |

## Task Rules

Requirement 하나가 바로 구현하기 크면 Task로 나눈다.

```text
[TASK] Implement RVC-REQ-003 left/right obstacle decision
[TASK] Add GTest for RVC-REQ-003 obstacle avoidance
[TASK] Update SSD for RVC-REQ-004 dust power-up
```

Task는 반드시 관련 Requirement에 연결한다.

```text
Task implements Requirement
```

## Bug Rules

Bug는 요구사항 불일치나 CI/테스트 실패를 추적할 때만 만든다.

```text
[BUG] RVC-REQ-003 moves forward when front obstacle exists
[BUG] CI fails in Cppcheck stage
[BUG] Coverage report upload fails
```

Bug는 관련 Requirement 또는 Task에 연결한다.

```text
Bug blocks Requirement
Bug relates to Requirement
```

## GitHub Branch Naming

브랜치 이름에는 Jira 이슈 키를 포함한다.

```text
feature/RVC-REQ-001-power-button-control
feature/RVC-REQ-002-basic-cleaning
feature/RVC-REQ-003-obstacle-avoidance
feature/RVC-REQ-004-dust-power-up
fix/RVC-BUG-001-cppcheck-failure
docs/RVC-REQ-004-update-ssd
```

요구사항 20개가 있어도 브랜치를 20개 만들 필요는 없다. 리뷰하기 좋은 기능 묶음 단위로 브랜치를 만든다.

## Commit and PR Naming

Commit과 PR 제목에도 Jira 키를 넣는다.

```text
RVC-REQ-001 Implement power button transition
RVC-REQ-003 Add obstacle avoidance tests
RVC-BUG-001 Fix Cppcheck false positive
```

PR 본문에는 다음 정보를 포함한다.

```text
Jira: RVC-REQ-003
Requirement: Obstacle avoidance
Test: GTest passed
CI: GitHub Actions passed
```

## Lightweight Agile Flow

1. Inception 단계에서 Requirement를 Jira에 등록한다.
2. Requirement를 기능 단위로 묶어 우선순위를 정한다.
3. 구현이 필요한 Requirement마다 Task를 만든다.
4. Task 또는 Requirement 기준으로 GitHub feature branch를 만든다.
5. 구현, 테스트, 문서 수정을 진행한다.
6. PR을 만들고 CI와 리뷰를 확인한다.
7. PR이 merge되면 Jira 이슈를 `Done`으로 이동한다.

## Initial Work Order

1. `RVC-REQ-001 Power button start and stop`
2. `RVC-REQ-002 Basic forward cleaning`
3. `RVC-REQ-003 Obstacle avoidance`
4. `RVC-REQ-004 Dust detection and cleaner power-up`
5. `RVC-NFR-001 CI and quality checks`

## Operating Principles

- Jira는 상태와 추적만 담당한다.
- GitHub는 코드, 리뷰, CI 결과를 담당한다.
- Requirement는 너무 잘게 쪼개지 말고 기능 단위로 관리한다.
- Task는 실제 구현, 테스트, 문서 작업 단위로 나눈다.
- Bug는 실패나 불일치가 생겼을 때만 만든다.
