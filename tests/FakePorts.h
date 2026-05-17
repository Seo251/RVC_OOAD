#pragma once

#include "rvc/RvcController.h"

#include <string>
#include <vector>

namespace rvc::test {

struct CallLog {
  std::vector<std::string> calls;

  void Add(const std::string& call) { calls.push_back(call); }
};

class FakeMotorPort final : public MotorPort {
 public:
  explicit FakeMotorPort(CallLog& log) : log_(log) {}

  void Forward() override { log_.Add("motor.forward"); }
  void Backward() override { log_.Add("motor.backward"); }
  void TurnLeft() override { log_.Add("motor.turnLeft"); }
  void TurnRight() override { log_.Add("motor.turnRight"); }
  void Stop() override { log_.Add("motor.stop"); }

 private:
  CallLog& log_;
};

class FakeCleanerPort final : public CleanerPort {
 public:
  explicit FakeCleanerPort(CallLog& log) : log_(log) {}

  void TurnOn() override { log_.Add("cleaner.on"); }
  void TurnOff() override { log_.Add("cleaner.off"); }
  void PowerUp() override { log_.Add("cleaner.powerUp"); }

 private:
  CallLog& log_;
};

class FakeTimerPort final : public TimerPort {
 public:
  explicit FakeTimerPort(CallLog& log) : log_(log) {}

  void StartPowerUpTimer(int seconds) override {
    log_.Add("timer.start:" + std::to_string(seconds));
  }

  void RestartPowerUpTimer(int seconds) override {
    log_.Add("timer.restart:" + std::to_string(seconds));
  }

  void CancelPowerUpTimer() override { log_.Add("timer.cancel"); }

 private:
  CallLog& log_;
};

class FakeFrontSensorPort final : public FrontSensorPort {
 public:
  explicit FakeFrontSensorPort(CallLog& log) : log_(log) {}

  void RequestFrontCheck() override { log_.Add("front.request"); }

 private:
  CallLog& log_;
};

class FakeSideSensorPort final : public SideSensorPort {
 public:
  explicit FakeSideSensorPort(CallLog& log) : log_(log) {}

  void RequestSideCheck() override { log_.Add("side.request"); }

 private:
  CallLog& log_;
};

struct ControllerFixture {
  CallLog log;
  FakeMotorPort motor{log};
  FakeCleanerPort cleaner{log};
  FakeTimerPort timer{log};
  FakeFrontSensorPort front_sensor{log};
  FakeSideSensorPort side_sensor{log};
  RvcController controller{motor, cleaner, timer, front_sensor, side_sensor};
};

}  // namespace rvc::test
