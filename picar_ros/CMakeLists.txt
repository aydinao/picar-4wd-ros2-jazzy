cmake_minimum_required(VERSION 3.16)
project(picar_ros LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra)
endif()

find_package(ament_cmake REQUIRED)
find_package(picar_hw REQUIRED)
find_package(hardware_interface REQUIRED)
find_package(pluginlib REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)

# Thin by design: this library should contain the SystemInterface and nothing
# else. Anything that talks to the HAT belongs in picar_hw.
add_library(picar_ros SHARED src/picar_system.cpp)

target_include_directories(picar_ros PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

ament_target_dependencies(picar_ros PUBLIC
  picar_hw
  hardware_interface
  pluginlib
  rclcpp
  rclcpp_lifecycle
)

pluginlib_export_plugin_description_file(hardware_interface picar_ros.xml)

install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY description bringup DESTINATION share/${PROJECT_NAME})
install(TARGETS picar_ros
  EXPORT export_picar_ros
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

ament_export_targets(export_picar_ros HAS_LIBRARY_TARGET)
ament_export_dependencies(picar_hw hardware_interface pluginlib rclcpp rclcpp_lifecycle)
ament_package()
