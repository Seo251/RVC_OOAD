# RVC Controller Inception Requirements

## Scope

이 문서는 UP(Unified Process)의 Inception 단계에서 `rvc_controller`의 자동 청소 기능을 정의하기 위한 초기 요구사항 분석 산출물이다. 하드웨어 제어의 상세 구현은 범위에서 제외하고, 컨트롤러가 센서 입력을 받아 motor와 cleaner에 어떤 명령을 내려야 하는지에 집중한다.

## Functional Requirements

| ID | Requirement |
| --- | --- |
| FR-001 | `rvc_controller`는 초기 상태에서 전원이 꺼져 있어야 한다. |
| FR-002 | 전원이 꺼진 상태에서 사용자가 버튼을 누르면 `rvc_controller`는 전원을 켜고 청소 시작 절차를 수행해야 한다. |
| FR-003 | 전원이 켜진 상태에서 사용자가 버튼을 누르면 `rvc_controller`는 motor를 정지시키고 cleaner를 끈 뒤 청소를 종료해야 한다. |
| FR-004 | 전원이 켜지거나 청소 중일 때 `rvc_controller`는 전방 장애물 여부를 확인해야 한다. |
| FR-005 | 전방 장애물이 없으면 `rvc_controller`는 motor를 전진시키고 cleaner를 켜야 한다. |
| FR-006 | 전방 장애물이 감지되면 `rvc_controller`는 장애물 회피 프로세스를 시작해야 한다. |
| FR-007 | 장애물 회피 중에는 cleaner를 꺼야 한다. |
| FR-008 | 장애물 회피 프로세스는 먼저 좌측 장애물 여부를 확인해야 한다. |
| FR-009 | 좌측 장애물이 없으면 `rvc_controller`는 motor에 좌회전 명령을 내려야 한다. |
| FR-010 | 좌측 장애물이 있으면 `rvc_controller`는 우측 장애물 여부를 확인해야 한다. |
| FR-011 | 우측 장애물이 없으면 `rvc_controller`는 motor에 우회전 명령을 내려야 한다. |
| FR-012 | 좌측과 우측 모두 장애물이 있으면 `rvc_controller`는 좌측 또는 우측 중 하나가 비어 있을 때까지 motor에 후진 명령을 내려야 한다. |
| FR-013 | 후진 중 좌측 또는 우측 중 하나가 비어 있으면 `rvc_controller`는 장애물이 없는 방향으로 회전해야 한다. |
| FR-014 | 장애물 회피 프로세스가 종료되면 `rvc_controller`는 다시 motor를 전진시키고 cleaner를 켜야 한다. |
| FR-015 | 전진 중 전방 먼지가 감지되면 `rvc_controller`는 motor의 전진 동작을 멈추거나 지연시키지 않은 채 cleaner의 흡입력만 3초 동안 증가시켜야 한다. |
| FR-016 | 흡입력 증가 후 3초가 지나면 `rvc_controller`는 cleaner의 흡입력을 정상화해야 하며, 이 변경은 motor의 전진 동작에 영향을 주지 않아야 한다. |
| FR-017 | 흡입력 증가 중 전방 장애물이 감지되면 `rvc_controller`는 증가된 시간을 저장하지 않고 즉시 cleaner를 끄고 장애물 회피 프로세스를 시작해야 한다. |
| FR-021 | 흡입력 증가 중 다시 먼지가 감지되면 `rvc_controller`는 이전 타이머를 폐기하고 그 시점부터 다시 3초 타이머를 시작해야 한다. 이 재시작은 motor 동작에 영향을 주지 않는다. |
| FR-018 | `rvc_controller`는 motor 명령을 `Forward`, `Backward`, `TurnLeft`, `TurnRight`, `Stop` 중 하나로 출력해야 한다. |
| FR-019 | `rvc_controller`는 cleaner 명령을 `Off`, `On`, `PowerUp` 중 하나로 출력해야 한다. |
| FR-020 | `rvc_controller`는 전방 센서, 좌측 센서, 우측 센서, 먼지 센서, 사용자 버튼 입력을 처리해야 한다. |

## Non-Functional Requirements

| ID | Requirement |
| --- | --- |
| NFR-001 | 장애물 감지 후 cleaner를 끄고 회피 명령을 내리는 반응은 사용자가 체감할 수 없을 정도로 즉시 수행되어야 한다. |
| NFR-002 | 먼지 감지 후 흡입력 증가 시간은 3초를 기준으로 하며, 타이머 오차는 구현 단계에서 명시적으로 관리되어야 한다. |
| NFR-003 | 전원 종료 명령은 청소, 회피, 흡입력 증가 상태보다 우선해야 한다. |
| NFR-004 | 컨트롤러 로직은 하드웨어 상세 제어와 분리되어 단위 테스트가 가능해야 한다. |
| NFR-005 | 모든 핵심 상태 전이는 GTest 기반 단위 테스트로 검증 가능해야 한다. |
| NFR-006 | CI에서는 CMake build, GTest, Cppcheck, clang-tidy, SonarCloud 분석, coverage 수집이 자동으로 수행되어야 한다. |
| NFR-007 | 회피 정책은 좌측 우선 정책을 기본으로 하되, 향후 정책 변경이 가능하도록 controller 내부 의사결정과 actuator 출력이 분리되어야 한다. |
| NFR-008 | 전원 상태, 청소 상태, 회피 상태, 흡입력 증가 상태는 명확한 상태 모델로 표현되어야 한다. |
| NFR-009 | 잘못된 센서 조합이나 반복 입력에도 controller는 정의되지 않은 motor/cleaner 명령을 출력하지 않아야 한다. |
| NFR-010 | 본 Inception 범위에서는 모터 각도, 속도, 흡입력 세기 등 하드웨어 상세 파라미터를 결정하지 않는다. |
| NFR-011 | Motor 출력(`Forward`, `Backward`, `TurnLeft`, `TurnRight`, `Stop`)과 Cleaner 출력(`Off`, `On`, `PowerUp`)은 서로 독립적으로 결정되어야 한다. Motor 상태 변화는 Cleaner 상태에 영향을 줄 수 있으나(예: 전방 장애물 → cleaner off, 전진 시작 → cleaner on), 반대로 Cleaner 상태 변화는 Motor 상태나 진행을 변경하지 않아야 한다(예: 먼지 감지로 cleaner가 `PowerUp`이 되어도 motor의 전진은 멈추거나 느려지지 않는다). |

## Use Cases

### UC-001 Start Cleaning

| Field | Description |
| --- | --- |
| Primary Actor | User |
| Supporting Actors | Front Sensor, Motor, Cleaner |
| Trigger | 전원이 꺼진 상태에서 사용자가 버튼을 누른다. |
| Preconditions | `rvc_controller` 전원이 꺼져 있다. |
| Postconditions | 전원이 켜지고, 전방 장애물이 없으면 전진 청소가 시작된다. |
| Main Success Scenario | 1. User가 power button을 누른다.<br>2. `rvc_controller`가 전원 상태를 `On`으로 변경한다.<br>3. `rvc_controller`가 전방 장애물 여부를 확인한다.<br>4. 전방 장애물이 없다.<br>5. `rvc_controller`가 Motor에 `Forward` 명령을 보낸다.<br>6. `rvc_controller`가 Cleaner에 `On` 명령을 보낸다. |
| Extensions | 3a. 전방 장애물이 있으면 UC-003을 수행한다. |

### UC-002 Stop Cleaning

| Field | Description |
| --- | --- |
| Primary Actor | User |
| Supporting Actors | Motor, Cleaner |
| Trigger | 전원이 켜진 상태에서 사용자가 버튼을 누른다. |
| Preconditions | `rvc_controller` 전원이 켜져 있다. |
| Postconditions | Motor가 정지하고 Cleaner가 꺼지며 전원이 꺼진 상태가 된다. |
| Main Success Scenario | 1. User가 power button을 누른다.<br>2. `rvc_controller`가 진행 중인 청소, 회피, 흡입력 증가 동작을 종료한다.<br>3. `rvc_controller`가 Motor에 `Stop` 명령을 보낸다.<br>4. `rvc_controller`가 Cleaner에 `Off` 명령을 보낸다.<br>5. `rvc_controller`가 전원 상태를 `Off`로 변경한다. |

### UC-003 Avoid Obstacle

| Field | Description |
| --- | --- |
| Primary Actor | Front Sensor |
| Supporting Actors | Left Sensor, Right Sensor, Motor, Cleaner |
| Trigger | 전원이 켜져 있거나 청소 중일 때 전방 장애물이 감지된다. |
| Preconditions | `rvc_controller` 전원이 켜져 있다. |
| Postconditions | 장애물 회피 후 전진 청소 상태로 복귀한다. |
| Main Success Scenario | 1. Front Sensor가 전방 장애물을 감지한다.<br>2. `rvc_controller`가 Cleaner에 `Off` 명령을 보낸다.<br>3. `rvc_controller`가 Left Sensor를 확인한다.<br>4. 좌측 장애물이 없다.<br>5. `rvc_controller`가 Motor에 `TurnLeft` 명령을 보낸다.<br>6. 회피가 종료된다.<br>7. `rvc_controller`가 Motor에 `Forward` 명령을 보낸다.<br>8. `rvc_controller`가 Cleaner에 `On` 명령을 보낸다. |
| Extensions | 4a. 좌측 장애물이 있으면 Right Sensor를 확인한다.<br>4a1. 우측 장애물이 없으면 Motor에 `TurnRight` 명령을 보낸 뒤 전진 청소로 복귀한다.<br>4a2. 우측 장애물도 있으면 좌측 또는 우측 중 하나가 비어 있을 때까지 Motor에 `Backward` 명령을 보낸다.<br>4a3. 빈 방향이 감지되면 해당 방향으로 회전하고 전진 청소로 복귀한다. |

### UC-004 Power Up Cleaning On Dust

| Field | Description |
| --- | --- |
| Primary Actor | Dust Sensor |
| Supporting Actors | Cleaner, Timer, Motor |
| Trigger | 전진 중 전방 먼지가 감지된다. |
| Preconditions | `rvc_controller` 전원이 켜져 있고 Motor가 전진 중이며 Cleaner가 켜져 있다. |
| Postconditions | 3초 동안 motor의 전진은 한 번도 멈추지 않고 그대로 유지되며, cleaner만 흡입력이 증가한 뒤 정상 흡입력으로 복귀한다. |
| Main Success Scenario | 1. Dust Sensor가 먼지를 감지한다.<br>2. `rvc_controller`가 Cleaner에 `PowerUp` 명령을 보낸다 (NFR-011: motor 명령은 발생하지 않는다).<br>3. `rvc_controller`가 3초 타이머를 시작한다.<br>4. Motor는 직전 상태인 `Forward`를 그대로 유지하며 평소와 동일한 속도로 한 칸씩 전진한다.<br>5. 3초가 경과한다.<br>6. `rvc_controller`가 Cleaner에 `On` 명령을 보내 정상 흡입력으로 복귀한다 (NFR-011: motor 명령은 여전히 발생하지 않으며 전진은 끊김 없이 계속된다). |
| Extensions | 3a. 3초가 지나기 전에 전방 장애물이 감지되면 증가 시간을 저장하지 않는다.<br>3a1. `rvc_controller`가 Cleaner에 `Off` 명령을 보낸다.<br>3a2. UC-003을 수행한다 (motor 변화는 회피 절차의 일부로만 발생한다).<br>4a. Power-up 중 다시 먼지가 감지되면 `rvc_controller`는 이전 타이머를 폐기하고 그 시점부터 새 3초 타이머를 시작한다 (FR-021). Motor 명령은 발생하지 않으며 전진은 끊김 없이 유지된다. |

## Diagram Files

- `docs/inception/diagrams/use-case-diagram.puml`
- `docs/inception/diagrams/ssd-start-stop.puml`
- `docs/inception/diagrams/ssd-obstacle-avoidance.puml`
- `docs/inception/diagrams/ssd-dust-power-up.puml`
- `docs/inception/diagrams/domain-model.puml`
