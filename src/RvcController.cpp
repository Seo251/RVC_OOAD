#include "rvc/RvcController.h"

namespace rvc {

ControlCommand RvcController::Update(const SensorInput& input) const {
  ControlCommand command;

  if (input.dust_detected) {
    command.clean = CleanCommand::PowerUp;
  }

  if (!input.front_obstacle) {
    command.direction = DirectionCommand::Forward;
    return command;
  }

  if (!input.left_obstacle) {
    command.direction = DirectionCommand::Left;
    return command;
  }

  if (!input.right_obstacle) {
    command.direction = DirectionCommand::Right;
    return command;
  }

  command.direction = DirectionCommand::Backward;
  return command;
}

}  // namespace rvc
