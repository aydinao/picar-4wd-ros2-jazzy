cmake_minimum_required(VERSION 3.10)
project(picar4wd CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GPIOD REQUIRED IMPORTED_TARGET libgpiod)

add_subdirectory(libs/I2CPP)

set(COMMON_INCLUDES
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/libs/I2CPP/include
)

add_executable(picar_test
    src/main.cpp
    src/device/4WDHAT.cpp
)
target_include_directories(picar_test PRIVATE ${COMMON_INCLUDES})
target_link_libraries(picar_test PRIVATE i2cpp PkgConfig::GPIOD)

add_executable(test_motor
    src/test_motor.cpp
    src/motor.cpp
    src/device/4WDHAT.cpp
)
target_include_directories(test_motor PRIVATE ${COMMON_INCLUDES})
target_link_libraries(test_motor PRIVATE i2cpp PkgConfig::GPIOD)
