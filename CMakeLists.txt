cmake_minimum_required(VERSION 3.10)
project(picar4wd CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build the vendored I2CPP as a static lib alongside your code
add_subdirectory(libs/I2CPP)

add_executable(picar_test
    src/main.cpp
    src/device/4wdhat.cpp
)

target_include_directories(picar_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/libs/I2CPP/include   # I2CPP's CMake doesn't propagate this
)

target_link_libraries(picar_test PRIVATE i2cpp)