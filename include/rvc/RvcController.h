#pragma once

#include <string>
#include <vector>

namespace rvc {

enum class PowerState {
  Off,
  On,
};

enum class CleaningState {
  PoweredOff,
  CheckingFront,
  CleaningForward,
  PowerUpCleaning,
  AvoidingObstacle,
};

enum class MotionCommand {
  Stop,
  Forward,
  Backward,
  TurnLeft,
  TurnRight,
};

enum class CleanerCommand {
  Off,
  On,
  PowerUp,
};

// [CHG-001] Right Sensor 제거: right_obstacle 필드 삭제 (Front + Left 센서만 사용)
struct SensorInput {
  bool front_obstacle{false};
  bool left_obstacle{false};
  bool dust_detected{false};
};

struct ControlCommand {
  MotionCommand motion{MotionCommand::Forward};
  CleanerCommand cleaner{CleanerCommand::On};
};

struct ControllerSnapshot {
  PowerState power_state{PowerState::Off};
  CleaningState cleaning_state{CleaningState::PoweredOff};
  MotionCommand last_motion{MotionCommand::Stop};
  CleanerCommand last_cleaner{CleanerCommand::Off};
};

class MotorPort {
 public:
  virtual ~MotorPort() = default;
  virtual void Forward() = 0;
  virtual void Backward() = 0;
  virtual void TurnLeft() = 0;
  virtual void TurnRight() = 0;
  virtual void Stop() = 0;
};

class CleanerPort {
 public:
  virtual ~CleanerPort() = default;
  virtual void TurnOn() = 0;
  virtual void TurnOff() = 0;
  virtual void PowerUp() = 0;
};

class TimerPort {
 public:
  virtual ~TimerPort() = default;
  virtual void StartPowerUpTimer(int seconds) = 0;
  virtual void RestartPowerUpTimer(int seconds) = 0;
  virtual void CancelPowerUpTimer() = 0;
};

class FrontSensorPort {
 public:
  virtual ~FrontSensorPort() = default;
  virtual void RequestFrontCheck() = 0;
};

class SideSensorPort {
 public:
  virtual ~SideSensorPort() = default;
  virtual void RequestSideCheck() = 0;
};

class ControllerState {
 public:
  bool IsPoweredOn() const;
  bool IsPoweredOff() const;
  bool IsAvoidingObstacle() const;
  CleaningState CurrentCleaningState() const;
  PowerState CurrentPowerState() const;
  void SetPowerState(PowerState state);
  void SetCleaningState(CleaningState state);

 private:
  PowerState power_state_{PowerState::Off};
  CleaningState cleaning_state_{CleaningState::PoweredOff};
};

// [CHG-001][Option C] Right Sensor 제거: 좌측 센서 하나로만 회전 방향 결정.
// left clear -> TurnLeft, left blocked -> TurnRight (우측 센서 없는 fallback).
// dead-end(우회전 후 전방까지 막힘)는 RvcController가 좌회전(원래 방향 복원)
// 후 후진으로 처리한다.
class ObstacleAvoidancePolicy {
 public:
  MotionCommand SelectMotion(bool left_blocked) const;
};

class RvcController {
 public:
  RvcController(MotorPort& motor, CleanerPort& cleaner, TimerPort& timer,
                FrontSensorPort& front_sensor, SideSensorPort& side_sensor);

  void PressPowerButton();
  void FrontObstacleDetected();
  void FrontPathClear();
  // [CHG-001] Right Sensor 제거: left_blocked 하나만 받는다.
  void SideSensorUpdated(bool left_blocked);
  void DustDetected();
  void PowerUpTimerExpired();
  void MotionCompleted(MotionCommand motion);

  ControllerSnapshot Snapshot() const;

  // Compatibility helper for simple one-shot decisions used by early tests.
  ControlCommand Update(const SensorInput& input) const;

 private:
  void SetLastMotion(MotionCommand motion);
  void SetLastCleaner(CleanerCommand cleaner);
  void ResumeForwardCleaning();
  void PowerOffSafely();

  MotorPort& motor_;
  CleanerPort& cleaner_;
  TimerPort& timer_;
  FrontSensorPort& front_sensor_;
  SideSensorPort& side_sensor_;
  ControllerState state_;
  ObstacleAvoidancePolicy avoidance_policy_;
  MotionCommand last_motion_{MotionCommand::Stop};
  CleanerCommand last_cleaner_{CleanerCommand::Off};
  // [CHG-001][Option C] 좌측이 막혀 우회전(fallback)을 시도했는지 추적한다.
  // 우회전 후 전방이 여전히 막혀 있으면(dead-end) 먼저 좌회전으로 원래 진행
  // 방향을 복원한 뒤(이 플래그로 복원용 좌회전과 일반 좌회피를 구분), 열린
  // 뒤쪽으로 안전하게 후진한다.
  bool right_turn_attempted_{false};
};

std::string ToString(PowerState state);
std::string ToString(CleaningState state);
std::string ToString(MotionCommand command);
std::string ToString(CleanerCommand command);

}  // namespace rvc
