cmake_minimum_required(VERSION 3.20)

project(
  RVC_OOAD
  VERSION 0.1.0
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

add_library(rvc_controller
  src/RvcController.cpp
)

target_include_directories(rvc_controller
  PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(rvc_controller PUBLIC cxx_std_17)

if(MSVC)
  target_compile_options(rvc_controller PRIVATE /W4 /permissive-)
else()
  target_compile_options(rvc_controller PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(rvc_simulator_bridge
  simulator/RvcSimulatorBridge.cpp
)

target_include_directories(rvc_simulator_bridge
  PUBLIC
    ${PROJECT_SOURCE_DIR}/simulator
)

target_link_libraries(rvc_simulator_bridge
  PUBLIC
    rvc_controller
)

if(WIN32)
  target_link_libraries(rvc_simulator_bridge PRIVATE ws2_32)
endif()

add_executable(rvc_controller_app
  apps/rvc_controller_app.cpp
)

target_link_libraries(rvc_controller_app
  PRIVATE
    rvc_simulator_bridge
)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
