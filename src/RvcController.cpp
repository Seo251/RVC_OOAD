#include "rvc/RvcController.h"

namespace rvc {

namespace {

constexpr int kPowerUpDurationSeconds = 3;

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

// [CHG-001] Right Sensor 제거: 좌측 센서 하나로 회전 방향만 결정한다.
// 좌측이 비면 좌회전, 막혀 있으면 우회전(우측 센서가 없는 fallback).
MotionCommand ObstacleAvoidancePolicy::SelectMotion(
    const bool left_blocked) const {
  return left_blocked ? MotionCommand::TurnRight : MotionCommand::TurnLeft;
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

  // [CHG-001][Option C] Dead-end: 좌측이 막혀 우회전을 시도했는데 회전 후의
  // 전방마저 막혀 있다면(전방+좌측+우측 모두 막힘) 곧장 후진하지 않는다.
  // 우회전으로 이미 방향을 틀었으므로 그대로 후진하면 방금 등진 좌측
  // 장애물로 들이받는다. 따라서 먼저 좌회전으로 원래 진행 방향을 복원하고,
  // 복원 완료(MotionCompleted)에서 열려 있는 뒤쪽(왔던 칸)으로 안전하게
  // 후진한다. right_turn_attempted_는 그대로 두어 복원 완료를 구분한다.
  if (state_.IsAvoidingObstacle() && right_turn_attempted_) {
    motor_.TurnLeft();
    SetLastMotion(MotionCommand::TurnLeft);
    return;
  }

  timer_.CancelPowerUpTimer();
  cleaner_.TurnOff();
  SetLastCleaner(CleanerCommand::Off);
  state_.SetCleaningState(CleaningState::AvoidingObstacle);
  right_turn_attempted_ = false;
  side_sensor_.RequestSideCheck();
}

void RvcController::FrontPathClear() {
  if (state_.IsPoweredOff()) {
    return;
  }

  if (state_.CurrentCleaningState() == CleaningState::PowerUpCleaning) {
    motor_.Forward();
    SetLastMotion(MotionCommand::Forward);
    return;
  }

  ResumeForwardCleaning();
}

// [CHG-001] Right Sensor 제거: 좌측 센서 결과만으로 좌회전/우회전을 결정한다.
void RvcController::SideSensorUpdated(const bool left_blocked) {
  if (!state_.IsAvoidingObstacle()) {
    return;
  }

  const MotionCommand motion = avoidance_policy_.SelectMotion(left_blocked);

  if (motion == MotionCommand::TurnRight) {
    motor_.TurnRight();
    // 우측이 비었는지 모르는 채 시도하는 fallback 회전. 완료 후 전방을
    // 재확인해 dead-end(후진) 여부를 판단하기 위해 표시해 둔다.
    right_turn_attempted_ = true;
  } else {
    motor_.TurnLeft();
    right_turn_attempted_ = false;
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

  switch (motion) {
    case MotionCommand::Backward:
      // 후진 완료 -> 좌측 센서를 다시 확인해 회피를 재시작한다.
      side_sensor_.RequestSideCheck();
      break;
    case MotionCommand::TurnLeft:
      if (right_turn_attempted_) {
        // [CHG-001][Option C] dead-end 복귀용 좌회전 완료 -> 원래 진행 방향으로
        // 돌아왔으니 이제 열려 있는 뒤쪽(왔던 칸)으로 안전하게 후진한다.
        right_turn_attempted_ = false;
        motor_.Backward();
        SetLastMotion(MotionCommand::Backward);
      } else {
        // 좌측이 비어 좌회전했으므로 바로 전진 청소로 복귀한다.
        ResumeForwardCleaning();
      }
      break;
    case MotionCommand::TurnRight:
      // [CHG-001][Option C] fallback 우회전 완료 -> 전방을 재확인한다.
      // 전방이 비면 FrontPathClear()로 복귀(우측이 열려 있던 경우, 후진 없음),
      // 막혀 있으면 FrontObstacleDetected()가 dead-end로 보고 좌회전으로
      // 원래 방향 복원 후 후진한다.
      front_sensor_.RequestFrontCheck();
      break;
    case MotionCommand::Stop:
    case MotionCommand::Forward:
      break;
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

  // [CHG-001] Right Sensor 제거: 좌측이 막히면 우회전(fallback). dead-end 후진은
  // 순차적 전방 재확인이 필요하므로 one-shot 헬퍼에서는 표현하지 않는다.
  command.motion = MotionCommand::TurnRight;
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
  right_turn_attempted_ = false;
}

void RvcController::PowerOffSafely() {
  timer_.CancelPowerUpTimer();
  motor_.Stop();
  cleaner_.TurnOff();
  SetLastMotion(MotionCommand::Stop);
  SetLastCleaner(CleanerCommand::Off);
  state_.SetCleaningState(CleaningState::PoweredOff);
  state_.SetPowerState(PowerState::Off);
  right_turn_attempted_ = false;
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
