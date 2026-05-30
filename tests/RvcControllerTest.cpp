#include "rvc/RvcController.h"

#include "FakePorts.h"

#include <gtest/gtest.h>

#include <vector>

namespace rvc {
namespace {

using test::ControllerFixture;

TEST(RvcControllerTest, StartsByRequestingFrontCheck) {
  ControllerFixture fixture;

  fixture.controller.PressPowerButton();

  const ControllerSnapshot snapshot = fixture.controller.Snapshot();
  EXPECT_EQ(snapshot.power_state, PowerState::On);
  EXPECT_EQ(snapshot.cleaning_state, CleaningState::CheckingFront);
  EXPECT_EQ(fixture.log.calls, std::vector<std::string>{"front.request"});
}

TEST(RvcControllerTest, StartsForwardCleaningWhenFrontPathIsClear) {
  ControllerFixture fixture;

  fixture.controller.PressPowerButton();
  fixture.controller.FrontPathClear();

  const ControllerSnapshot snapshot = fixture.controller.Snapshot();
  EXPECT_EQ(snapshot.cleaning_state, CleaningState::CleaningForward);
  EXPECT_EQ(snapshot.last_motion, MotionCommand::Forward);
  EXPECT_EQ(snapshot.last_cleaner, CleanerCommand::On);
  EXPECT_EQ(fixture.log.calls,
            (std::vector<std::string>{"front.request", "motor.forward",
                                      "cleaner.on"}));
}

TEST(RvcControllerTest, PowerButtonStopsEverythingWhenPoweredOn) {
  ControllerFixture fixture;

  fixture.controller.PressPowerButton();
  fixture.controller.FrontPathClear();
  fixture.log.calls.clear();

  fixture.controller.PressPowerButton();

  const ControllerSnapshot snapshot = fixture.controller.Snapshot();
  EXPECT_EQ(snapshot.power_state, PowerState::Off);
  EXPECT_EQ(snapshot.cleaning_state, CleaningState::PoweredOff);
  EXPECT_EQ(snapshot.last_motion, MotionCommand::Stop);
  EXPECT_EQ(snapshot.last_cleaner, CleanerCommand::Off);
  EXPECT_EQ(fixture.log.calls,
            (std::vector<std::string>{"timer.cancel", "motor.stop",
                                      "cleaner.off"}));
}

TEST(RvcControllerTest, FrontObstacleInterruptsPowerUpAndRequestsSideCheck) {
  ControllerFixture fixture;
  fixture.controller.PressPowerButton();
  fixture.controller.FrontPathClear();
  fixture.controller.DustDetected();
  fixture.log.calls.clear();

  fixture.controller.FrontObstacleDetected();

  const ControllerSnapshot snapshot = fixture.controller.Snapshot();
  EXPECT_EQ(snapshot.cleaning_state, CleaningState::AvoidingObstacle);
  EXPECT_EQ(snapshot.last_cleaner, CleanerCommand::Off);
  EXPECT_EQ(fixture.log.calls,
            (std::vector<std::string>{"timer.cancel", "cleaner.off",
                                      "side.request"}));
}

// [CHG-001] 좌측이 비면 좌회전 (우측 센서 없음).
TEST(RvcControllerTest, AvoidanceTurnsLeftWhenLeftIsClear) {
  ControllerFixture fixture;
  fixture.controller.PressPowerButton();
  fixture.controller.FrontObstacleDetected();
  fixture.log.calls.clear();

  fixture.controller.SideSensorUpdated(false);

  EXPECT_EQ(fixture.controller.Snapshot().last_motion, MotionCommand::TurnLeft);
  EXPECT_EQ(fixture.log.calls, std::vector<std::string>{"motor.turnLeft"});
}

// [CHG-001] 좌측이 막히면 우회전(fallback). 후진은 하지 않는다.
TEST(RvcControllerTest, AvoidanceTurnsRightWhenLeftBlocked) {
  ControllerFixture fixture;
  fixture.controller.PressPowerButton();
  fixture.controller.FrontObstacleDetected();
  fixture.log.calls.clear();

  fixture.controller.SideSensorUpdated(true);

  EXPECT_EQ(fixture.controller.Snapshot().last_motion, MotionCommand::TurnRight);
  EXPECT_EQ(fixture.log.calls, std::vector<std::string>{"motor.turnRight"});
}

// [CHG-001] 우회전 완료 후에는 전방을 재확인한다 (즉시 복귀하지 않는다).
TEST(RvcControllerTest, RightTurnCompletionRequestsFrontRecheck) {
  ControllerFixture fixture;
  fixture.controller.PressPowerButton();
  fixture.controller.FrontObstacleDetected();
  fixture.controller.SideSensorUpdated(true);
  fixture.log.calls.clear();

  fixture.controller.MotionCompleted(MotionCommand::TurnRight);

  EXPECT_EQ(fixture.controller.Snapshot().cleaning_state,
            CleaningState::AvoidingObstacle);
  EXPECT_EQ(fixture.log.calls, std::vector<std::string>{"front.request"});
}

// [CHG-001] 우회전 후 전방이 비면(우측이 열려 있던 경우) 후진 없이 복귀한다.
TEST(RvcControllerTest, RightTurnWithClearFrontResumesWithoutBackward) {
  ControllerFixture fixture;
  fixture.controller.PressPowerButton();
  fixture.controller.FrontObstacleDetected();
  fixture.controller.SideSensorUpdated(true);
  fixture.controller.MotionCompleted(MotionCommand::TurnRight);
  fixture.log.calls.clear();

  fixture.controller.FrontPathClear();

  const ControllerSnapshot snapshot = fixture.controller.Snapshot();
  EXPECT_EQ(snapshot.cleaning_state, CleaningState::CleaningForward);
  EXPECT_EQ(snapshot.last_motion, MotionCommand::Forward);
  EXPECT_EQ(snapshot.last_cleaner, CleanerCommand::On);
  for (const std::string& call : fixture.log.calls) {
    EXPECT_NE(call, "motor.backward") << "Right turn with clear front backed up";
  }
}

// [CHG-001][Option C] dead-end: 우회전 후에도 전방이 막혀 있으면(전+좌+우 모두
// 막힘) 곧장 후진하지 않는다. 우회전으로 등진 좌측 장애물로 후진해 들이받지
// 않도록, 먼저 좌회전으로 원래 진행 방향을 복원한 뒤 열린 뒤쪽으로 후진하고,
// 후진 완료 후 좌측을 다시 확인한다.
TEST(RvcControllerTest, DeadEndAfterRightTurnRecoversHeadingThenBacksUpThenRechecksLeft) {
  ControllerFixture fixture;
  fixture.controller.PressPowerButton();
  fixture.controller.FrontObstacleDetected();
  fixture.controller.SideSensorUpdated(true);
  fixture.controller.MotionCompleted(MotionCommand::TurnRight);
  fixture.log.calls.clear();

  // 우회전 후의 전방마저 막혀 있음 -> dead-end -> 먼저 좌회전으로 원래 방향 복원.
  fixture.controller.FrontObstacleDetected();
  EXPECT_EQ(fixture.controller.Snapshot().last_motion, MotionCommand::TurnLeft);
  EXPECT_EQ(fixture.log.calls, std::vector<std::string>{"motor.turnLeft"});

  // 원래 방향 복원(좌회전) 완료 -> 열린 뒤쪽(왔던 칸)으로 안전하게 후진.
  fixture.log.calls.clear();
  fixture.controller.MotionCompleted(MotionCommand::TurnLeft);
  EXPECT_EQ(fixture.controller.Snapshot().last_motion, MotionCommand::Backward);
  EXPECT_EQ(fixture.log.calls, std::vector<std::string>{"motor.backward"});

  // 후진 완료 -> 좌측 센서 재확인.
  fixture.log.calls.clear();
  fixture.controller.MotionCompleted(MotionCommand::Backward);
  EXPECT_EQ(fixture.log.calls, std::vector<std::string>{"side.request"});
}

// [CHG-001] 좌회전 경로에서는 우회전 fallback 표시가 서지 않으므로, 이후
// 전방 장애물은 dead-end(후진)가 아니라 일반 회피(좌측 재확인)로 처리된다.
TEST(RvcControllerTest, LeftTurnPathDoesNotTriggerDeadEndBackward) {
  ControllerFixture fixture;
  fixture.controller.PressPowerButton();
  fixture.controller.FrontObstacleDetected();
  fixture.controller.SideSensorUpdated(false);  // turn left
  fixture.log.calls.clear();

  fixture.controller.FrontObstacleDetected();

  EXPECT_EQ(fixture.controller.Snapshot().last_motion, MotionCommand::TurnLeft);
  EXPECT_EQ(fixture.log.calls,
            (std::vector<std::string>{"timer.cancel", "cleaner.off",
                                      "side.request"}));
}

TEST(RvcControllerTest, BackwardCompletionRequestsSideCheckAgain) {
  ControllerFixture fixture;
  fixture.controller.PressPowerButton();
  fixture.controller.FrontObstacleDetected();
  fixture.log.calls.clear();

  fixture.controller.MotionCompleted(MotionCommand::Backward);

  EXPECT_EQ(fixture.log.calls, std::vector<std::string>{"side.request"});
}

TEST(RvcControllerTest, TurnCompletionResumesForwardCleaning) {
  ControllerFixture fixture;
  fixture.controller.PressPowerButton();
  fixture.controller.FrontObstacleDetected();
  fixture.log.calls.clear();

  fixture.controller.MotionCompleted(MotionCommand::TurnLeft);

  const ControllerSnapshot snapshot = fixture.controller.Snapshot();
  EXPECT_EQ(snapshot.cleaning_state, CleaningState::CleaningForward);
  EXPECT_EQ(snapshot.last_motion, MotionCommand::Forward);
  EXPECT_EQ(snapshot.last_cleaner, CleanerCommand::On);
  EXPECT_EQ(fixture.log.calls,
            (std::vector<std::string>{"motor.forward", "cleaner.on"}));
}

TEST(RvcControllerTest, DustPowerUpStartsThreeSecondTimerDuringForwardCleaning) {
  ControllerFixture fixture;
  fixture.controller.PressPowerButton();
  fixture.controller.FrontPathClear();
  fixture.log.calls.clear();

  fixture.controller.DustDetected();

  const ControllerSnapshot snapshot = fixture.controller.Snapshot();
  EXPECT_EQ(snapshot.cleaning_state, CleaningState::PowerUpCleaning);
  EXPECT_EQ(snapshot.last_cleaner, CleanerCommand::PowerUp);
  EXPECT_EQ(snapshot.last_motion, MotionCommand::Forward);
  EXPECT_EQ(fixture.log.calls,
            (std::vector<std::string>{"cleaner.powerUp", "timer.start:3"}));
}

TEST(RvcControllerTest, DuplicateDustRestartsThreeSecondTimerAndKeepsState) {
  ControllerFixture fixture;
  fixture.controller.PressPowerButton();
  fixture.controller.FrontPathClear();
  fixture.controller.DustDetected();
  fixture.log.calls.clear();

  fixture.controller.DustDetected();

  const ControllerSnapshot snapshot = fixture.controller.Snapshot();
  EXPECT_EQ(snapshot.cleaning_state, CleaningState::PowerUpCleaning);
  EXPECT_EQ(snapshot.last_cleaner, CleanerCommand::PowerUp);
  EXPECT_EQ(snapshot.last_motion, MotionCommand::Forward);
  EXPECT_EQ(fixture.log.calls,
            (std::vector<std::string>{"timer.restart:3", "cleaner.powerUp"}));
}

TEST(RvcControllerTest, FrontPathClearDuringPowerUpKeepsStateAndDrivesForward) {
  ControllerFixture fixture;
  fixture.controller.PressPowerButton();
  fixture.controller.FrontPathClear();
  fixture.controller.DustDetected();
  fixture.log.calls.clear();

  fixture.controller.FrontPathClear();

  const ControllerSnapshot snapshot = fixture.controller.Snapshot();
  EXPECT_EQ(snapshot.cleaning_state, CleaningState::PowerUpCleaning);
  EXPECT_EQ(snapshot.last_cleaner, CleanerCommand::PowerUp);
  EXPECT_EQ(snapshot.last_motion, MotionCommand::Forward);
  EXPECT_EQ(fixture.log.calls, std::vector<std::string>{"motor.forward"});
}

// NFR-011: cleaner-only events (dustDetected, repeated dustDetected,
// powerUpTimerExpired) must never touch the motor port. This test exercises
// the full PowerUpCleaning enter / restart / expire sequence and asserts that
// every recorded call belongs to the cleaner or the timer, never the motor.
TEST(RvcControllerTest,
     CleanerOnlyEventsDoNotIssueMotorCommandsDuringPowerUpFlow) {
  ControllerFixture fixture;
  fixture.controller.PressPowerButton();
  fixture.controller.FrontPathClear();
  fixture.log.calls.clear();

  fixture.controller.DustDetected();
  fixture.controller.DustDetected();
  fixture.controller.PowerUpTimerExpired();

  const ControllerSnapshot snapshot = fixture.controller.Snapshot();
  EXPECT_EQ(snapshot.last_motion, MotionCommand::Forward);
  EXPECT_EQ(snapshot.cleaning_state, CleaningState::CleaningForward);

  for (const std::string& call : fixture.log.calls) {
    EXPECT_EQ(call.rfind("motor.", 0), std::string::npos)
        << "Cleaner-only flow produced motor call: " << call;
  }

  EXPECT_EQ(fixture.log.calls,
            (std::vector<std::string>{"cleaner.powerUp", "timer.start:3",
                                      "timer.restart:3", "cleaner.powerUp",
                                      "cleaner.on"}));
}

TEST(RvcControllerTest, DustIsIgnoredDuringAvoidance) {
  ControllerFixture fixture;
  fixture.controller.PressPowerButton();
  fixture.controller.FrontObstacleDetected();
  fixture.log.calls.clear();

  fixture.controller.DustDetected();

  EXPECT_EQ(fixture.controller.Snapshot().cleaning_state,
            CleaningState::AvoidingObstacle);
  EXPECT_TRUE(fixture.log.calls.empty());
}

TEST(RvcControllerTest, TimerExpiryRestoresNormalCleaning) {
  ControllerFixture fixture;
  fixture.controller.PressPowerButton();
  fixture.controller.FrontPathClear();
  fixture.controller.DustDetected();
  fixture.log.calls.clear();

  fixture.controller.PowerUpTimerExpired();

  const ControllerSnapshot snapshot = fixture.controller.Snapshot();
  EXPECT_EQ(snapshot.cleaning_state, CleaningState::CleaningForward);
  EXPECT_EQ(snapshot.last_cleaner, CleanerCommand::On);
  EXPECT_EQ(fixture.log.calls, std::vector<std::string>{"cleaner.on"});
}

TEST(RvcControllerTest, EventsAreIgnoredWhenPoweredOff) {
  ControllerFixture fixture;

  fixture.controller.FrontObstacleDetected();
  fixture.controller.FrontPathClear();
  fixture.controller.DustDetected();
  fixture.controller.PowerUpTimerExpired();

  EXPECT_EQ(fixture.controller.Snapshot().power_state, PowerState::Off);
  EXPECT_TRUE(fixture.log.calls.empty());
}

TEST(RvcControllerTest, NonMatchingStateEventsAreIgnored) {
  ControllerFixture fixture;
  fixture.controller.PressPowerButton();
  fixture.log.calls.clear();

  fixture.controller.SideSensorUpdated(false);
  fixture.controller.PowerUpTimerExpired();
  fixture.controller.MotionCompleted(MotionCommand::TurnLeft);
  fixture.controller.DustDetected();

  const ControllerSnapshot snapshot = fixture.controller.Snapshot();
  EXPECT_EQ(snapshot.cleaning_state, CleaningState::CheckingFront);
  EXPECT_EQ(snapshot.last_motion, MotionCommand::Stop);
  EXPECT_EQ(snapshot.last_cleaner, CleanerCommand::Off);
  EXPECT_TRUE(fixture.log.calls.empty());
}

TEST(RvcControllerTest, MotionCompletionIgnoresUnsupportedAvoidanceMotions) {
  ControllerFixture fixture;
  fixture.controller.PressPowerButton();
  fixture.controller.FrontObstacleDetected();
  fixture.log.calls.clear();

  fixture.controller.MotionCompleted(MotionCommand::Stop);
  fixture.controller.MotionCompleted(MotionCommand::Forward);

  const ControllerSnapshot snapshot = fixture.controller.Snapshot();
  EXPECT_EQ(snapshot.cleaning_state, CleaningState::AvoidingObstacle);
  EXPECT_TRUE(fixture.log.calls.empty());
}

TEST(RvcControllerTest, CompatibilityUpdateStillMapsSensorInput) {
  ControllerFixture fixture;
  SensorInput input;
  input.front_obstacle = true;
  input.left_obstacle = true;
  input.dust_detected = true;

  const ControlCommand command = fixture.controller.Update(input);

  // [CHG-001] 전방+좌측 막힘 -> 우회전(fallback). (우측 센서 없음)
  EXPECT_EQ(command.motion, MotionCommand::TurnRight);
  EXPECT_EQ(command.cleaner, CleanerCommand::PowerUp);
}

TEST(RvcControllerTest, CompatibilityUpdateCoversAllBasicMotionChoices) {
  ControllerFixture fixture;

  SensorInput clear_path;
  ControlCommand command = fixture.controller.Update(clear_path);
  EXPECT_EQ(command.motion, MotionCommand::Forward);
  EXPECT_EQ(command.cleaner, CleanerCommand::On);

  SensorInput left_clear;
  left_clear.front_obstacle = true;
  command = fixture.controller.Update(left_clear);
  EXPECT_EQ(command.motion, MotionCommand::TurnLeft);
  EXPECT_EQ(command.cleaner, CleanerCommand::On);

  // [CHG-001] 전방+좌측 막힘 -> 우회전 (이전 Backward 분기 제거).
  SensorInput front_and_left_blocked;
  front_and_left_blocked.front_obstacle = true;
  front_and_left_blocked.left_obstacle = true;
  command = fixture.controller.Update(front_and_left_blocked);
  EXPECT_EQ(command.motion, MotionCommand::TurnRight);
  EXPECT_EQ(command.cleaner, CleanerCommand::On);
}

TEST(RvcControllerTest, ControllerStateAccessorsAndMutators) {
  ControllerState state;

  EXPECT_TRUE(state.IsPoweredOff());
  EXPECT_FALSE(state.IsPoweredOn());
  EXPECT_FALSE(state.IsAvoidingObstacle());
  EXPECT_EQ(state.CurrentPowerState(), PowerState::Off);
  EXPECT_EQ(state.CurrentCleaningState(), CleaningState::PoweredOff);

  state.SetPowerState(PowerState::On);
  state.SetCleaningState(CleaningState::AvoidingObstacle);

  EXPECT_TRUE(state.IsPoweredOn());
  EXPECT_FALSE(state.IsPoweredOff());
  EXPECT_TRUE(state.IsAvoidingObstacle());
  EXPECT_EQ(state.CurrentPowerState(), PowerState::On);
  EXPECT_EQ(state.CurrentCleaningState(), CleaningState::AvoidingObstacle);
}

// [CHG-001] 좌측 센서 하나로만 회전 방향을 결정한다.
TEST(RvcControllerTest, ObstacleAvoidancePolicySelectsLeftThenRight) {
  ObstacleAvoidancePolicy policy;

  EXPECT_EQ(policy.SelectMotion(false), MotionCommand::TurnLeft);
  EXPECT_EQ(policy.SelectMotion(true), MotionCommand::TurnRight);
}

TEST(RvcControllerTest, ToStringCoversAllEnumValues) {
  EXPECT_EQ(ToString(PowerState::Off), "Off");
  EXPECT_EQ(ToString(PowerState::On), "On");
  EXPECT_EQ(ToString(static_cast<PowerState>(99)), "Unknown");

  EXPECT_EQ(ToString(CleaningState::PoweredOff), "PoweredOff");
  EXPECT_EQ(ToString(CleaningState::CheckingFront), "CheckingFront");
  EXPECT_EQ(ToString(CleaningState::CleaningForward), "CleaningForward");
  EXPECT_EQ(ToString(CleaningState::PowerUpCleaning), "PowerUpCleaning");
  EXPECT_EQ(ToString(CleaningState::AvoidingObstacle), "AvoidingObstacle");
  EXPECT_EQ(ToString(static_cast<CleaningState>(99)), "Unknown");

  EXPECT_EQ(ToString(MotionCommand::Stop), "Stop");
  EXPECT_EQ(ToString(MotionCommand::Forward), "Forward");
  EXPECT_EQ(ToString(MotionCommand::Backward), "Backward");
  EXPECT_EQ(ToString(MotionCommand::TurnLeft), "TurnLeft");
  EXPECT_EQ(ToString(MotionCommand::TurnRight), "TurnRight");
  EXPECT_EQ(ToString(static_cast<MotionCommand>(99)), "Unknown");

  EXPECT_EQ(ToString(CleanerCommand::Off), "Off");
  EXPECT_EQ(ToString(CleanerCommand::On), "On");
  EXPECT_EQ(ToString(CleanerCommand::PowerUp), "PowerUp");
  EXPECT_EQ(ToString(static_cast<CleanerCommand>(99)), "Unknown");
}

}  // namespace
}  // namespace rvc
