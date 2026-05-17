#include "RvcSimulatorBridge.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace rvc {

namespace {

#ifdef _WIN32
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;

void CloseSocket(SocketHandle socket) {
  closesocket(socket);
}
#else
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;

void CloseSocket(SocketHandle socket) {
  close(socket);
}
#endif

bool Contains(const std::string& text, const std::string& token) {
  return text.find(token) != std::string::npos;
}

bool ReadBoolField(const std::string& text, const std::string& field) {
  const std::string true_pattern = "\"" + field + "\":true";
  const std::string spaced_true_pattern = "\"" + field + "\": true";
  return Contains(text, true_pattern) || Contains(text, spaced_true_pattern);
}

MotionCommand ReadMotionField(const std::string& text) {
  if (Contains(text, "TurnLeft")) {
    return MotionCommand::TurnLeft;
  }
  if (Contains(text, "TurnRight")) {
    return MotionCommand::TurnRight;
  }
  if (Contains(text, "Backward")) {
    return MotionCommand::Backward;
  }
  if (Contains(text, "Forward")) {
    return MotionCommand::Forward;
  }
  return MotionCommand::Stop;
}

std::string JsonArray(const std::vector<std::string>& values) {
  std::ostringstream output;
  output << "[";
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index > 0) {
      output << ",";
    }
    output << "\"" << values[index] << "\"";
  }
  output << "]";
  return output.str();
}

}  // namespace

void RecordingMotorPort::Forward() {
  Record(MotionCommand::Forward);
}

void RecordingMotorPort::Backward() {
  Record(MotionCommand::Backward);
}

void RecordingMotorPort::TurnLeft() {
  Record(MotionCommand::TurnLeft);
}

void RecordingMotorPort::TurnRight() {
  Record(MotionCommand::TurnRight);
}

void RecordingMotorPort::Stop() {
  Record(MotionCommand::Stop);
}

const std::vector<std::string>& RecordingMotorPort::Events() const {
  return events_;
}

void RecordingMotorPort::Record(const MotionCommand command) {
  events_.push_back(ToString(command));
}

void RecordingCleanerPort::TurnOn() {
  Record(CleanerCommand::On);
}

void RecordingCleanerPort::TurnOff() {
  Record(CleanerCommand::Off);
}

void RecordingCleanerPort::PowerUp() {
  Record(CleanerCommand::PowerUp);
}

const std::vector<std::string>& RecordingCleanerPort::Events() const {
  return events_;
}

void RecordingCleanerPort::Record(const CleanerCommand command) {
  events_.push_back(ToString(command));
}

void RecordingTimerPort::StartPowerUpTimer(const int seconds) {
  events_.push_back("Start:" + std::to_string(seconds));
}

void RecordingTimerPort::RestartPowerUpTimer(const int seconds) {
  events_.push_back("Restart:" + std::to_string(seconds));
}

void RecordingTimerPort::CancelPowerUpTimer() {
  events_.push_back("Cancel");
}

const std::vector<std::string>& RecordingTimerPort::Events() const {
  return events_;
}

void RecordingFrontSensorPort::RequestFrontCheck() {
  events_.push_back("RequestFrontCheck");
}

const std::vector<std::string>& RecordingFrontSensorPort::Events() const {
  return events_;
}

void RecordingSideSensorPort::RequestSideCheck() {
  events_.push_back("RequestSideCheck");
}

const std::vector<std::string>& RecordingSideSensorPort::Events() const {
  return events_;
}

RvcSimulatorBridge::RvcSimulatorBridge(const std::uint16_t port)
    : port_(port),
      controller_(motor_, cleaner_, timer_, front_sensor_, side_sensor_) {}

int RvcSimulatorBridge::Run() {
#ifdef _WIN32
  WSADATA wsa_data;
  if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
    std::cerr << "WSAStartup failed\n";
    return 1;
  }
#endif

  const SocketHandle server_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (server_socket == kInvalidSocket) {
    std::cerr << "socket creation failed\n";
    return 1;
  }

  int enabled = 1;
  setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR,
             reinterpret_cast<const char*>(&enabled), sizeof(enabled));

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  address.sin_port = htons(port_);

  if (bind(server_socket, reinterpret_cast<sockaddr*>(&address),
           sizeof(address)) < 0) {
    std::cerr << "bind failed\n";
    CloseSocket(server_socket);
    return 1;
  }

  if (listen(server_socket, 1) < 0) {
    std::cerr << "listen failed\n";
    CloseSocket(server_socket);
    return 1;
  }

  std::cout << "RVC simulator bridge listening on port " << port_ << "\n";

  while (true) {
    const SocketHandle client_socket =
        accept(server_socket, nullptr, nullptr);
    if (client_socket == kInvalidSocket) {
      continue;
    }

    std::string pending;
    char buffer[512];
    while (true) {
      const int received =
          recv(client_socket, buffer, static_cast<int>(sizeof(buffer)), 0);
      if (received <= 0) {
        break;
      }

      pending.append(buffer, buffer + received);
      std::size_t newline = pending.find('\n');
      while (newline != std::string::npos) {
        const std::string line = pending.substr(0, newline);
        pending.erase(0, newline + 1);
        const std::string response = HandleCommand(line) + "\n";
        send(client_socket, response.c_str(), static_cast<int>(response.size()),
             0);
        newline = pending.find('\n');
      }
    }

    CloseSocket(client_socket);
  }
}

std::string RvcSimulatorBridge::HandleCommand(const std::string& line) {
  if (Contains(line, "pressPowerButton")) {
    controller_.PressPowerButton();
  } else if (Contains(line, "frontObstacleDetected")) {
    controller_.FrontObstacleDetected();
  } else if (Contains(line, "frontPathClear")) {
    controller_.FrontPathClear();
  } else if (Contains(line, "sideSensorUpdated")) {
    controller_.SideSensorUpdated(ReadBoolField(line, "leftBlocked"),
                                  ReadBoolField(line, "rightBlocked"));
  } else if (Contains(line, "dustDetected")) {
    controller_.DustDetected();
  } else if (Contains(line, "powerUpTimerExpired")) {
    controller_.PowerUpTimerExpired();
  } else if (Contains(line, "motionCompleted")) {
    controller_.MotionCompleted(ReadMotionField(line));
  }

  return BuildSnapshotJson();
}

std::string RvcSimulatorBridge::BuildSnapshotJson() const {
  const ControllerSnapshot snapshot = controller_.Snapshot();
  std::ostringstream output;
  output << "{";
  output << "\"powerState\":\"" << ToString(snapshot.power_state) << "\",";
  output << "\"cleaningState\":\"" << ToString(snapshot.cleaning_state)
         << "\",";
  output << "\"lastMotion\":\"" << ToString(snapshot.last_motion) << "\",";
  output << "\"lastCleaner\":\"" << ToString(snapshot.last_cleaner) << "\",";
  output << "\"motorEvents\":" << JsonArray(motor_.Events()) << ",";
  output << "\"cleanerEvents\":" << JsonArray(cleaner_.Events()) << ",";
  output << "\"timerEvents\":" << JsonArray(timer_.Events()) << ",";
  output << "\"frontSensorEvents\":" << JsonArray(front_sensor_.Events())
         << ",";
  output << "\"sideSensorEvents\":" << JsonArray(side_sensor_.Events());
  output << "}";
  return output.str();
}

}  // namespace rvc
