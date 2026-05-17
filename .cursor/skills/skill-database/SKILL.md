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

| Input/ Output Event | Description | Format / Type |
| :--- | :--- | :--- |
| Front Sensor Input | Detects obstacles in front of the RVC | True / False, Interrupt |
| Left Sensor Input | Detects obstacles in the left side of the RVC periodically | True / False, Periodic |
| Right Sensor Input | Detects obstacles in the right side of the RVC periodically | True / False, Periodic |
| Dust Sensor Input | Detects dust on the floor periodically | True / False, Periodic |
| Direction | Direction commands to the motor<br>(go forward / turn left with an angle / turn right with an angle) | Forward / Backward / Left / Right |
| Clean | Turn off / Turn on / Power-Up | On / Off / Up |