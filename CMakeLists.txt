cmake_minimum_required(VERSION 3.20)

project(
  RVC_OOAD
  VERSION 0.1.0
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

enable_testing()
add_subdirectory(tests)
