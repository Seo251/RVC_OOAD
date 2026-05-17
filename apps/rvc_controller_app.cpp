#include "RvcSimulatorBridge.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>

int main(int argc, char** argv) {
  std::uint16_t port = 5050;
  if (argc > 1) {
    const int parsed_port = std::atoi(argv[1]);
    if (parsed_port > 0 && parsed_port <= 65535) {
      port = static_cast<std::uint16_t>(parsed_port);
    }
  }

  rvc::RvcSimulatorBridge bridge(port);
  return bridge.Run();
}
