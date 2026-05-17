#pragma once

#include "rvc/RvcController.h"

#include <cstdint>
#include <string>
#include <vector>

namespace rvc {

class RecordingMotorPort final : public MotorPort {
 public:
  void Forward() override;
  void Backward() override;
  void TurnLeft() override;
  void TurnRight() override;
  void Stop() override;

  const std::vector<std::string>& Events() const;

 private:
  void Record(MotionCommand command);

  std::vector<std::string> events_;
};

class RecordingCleanerPort final : public CleanerPort {
 public:
  void TurnOn() override;
  void TurnOff() override;
  void PowerUp() override;

  const std::vector<std::string>& Events() const;

 private:
  void Record(CleanerCommand command);

  std::vector<std::string> events_;
};

class RecordingTimerPort final : public TimerPort {
 public:
  void StartPowerUpTimer(int seconds) override;
  void RestartPowerUpTimer(int seconds) override;
  void CancelPowerUpTimer() override;

  const std::vector<std::string>& Events() const;

 private:
  std::vector<std::string> events_;
};

class RecordingFrontSensorPort final : public FrontSensorPort {
 public:
  void RequestFrontCheck() override;
  const std::vector<std::string>& Events() const;

 private:
  std::vector<std::string> events_;
};

class RecordingSideSensorPort final : public SideSensorPort {
 public:
  void RequestSideCheck() override;
  const std::vector<std::string>& Events() const;

 private:
  std::vector<std::string> events_;
};

class RvcSimulatorBridge {
 public:
  explicit RvcSimulatorBridge(std::uint16_t port);

  int Run();

 private:
  std::string HandleCommand(const std::string& line);
  std::string BuildSnapshotJson() const;

  std::uint16_t port_;
  RecordingMotorPort motor_;
  RecordingCleanerPort cleaner_;
  RecordingTimerPort timer_;
  RecordingFrontSensorPort front_sensor_;
  RecordingSideSensorPort side_sensor_;
  RvcController controller_;
};

}  // namespace rvc
