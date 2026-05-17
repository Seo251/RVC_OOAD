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

struct SensorInput {
  bool front_obstacle{false};
  bool left_obstacle{false};
  bool right_obstacle{false};
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

class ObstacleAvoidancePolicy {
 public:
  MotionCommand SelectMotion(bool left_blocked, bool right_blocked) const;
};

class RvcController {
 public:
  RvcController(MotorPort& motor, CleanerPort& cleaner, TimerPort& timer,
                FrontSensorPort& front_sensor, SideSensorPort& side_sensor);

  void PressPowerButton();
  void FrontObstacleDetected();
  void FrontPathClear();
  void SideSensorUpdated(bool left_blocked, bool right_blocked);
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
};

std::string ToString(PowerState state);
std::string ToString(CleaningState state);
std::string ToString(MotionCommand command);
std::string ToString(CleanerCommand command);

}  // namespace rvc
