---
name: skill-database
description: Provides RVC software controller requirements and domain constraints for OOAD work. Use when working on RVC requirements, design, controller behavior, or project documentation.
---

### Preliminary Requirements for RVC SW Controller

• An RVC automatically cleans and mops household surface.
• It goes straight forward while cleaning.
• If its sensors found an obstacle, it stops cleaning, turns aside left or right, and goes forward with cleaning.
• If there are obstacles in both front, left and right, it move backward and turn aside left or right, and goes forward.
• If it detects dust, power up the cleaning for a while.
• We do not consider the detail design and implementation on HW controls.
• We only focus on the automatic cleaning function. 

### RVC Input/Output Event Definitions

<!-- [CHG-001] Remove Right Sensor: 3 obstacle sensors -> 2 (Front + Left only) -->

| Input/ Output Event | Description | Format / Type |
| :--- | :--- | :--- |
| Front Sensor Input | Detects obstacles in front of the RVC | True / False, Interrupt |
| Left Sensor Input | Detects obstacles in the left side of the RVC periodically | True / False, Periodic |
| <span style="color:#c62828">~~Right Sensor Input~~</span> | <span style="color:#c62828">~~Detects obstacles in the right side of the RVC periodically~~ (CHG-001: removed)</span> | <span style="color:#c62828">~~True / False, Periodic~~</span> |
| Dust Sensor Input | Detects dust on the floor periodically | True / False, Periodic |
| Direction | Direction commands to the motor<br>(go forward / turn left with an angle / turn right with an angle) | Forward / Backward / Left / Right |
| Clean | Turn off / Turn on / Power-Up | On / Off / Up |

> **[CHG-001] 1차 요구사항 변경**: Right Sensor 입력을 제거한다. 장애물 감지는 <span style="color:#2e7d32">Front Sensor + Left Sensor 두 개</span>만 사용한다.
> Preliminary Requirements(자동 청소, 직진, 장애물 시 회피/후진, 먼지 시 power-up)는 그대로 충족하되, 우측 장애물은 센싱하지 않고 좌측 센서와 전방 센서만으로 회피 방향을 결정한다. Motor의 `Right`(우회전) 출력 자체는 유지된다(센서 없이 수행하는 fallback 회전).