#pragma once

namespace rvc {

enum class DirectionCommand {
  Forward,
  Backward,
  Left,
  Right,
};

enum class CleanCommand {
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
  DirectionCommand direction{DirectionCommand::Forward};
  CleanCommand clean{CleanCommand::On};
};

class RvcController {
 public:
  ControlCommand Update(const SensorInput& input) const;
};

}  // namespace rvc
