cmake_minimum_required(VERSION 3.16)
project(picar_4wd_hardware LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra)
endif()

find_package(ament_cmake REQUIRED)
find_package(hardware_interface REQUIRED)
find_package(pluginlib REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GPIOD REQUIRED IMPORTED_TARGET libgpiod)

# Vendored I2C wrapper (git submodule). Its own CMakeLists declares
# cmake_minimum_required(VERSION 3.4), which CMake >= 4 refuses outright, so we
# compile the two sources we need directly into this target instead of calling
# add_subdirectory(). This also sidesteps linking a non-PIC static library into
# a shared plugin. We do not use its pca9555/ads1115 device classes.
set(I2CPP_SOURCES
  libs/I2CPP/src/i2cpp/i2cpp.cpp
  libs/I2CPP/src/i2cpp/device.cpp
)

# ---------------------------------------------------------------------------
# The ros2_control plugin. Loaded by controller_manager at runtime, which is
# why it is SHARED and why there is no main() anywhere in this package.
# ---------------------------------------------------------------------------
add_library(picar_4wd_hardware SHARED
  hardware/picar_system.cpp
  hardware/hat.cpp
  hardware/motor.cpp
  ${I2CPP_SOURCES}
)

target_include_directories(picar_4wd_hardware PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/hardware/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/I2CPP/include>
  $<INSTALL_INTERFACE:include/picar_4wd_hardware>
)

target_link_libraries(picar_4wd_hardware PUBLIC PkgConfig::GPIOD)

ament_target_dependencies(picar_4wd_hardware PUBLIC
  hardware_interface
  pluginlib
  rclcpp
  rclcpp_lifecycle
)

pluginlib_export_plugin_description_file(hardware_interface picar_4wd_hardware.xml)

install(DIRECTORY hardware/include/ DESTINATION include/picar_4wd_hardware)
install(DIRECTORY description bringup DESTINATION share/picar_4wd_hardware)
install(TARGETS picar_4wd_hardware
  EXPORT export_picar_4wd_hardware
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

ament_export_targets(export_picar_4wd_hardware HAS_LIBRARY_TARGET)
ament_export_dependencies(hardware_interface pluginlib rclcpp rclcpp_lifecycle)
ament_package()
