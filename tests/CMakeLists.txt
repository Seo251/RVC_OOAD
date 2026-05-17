include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(rvc_controller_tests
  RvcControllerTest.cpp
)

target_link_libraries(rvc_controller_tests
  PRIVATE
    rvc_controller
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(rvc_controller_tests)
