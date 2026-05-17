#include "rvc/RvcController.h"

namespace rvc {

namespace {

constexpr int kPowerUpDurationSeconds = 5;

}  // namespace

bool ControllerState::IsPoweredOn() const {
  return power_state_ == PowerState::On;
}

bool ControllerState::IsPoweredOff() const {
  return power_state_ == PowerState::Off;
}

bool ControllerState::IsAvoidingObstacle() const {
  return cleaning_state_ == CleaningState::AvoidingObstacle;
}

CleaningState ControllerState::CurrentCleaningState() const {
  return cleaning_state_;
}

PowerState ControllerState::CurrentPowerState() const {
  return power_state_;
}

void ControllerState::SetPowerState(PowerState state) {
  power_state_ = state;
}

void ControllerState::SetCleaningState(CleaningState state) {
  cleaning_state_ = state;
}

MotionCommand ObstacleAvoidancePolicy::SelectMotion(
    const bool left_blocked, const bool right_blocked) const {
  if (!left_blocked) {
    return MotionCommand::TurnLeft;
  }
  if (!right_blocked) {
    return MotionCommand::TurnRight;
  }
  return MotionCommand::Backward;
}

RvcController::RvcController(MotorPort& motor, CleanerPort& cleaner,
                             TimerPort& timer, FrontSensorPort& front_sensor,
                             SideSensorPort& side_sensor)
    : motor_(motor),
      cleaner_(cleaner),
      timer_(timer),
      front_sensor_(front_sensor),
      side_sensor_(side_sensor) {}

void RvcController::PressPowerButton() {
  if (state_.IsPoweredOff()) {
    state_.SetPowerState(PowerState::On);
    state_.SetCleaningState(CleaningState::CheckingFront);
    front_sensor_.RequestFrontCheck();
    return;
  }

  PowerOffSafely();
}

void RvcController::FrontObstacleDetected() {
  if (state_.IsPoweredOff()) {
    return;
  }

  timer_.CancelPowerUpTimer();
  cleaner_.TurnOff();
  SetLastCleaner(CleanerCommand::Off);
  state_.SetCleaningState(CleaningState::AvoidingObstacle);
  side_sensor_.RequestSideCheck();
}

void RvcController::FrontPathClear() {
  if (state_.IsPoweredOff()) {
    return;
  }

  ResumeForwardCleaning();
}

void RvcController::SideSensorUpdated(const bool left_blocked,
                                      const bool right_blocked) {
  if (!state_.IsAvoidingObstacle()) {
    return;
  }

  const MotionCommand motion =
      avoidance_policy_.SelectMotion(left_blocked, right_blocked);

  switch (motion) {
    case MotionCommand::TurnLeft:
      motor_.TurnLeft();
      break;
    case MotionCommand::TurnRight:
      motor_.TurnRight();
      break;
    case MotionCommand::Backward:
      motor_.Backward();
      break;
    case MotionCommand::Stop:
    case MotionCommand::Forward:
      return;
  }

  SetLastMotion(motion);
}

void RvcController::DustDetected() {
  if (state_.CurrentCleaningState() == CleaningState::CleaningForward) {
    cleaner_.PowerUp();
    timer_.StartPowerUpTimer(kPowerUpDurationSeconds);
    SetLastCleaner(CleanerCommand::PowerUp);
    state_.SetCleaningState(CleaningState::PowerUpCleaning);
    return;
  }

  if (state_.CurrentCleaningState() == CleaningState::PowerUpCleaning) {
    timer_.RestartPowerUpTimer(kPowerUpDurationSeconds);
    cleaner_.PowerUp();
    SetLastCleaner(CleanerCommand::PowerUp);
  }
}

void RvcController::PowerUpTimerExpired() {
  if (state_.CurrentCleaningState() != CleaningState::PowerUpCleaning) {
    return;
  }

  cleaner_.TurnOn();
  SetLastCleaner(CleanerCommand::On);
  state_.SetCleaningState(CleaningState::CleaningForward);
}

void RvcController::MotionCompleted(const MotionCommand motion) {
  if (!state_.IsAvoidingObstacle()) {
    return;
  }

  if (motion == MotionCommand::Backward) {
    side_sensor_.RequestSideCheck();
    return;
  }

  if (motion == MotionCommand::TurnLeft || motion == MotionCommand::TurnRight) {
    ResumeForwardCleaning();
  }
}

ControllerSnapshot RvcController::Snapshot() const {
  return ControllerSnapshot{state_.CurrentPowerState(),
                            state_.CurrentCleaningState(), last_motion_,
                            last_cleaner_};
}

ControlCommand RvcController::Update(const SensorInput& input) const {
  ControlCommand command;

  if (input.dust_detected) {
    command.cleaner = CleanerCommand::PowerUp;
  }

  if (!input.front_obstacle) {
    command.motion = MotionCommand::Forward;
    return command;
  }

  if (!input.left_obstacle) {
    command.motion = MotionCommand::TurnLeft;
    return command;
  }

  if (!input.right_obstacle) {
    command.motion = MotionCommand::TurnRight;
    return command;
  }

  command.motion = MotionCommand::Backward;
  return command;
}

void RvcController::SetLastMotion(const MotionCommand motion) {
  last_motion_ = motion;
}

void RvcController::SetLastCleaner(const CleanerCommand cleaner) {
  last_cleaner_ = cleaner;
}

void RvcController::ResumeForwardCleaning() {
  state_.SetCleaningState(CleaningState::CleaningForward);
  motor_.Forward();
  cleaner_.TurnOn();
  SetLastMotion(MotionCommand::Forward);
  SetLastCleaner(CleanerCommand::On);
}

void RvcController::PowerOffSafely() {
  timer_.CancelPowerUpTimer();
  motor_.Stop();
  cleaner_.TurnOff();
  SetLastMotion(MotionCommand::Stop);
  SetLastCleaner(CleanerCommand::Off);
  state_.SetCleaningState(CleaningState::PoweredOff);
  state_.SetPowerState(PowerState::Off);
}

std::string ToString(const PowerState state) {
  switch (state) {
    case PowerState::Off:
      return "Off";
    case PowerState::On:
      return "On";
  }
  return "Unknown";
}

std::string ToString(const CleaningState state) {
  switch (state) {
    case CleaningState::PoweredOff:
      return "PoweredOff";
    case CleaningState::CheckingFront:
      return "CheckingFront";
    case CleaningState::CleaningForward:
      return "CleaningForward";
    case CleaningState::PowerUpCleaning:
      return "PowerUpCleaning";
    case CleaningState::AvoidingObstacle:
      return "AvoidingObstacle";
  }
  return "Unknown";
}

std::string ToString(const MotionCommand command) {
  switch (command) {
    case MotionCommand::Stop:
      return "Stop";
    case MotionCommand::Forward:
      return "Forward";
    case MotionCommand::Backward:
      return "Backward";
    case MotionCommand::TurnLeft:
      return "TurnLeft";
    case MotionCommand::TurnRight:
      return "TurnRight";
  }
  return "Unknown";
}

std::string ToString(const CleanerCommand command) {
  switch (command) {
    case CleanerCommand::Off:
      return "Off";
    case CleanerCommand::On:
      return "On";
    case CleanerCommand::PowerUp:
      return "PowerUp";
  }
  return "Unknown";
}

}  // namespace rvc
