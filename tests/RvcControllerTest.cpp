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

TEST(RvcControllerTest, AvoidanceTurnsLeftWhenLeftIsClear) {
  ControllerFixture fixture;
  fixture.controller.PressPowerButton();
  fixture.controller.FrontObstacleDetected();
  fixture.log.calls.clear();

  fixture.controller.SideSensorUpdated(false, true);

  EXPECT_EQ(fixture.controller.Snapshot().last_motion, MotionCommand::TurnLeft);
  EXPECT_EQ(fixture.log.calls, std::vector<std::string>{"motor.turnLeft"});
}

TEST(RvcControllerTest, AvoidanceTurnsRightWhenLeftBlockedAndRightClear) {
  ControllerFixture fixture;
  fixture.controller.PressPowerButton();
  fixture.controller.FrontObstacleDetected();
  fixture.log.calls.clear();

  fixture.controller.SideSensorUpdated(true, false);

  EXPECT_EQ(fixture.controller.Snapshot().last_motion, MotionCommand::TurnRight);
  EXPECT_EQ(fixture.log.calls, std::vector<std::string>{"motor.turnRight"});
}

TEST(RvcControllerTest, AvoidanceMovesBackwardWhenBothSidesBlocked) {
  ControllerFixture fixture;
  fixture.controller.PressPowerButton();
  fixture.controller.FrontObstacleDetected();
  fixture.log.calls.clear();

  fixture.controller.SideSensorUpdated(true, true);

  EXPECT_EQ(fixture.controller.Snapshot().last_motion, MotionCommand::Backward);
  EXPECT_EQ(fixture.log.calls, std::vector<std::string>{"motor.backward"});
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

  fixture.controller.SideSensorUpdated(false, false);
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

TEST(RvcControllerTest, TurnRightCompletionAlsoResumesForwardCleaning) {
  ControllerFixture fixture;
  fixture.controller.PressPowerButton();
  fixture.controller.FrontObstacleDetected();
  fixture.log.calls.clear();

  fixture.controller.MotionCompleted(MotionCommand::TurnRight);

  const ControllerSnapshot snapshot = fixture.controller.Snapshot();
  EXPECT_EQ(snapshot.cleaning_state, CleaningState::CleaningForward);
  EXPECT_EQ(snapshot.last_motion, MotionCommand::Forward);
  EXPECT_EQ(snapshot.last_cleaner, CleanerCommand::On);
  EXPECT_EQ(fixture.log.calls,
            (std::vector<std::string>{"motor.forward", "cleaner.on"}));
}

TEST(RvcControllerTest, CompatibilityUpdateStillMapsSensorInput) {
  ControllerFixture fixture;
  SensorInput input;
  input.front_obstacle = true;
  input.left_obstacle = true;
  input.dust_detected = true;

  const ControlCommand command = fixture.controller.Update(input);

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

  SensorInput both_sides_blocked;
  both_sides_blocked.front_obstacle = true;
  both_sides_blocked.left_obstacle = true;
  both_sides_blocked.right_obstacle = true;
  command = fixture.controller.Update(both_sides_blocked);
  EXPECT_EQ(command.motion, MotionCommand::Backward);
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

TEST(RvcControllerTest, ObstacleAvoidancePolicySelectsLeftFirstThenRightThenBackward) {
  ObstacleAvoidancePolicy policy;

  EXPECT_EQ(policy.SelectMotion(false, false), MotionCommand::TurnLeft);
  EXPECT_EQ(policy.SelectMotion(false, true), MotionCommand::TurnLeft);
  EXPECT_EQ(policy.SelectMotion(true, false), MotionCommand::TurnRight);
  EXPECT_EQ(policy.SelectMotion(true, true), MotionCommand::Backward);
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
