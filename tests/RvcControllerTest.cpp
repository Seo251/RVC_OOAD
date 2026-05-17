#include "rvc/RvcController.h"

#include <gtest/gtest.h>

namespace rvc {
namespace {

TEST(RvcControllerTest, GoesForwardAndCleansByDefault) {
  const RvcController controller;

  const ControlCommand command = controller.Update(SensorInput{});

  EXPECT_EQ(command.direction, DirectionCommand::Forward);
  EXPECT_EQ(command.clean, CleanCommand::On);
}

TEST(RvcControllerTest, PowersUpCleaningWhenDustIsDetected) {
  const RvcController controller;
  SensorInput input;
  input.dust_detected = true;

  const ControlCommand command = controller.Update(input);

  EXPECT_EQ(command.direction, DirectionCommand::Forward);
  EXPECT_EQ(command.clean, CleanCommand::PowerUp);
}

TEST(RvcControllerTest, TurnsLeftWhenFrontIsBlockedAndLeftIsClear) {
  const RvcController controller;
  SensorInput input;
  input.front_obstacle = true;

  const ControlCommand command = controller.Update(input);

  EXPECT_EQ(command.direction, DirectionCommand::Left);
  EXPECT_EQ(command.clean, CleanCommand::On);
}

TEST(RvcControllerTest, TurnsRightWhenFrontAndLeftAreBlocked) {
  const RvcController controller;
  SensorInput input;
  input.front_obstacle = true;
  input.left_obstacle = true;

  const ControlCommand command = controller.Update(input);

  EXPECT_EQ(command.direction, DirectionCommand::Right);
  EXPECT_EQ(command.clean, CleanCommand::On);
}

TEST(RvcControllerTest, MovesBackwardWhenFrontLeftAndRightAreBlocked) {
  const RvcController controller;
  SensorInput input;
  input.front_obstacle = true;
  input.left_obstacle = true;
  input.right_obstacle = true;

  const ControlCommand command = controller.Update(input);

  EXPECT_EQ(command.direction, DirectionCommand::Backward);
  EXPECT_EQ(command.clean, CleanCommand::On);
}

}  // namespace
}  // namespace rvc
