# RVC Controller Change Log

요구사항 변경에 따른 SRS / SDD / CODE / UT 변경을 변경 ID 단위로 추적한다.
색상 규칙: <span style="color:#c62828">삭제(빨강)</span>, <span style="color:#2e7d32">추가(초록)</span>, <span style="color:#1565c0">변경(파랑)</span>.

## CHG-001 Remove Right Sensor (3 obstacle sensors → 2)

- **요약**: 장애물 감지 센서를 Front + Left + Right 3개에서 <span style="color:#1565c0">Front + Left 2개</span>로 축소한다. Right Sensor 입력을 제거한다.
- **전제**: Preliminary Requirements는 변경되지 않으며 그대로 충족되어야 한다. Motor의 `TurnRight` 출력 자체는 유지된다(우측 센서 없이 수행하는 fallback 회전).

### 회피 로직 변경 (핵심 설계 결정)

| 상황 | 변경 전 (3센서) | 변경 후 (2센서) |
| --- | --- | --- |
| 전방 장애물, 좌측 clear | 좌회전 | 좌회전 (동일) |
| 전방 장애물, 좌측 blocked | 우측 센서 확인 → 우측 clear면 우회전 | <span style="color:#1565c0">우회전 → 전방 재확인</span> |
| 우회전 후 전방 clear (우측이 열려 있던 경우) | (해당 없음) | <span style="color:#2e7d32">후진 없이 전진 청소 복귀</span> |
| 전·좌·우 모두 blocked (dead-end) | 한쪽이 빌 때까지 후진 후 회전 | <span style="color:#1565c0">좌회전(원래 방향 복원) → 후진 → 회피 재시작 (Option C)</span> |

> 우측 장애물을 직접 센싱할 수 없으므로, 좌측이 막히면 **먼저 우회전**한 뒤 전방 센서로 우측 가용 여부를 간접 확인한다. 우측이 열려 있으면 후진 없이 복귀하므로 불필요한 후진이 없다. 후진은 우회전 후에도 전방이 막힌 **진짜 dead-end(전·좌·우 모두 막힘)** 에서만 발생하며, 이는 Preliminary의 "obstacles in front, left and right → backward"와 정확히 일치한다.

> <span style="color:#1565c0">[Option C 보정]</span> 우회전을 하면 진행 방향이 이미 바뀌어 있어, dead-end에서 **곧바로 후진하면 방금 등진 좌측 장애물로 들이받는다**(뒤쪽 충돌). 따라서 dead-end에서는 **먼저 좌회전으로 원래 진행 방향을 복원**한 뒤, 항상 열려 있는 뒤쪽(왔던 칸)으로 안전하게 후진한다. `right_turn_attempted_` 플래그로 이 복원용 좌회전(→ 후진)과 일반 좌측 회피 좌회전(→ 전진 복귀)을 구분한다.

### 산출물별 추적

| 산출물 | 파일 | 상태 |
| --- | --- | --- |
| Skill (baseline) | `.cursor/skills/skill-database/SKILL.md` | ✅ Right Sensor 행 삭제 |
| SRS | `docs/inception/requirements.md` | ✅ FR-008~013, FR-020, NFR-007, UC-003 |
| SRS (refinement) | `docs/elaboration/use-case-details.md` | ✅ SideSensorUpdated, 우선순위, UC-003, traceability |
| SDD | `docs/inception/diagrams/ssd-obstacle-avoidance.puml`, `docs/elaboration/diagrams/*.puml`, `docs/reports/fr-traceability-matrix-uc003.puml` | ✅ 2센서 회피 흐름 반영 |
| CODE | `include/rvc/RvcController.h`, `src/RvcController.cpp`, `simulator/RvcSimulatorBridge.cpp` | ✅ `SideSensorUpdated(leftBlocked)`, `right_turn_attempted_` dead-end 로직 (Option C: 좌회전 복원 → 후진) |
| UT | `tests/RvcControllerTest.cpp` | ✅ 2센서 재작성 + dead-end 케이스 (Option C: 좌회전→후진→side.request) |
| System Test | `system_tests/suites/rvc_30_system_tests.json`, `system_tests/cases/*.json` | ✅ 04~08·16·26·30 + cases 2개 2센서, dead-end에 TurnLeft 복원 스텝 추가 |
| Simulator | `simulator/simulator_ui.py` | ✅ 수동 버튼 2개, 좌측 단일 센싱, 06~08 시각 맵 재설계 |
| 기타 | `.github/ISSUE_TEMPLATE/bug_report.yml`, `README.md` | ✅ 우측 센서 문구 정리 |
