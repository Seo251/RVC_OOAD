# RVC Controller Use Case Refinement

## Purpose

이 문서는 Inception 단계의 FR, Use Case, SSD를 기반으로 Elaboration/OOD 단계에서 사용할 상세 Use Case, 실행 우선순위, 시스템 오퍼레이션, 설계 보완점을 정의한다. 시스템 오퍼레이션은 `rvc_controller` 경계로 들어오는 operation만 의미한다.

## Output Independence Principle

NFR-011에 따라 Motor 출력과 Cleaner 출력은 서로 독립적인 의사결정 채널이다.

- **Motor → Cleaner는 허용**: motor 감지/전이에 따라 cleaner 명령이 바뀔 수 있다. 예) 전방 장애물 → `cleaner.off`, 회피 종료 → `cleaner.on`, 전원 OFF → `cleaner.off`.
- **Cleaner → Motor는 금지**: cleaner 상태 변경(`off`/`on`/`PowerUp`)이 motor 명령을 발생시키거나 motor의 진행을 늦추거나 멈추게 해서는 안 된다. 특히 `dustDetected()`, `powerUpTimerExpired()`, 중복 `dustDetected()` 같은 cleaner 중심 이벤트는 motor 포트로 **어떤 명령도 출력하지 않는다**. Motor는 직전 상태(보통 `Forward`)를 그대로 유지한다.

이 원칙은 단위 테스트에서 cleaner 이벤트 처리 후 호출 로그에 motor 메서드 호출이 없음을 직접 검증해서 유지된다.

## Event Priority Rules

| Priority | Event / Condition | Rule |
| --- | --- | --- |
| 1 | `pressPowerButton()` while powered on | 모든 진행 상태를 취소하고 motor stop, cleaner off, timer cancel 후 `PoweredOff`로 전이한다. |
| 2 | `frontObstacleDetected()` | 청소 또는 power-up 상태보다 우선한다. 즉시 cleaner off, timer cancel 후 장애물 회피로 전이한다. |
| 3 | `dustDetected()` while avoiding obstacle | 회피, 회전, 후진 중에는 무시한다. cleaner는 off 상태를 유지한다. motor에도 어떤 영향도 주지 않는다. |
| 4 | `dustDetected()` while power-up already active | 기존 3초 타이머를 폐기하고 그 시점부터 새 3초 타이머를 시작한다. 누적 시간은 저장하지 않는다. Cleaner는 `PowerUp`을 유지하고 motor 포트로는 어떤 명령도 출력하지 않는다 (FR-021, NFR-011). |
| 5 | `powerUpTimerExpired()` | 현재 상태가 `PowerUpCleaning`일 때만 cleaner를 normal on으로 복귀하고 `CleaningForward`로 전이한다. motor 포트로는 어떤 명령도 출력하지 않으며 motor는 직전의 `Forward`를 그대로 유지한다 (NFR-011). |
| 6 | `frontPathClear()` while `PowerUpCleaning` | power-up 청소 상태와 cleaner power-up을 유지하면서 motor에 `Forward` 명령을 다시 보내 다음 한 스텝을 진행한다. 이 motor 명령은 sensor 이벤트(`frontPathClear`)에서 비롯되었으며 cleaner의 상태 변화 때문에 발생한 것이 아니다 (NFR-011). |
| 7 | obstacle avoidance completed | 회피가 끝난 뒤에만 motor forward, cleaner on으로 복귀한다. |

## Controller States

| State | Meaning | Motor | Cleaner |
| --- | --- | --- | --- |
| `PoweredOff` | 전원이 꺼진 상태 | Stop | Off |
| `CheckingFront` | 전원 켜짐 직후 또는 회피 종료 후 전방 확인 | Stop 또는 이전 안전 상태 | Off |
| `CleaningForward` | 전방 장애물이 없고 전진 청소 중 | Forward | On |
| `PowerUpCleaning` | 전진을 유지한 상태로 3초 동안 cleaner power-up 중 | Forward | PowerUp |
| `AvoidingObstacle` | 장애물 회피 판단 및 회피 동작 중 | Left, Right, 또는 Backward | Off |

## System Operations

시스템 오퍼레이션은 외부 actor, sensor, timer, actuator completion event가 `rvc_controller`로 보내는 메시지다.

| Operation | Source | Description | Related FR / UC |
| --- | --- | --- | --- |
| `pressPowerButton()` | User | 전원 off이면 시작, 전원 on이면 즉시 종료한다. | FR-001, FR-002, FR-003, UC-001, UC-002 |
| `frontObstacleDetected()` | Front Sensor | 전방 장애물 감지 interrupt. 즉시 cleaning/power-up을 중단하고 회피로 전이한다. | FR-006, FR-007, FR-017, UC-003 |
| `frontPathClear()` | Front Sensor | 전원 on 또는 회피 종료 후 전방이 비어 있음을 알린다. | FR-004, FR-005, FR-014, UC-001 |
| `sideSensorUpdated(leftBlocked, rightBlocked)` | Side Sensors | 회피 중 좌/우 장애물 상태를 controller에 전달한다. | FR-008, FR-009, FR-010, FR-011, FR-012, FR-013 |
| `motionCompleted(motion)` | Motor | 회전 또는 후진 step 완료를 알린다. 하드웨어 세부 구현은 제외한다. | FR-013, FR-014 |
| `dustDetected()` | Dust Sensor | 전진 청소 중 먼지 감지를 전달한다. | FR-015, FR-017, FR-021, UC-004 |
| `powerUpTimerExpired()` | Timer | 3초 power-up 시간이 지났음을 전달한다. | FR-016, UC-004 |

## Operation Contracts

### `pressPowerButton()`

| Field | Contract |
| --- | --- |
| Preconditions | 없음 |
| Postconditions when powered off | `powerState=On`, front path check requested |
| Postconditions when powered on | active timer canceled, motor stopped, cleaner off, `powerState=Off`, `state=PoweredOff` |
| Priority | Highest |

### `frontObstacleDetected()`

| Field | Contract |
| --- | --- |
| Preconditions | `powerState=On` |
| Postconditions | active power-up timer canceled, cleaner off, `state=AvoidingObstacle`, side sensor check requested |
| Ignored when | `powerState=Off` |
| Priority | Higher than dust detection and timer timeout |

### `sideSensorUpdated(leftBlocked, rightBlocked)`

| Field | Contract |
| --- | --- |
| Preconditions | `state=AvoidingObstacle` |
| Postconditions | left clear -> motor left, right clear -> motor right, both blocked -> motor backward |
| Policy | left side has priority over right side |
| Ignored when | not avoiding obstacle |

### `motionCompleted(motion)`

| Field | Contract |
| --- | --- |
| Preconditions | `state=AvoidingObstacle` |
| Postconditions after turn | `state=CleaningForward`, motor forward, cleaner on |
| Postconditions after backward | request side sensor update again |
| Note | abstracts away hardware-level angle, distance, and speed |

### `dustDetected()`

| Field | Contract |
| --- | --- |
| Preconditions | `powerState=On` |
| Postconditions in `CleaningForward` | cleaner power-up, 3-second timer started, `state=PowerUpCleaning`. **No motor command is issued** (NFR-011); motor stays `Forward`. |
| Postconditions in `PowerUpCleaning` | previous 3-second timer discarded and a new 3-second timer started from this moment, cleaner remains power-up. **No motor command is issued** (NFR-011); motor stays `Forward`. |
| Ignored when | `PoweredOff`, `CheckingFront`, or `AvoidingObstacle` |
| NFR-011 invariant | `dustDetected()` never appears in a sequence that touches the motor port. Forward motion is driven entirely by the front-sensor / motion-completion chain. |

### `powerUpTimerExpired()`

| Field | Contract |
| --- | --- |
| Preconditions | timer event was created by power-up flow |
| Postconditions in `PowerUpCleaning` | cleaner on, `state=CleaningForward`. **No motor command is issued** (NFR-011); motor stays `Forward`. The next forward step is requested through the normal `frontPathClear()` cycle and is therefore caused by a sensor event, not by the cleaner. |
| Ignored when | current state is not `PowerUpCleaning` |

### `frontPathClear()`

| Field | Contract |
| --- | --- |
| Preconditions | `powerState=On` |
| Postconditions in `CheckingFront` / `CleaningForward` / `AvoidingObstacle` | enter `CleaningForward`, motor forward, cleaner on |
| Postconditions in `PowerUpCleaning` | stay in `PowerUpCleaning`, motor forward (no cleaner or timer change) |
| Ignored when | `powerState=Off` |

## Refined Use Cases

### UC-001 Start Cleaning

| Field | Detail |
| --- | --- |
| Trigger | User invokes `pressPowerButton()` while `PoweredOff`. |
| Main Flow | Controller turns power on, enters `CheckingFront`, requests front path state. If clear, motor forward and cleaner on. |
| Alternative | If front obstacle is detected during startup, controller does not turn cleaner on and enters UC-003. |
| Exception | If user presses the button again before cleaning starts, UC-002 priority applies and the controller powers off safely. |

### UC-002 Stop Cleaning

| Field | Detail |
| --- | --- |
| Trigger | User invokes `pressPowerButton()` while powered on. |
| Main Flow | Controller cancels timer, stops motor, turns cleaner off, and sets `PoweredOff`. |
| Priority | This use case overrides cleaning, obstacle avoidance, and dust power-up. |
| Exception | No active process exists: still output motor stop and cleaner off to keep the controller safe. |

### UC-003 Avoid Obstacle

| Field | Detail |
| --- | --- |
| Trigger | `frontObstacleDetected()` while powered on. |
| Main Flow | Controller cancels power-up timer, turns cleaner off, enters `AvoidingObstacle`, checks left/right sensors, turns toward clear side, then resumes forward cleaning. |
| Alternative A | Left blocked and right clear: turn right. |
| Alternative B | Left and right blocked: move backward repeatedly until one side is clear, then turn toward clear side. |
| Exception | Dust detected during avoidance is ignored. Cleaner remains off. |
| Exception | User button during avoidance immediately stops motor, turns cleaner off, cancels timer, and powers off. |

### UC-004 Power Up Cleaning On Dust

| Field | Detail |
| --- | --- |
| Trigger | `dustDetected()` while `CleaningForward`. |
| Main Flow | Controller sets cleaner to power-up and starts a 3-second timer. **No motor command is issued at any point during this use case** (NFR-011); the forward motion that started before the dust event continues uninterrupted, and after 3 seconds the cleaner returns to normal on while motor still keeps moving forward. |
| Alternative | Dust detected again while already in `PowerUpCleaning`: discard the previous timer and start a new 3-second timer from that moment. Cleaner stays at power-up. **No motor command is issued**; motor keeps moving forward. |
| Exception | Front obstacle detected during power-up: cancel timer, turn cleaner off, enter obstacle avoidance. Existing power-up elapsed time is discarded. Motor commands from this exception come from the avoidance flow (a motor-side event), not from the cleaner. |
| Exception | Dust detected while turning/backward/avoiding: ignored because cleaner must remain off during avoidance. Motor is also not touched. |

## Traceability

| FR | Refined Behavior |
| --- | --- |
| FR-001, FR-002, FR-003 | `pressPowerButton()` handles power start/stop with highest priority. |
| FR-004, FR-005 | `frontPathClear()` leads to forward cleaning. |
| FR-006, FR-007 | `frontObstacleDetected()` immediately turns cleaner off and enters avoidance. |
| FR-008, FR-009, FR-010, FR-011 | `sideSensorUpdated()` applies left-first avoidance policy. |
| FR-012, FR-013 | both sides blocked leads to repeated backward motion until a side is clear. |
| FR-014 | `motionCompleted(turn)` resumes forward cleaning. |
| FR-015, FR-016 | `dustDetected()` and `powerUpTimerExpired()` model a 3-second power-up while motor keeps moving forward. |
| FR-017 | obstacle during power-up cancels timer and starts avoidance. |
| FR-021 | repeated `dustDetected()` during `PowerUpCleaning` discards the previous 3-second timer and restarts from the new dust moment without leaving the state and without issuing any motor command. |
| NFR-011 | Cleaner-only events (`dustDetected()`, `powerUpTimerExpired()`, repeated dust) never touch the motor port. Forward motion is driven exclusively by sensor / motion-completion events, which keeps motor and cleaner outputs independently testable. |
| FR-018, FR-019 | motor and cleaner outputs are modeled through actuator ports and command enums. |
| FR-020 | controller accepts user, sensor, timer, and motion completion operations. |

## Logical Gaps Found

| Gap | Resolution in OOD |
| --- | --- |
| Current code has no `Stop` direction command, but stop cleaning requires it. | Add `Stop` to the OOD command model and mark implementation as future update. |
| Current `RvcController::Update()` is stateless. | OOD model introduces `ControllerState`, active timer state, and event-driven system operations. |
| Backward-until-clear needs an event that tells the controller when to re-check sides. | Use `motionCompleted(Backward)` followed by `sideSensorUpdated(leftBlocked, rightBlocked)`. |
| Turn completion is not specified at hardware level. | Abstract it as `motionCompleted(Left/Right)` without defining motor angle or duration. |
| Dust detected during avoidance was not explicitly prioritized. | It is ignored during `AvoidingObstacle`; cleaner remains off. |
| Original FR-015/016 said only "5 seconds power-up" and did not say what motor does, leaving the impression that the robot might stop. | FR-015/016/021 now require motor to stay `Forward` during `PowerUpCleaning`. `frontPathClear()` while `PowerUpCleaning` issues another `motor.forward()` without touching cleaner or state, so the controller still progresses one step at a time but the cleaning state and timer are not disrupted. |
| The original simulator used the cleaner-only `dustDetected()` event as a control point that paused the auto-drive loop for one tick. That violated the motor/cleaner independence that the controller already follows. | NFR-011 makes the independence explicit. Cleaner-only events MUST NOT cause motor commands, and the simulator's tick must continue driving forward in the same tick that it reports a dust event so that the robot never visibly stalls because of a cleaner state change. |
