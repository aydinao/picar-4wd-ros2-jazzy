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

# Vendored I2C wrapper lives at the repo root. Its own CMakeLists declares
# cmake_minimum_required(VERSION 3.4), which CMake >= 4 refuses, so we compile
# the two sources we use directly rather than add_subdirectory().
set(I2CPP_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../libs/I2CPP)

add_library(picar_hw SHARED
  src/hat.cpp
  src/motor.cpp
  ${I2CPP_ROOT}/src/i2cpp/i2cpp.cpp
  ${I2CPP_ROOT}/src/i2cpp/device.cpp
)

target_include_directories(picar_hw PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${I2CPP_ROOT}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(picar_hw PUBLIC PkgConfig::GPIOD)

# Standalone smoke test: drive one wheel without ROS, controller_manager or a
# URDF. The first thing to reach for when the robot misbehaves.
add_executable(twitch tools/twitch.cpp)
target_link_libraries(twitch PRIVATE picar_hw)

install(DIRECTORY include/ DESTINATION include)
install(TARGETS picar_hw
  EXPORT export_picar_hw
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(TARGETS twitch DESTINATION lib/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_hat test/test_hat.cpp)
  target_include_directories(test_hat PRIVATE test)
  target_link_libraries(test_hat picar_hw)
endif()

ament_export_targets(export_picar_hw HAS_LIBRARY_TARGET)
ament_export_include_directories(include)
ament_package(CONFIG_EXTRAS "cmake/picar_hw-extras.cmake")
