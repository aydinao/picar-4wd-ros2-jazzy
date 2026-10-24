cmake_minimum_required(VERSION 3.16)
project(picar_hw LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra)
endif()

find_package(ament_cmake REQUIRED)

# Deliberately NOT found here: rclcpp, hardware_interface, pluginlib.
# If this package ever needs them, the hw/ros boundary has been breached.
find_package(PkgConfig REQUIRED)
pkg_check_modules(GPIOD REQUIRED IMPORTED_TARGET libgpiod)

# Encoder runs its edge-counting loop on its own thread.
find_package(Threads REQUIRED)

add_library(picar_hw SHARED
  src/i2c_bus.cpp
  src/hat.cpp
  src/motor.cpp
  src/encoder.cpp
  src/reset.cpp
)

target_include_directories(picar_hw PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(picar_hw PUBLIC PkgConfig::GPIOD Threads::Threads)

# Standalone smoke test: drive one wheel without ROS, controller_manager or a
# URDF. The first thing to reach for when the robot misbehaves.
add_executable(twitch tools/twitch.cpp)
target_link_libraries(twitch PRIVATE picar_hw)

# The other half of the same idea: read the encoders with nothing else running.
# Establishes which encoder is on which side, which no code can determine.
add_executable(encoder_probe tools/encoder_probe.cpp)
target_link_libraries(encoder_probe PRIVATE picar_hw)

install(DIRECTORY include/ DESTINATION include)
install(TARGETS picar_hw
  EXPORT export_picar_hw
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(TARGETS twitch encoder_probe DESTINATION lib/${PROJECT_NAME})

if(BUILD_TESTING)
  # ROS 2 style, via ament_lint_common. Copyright headers are not checked
  # (by ament_copyright or cpplint) until a license header is chosen.
  find_package(ament_lint_auto REQUIRED)
  set(AMENT_LINT_AUTO_EXCLUDE ament_cmake_copyright ament_cmake_cpplint)
  ament_lint_auto_find_test_dependencies()
  ament_cpplint(FILTERS "-legal/copyright")

  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_hat test/test_hat.cpp)
  target_include_directories(test_hat PRIVATE test)
  target_link_libraries(test_hat picar_hw)
endif()

ament_export_targets(export_picar_hw HAS_LIBRARY_TARGET)
ament_export_include_directories(include)
ament_package(CONFIG_EXTRAS "cmake/picar_hw-extras.cmake")
