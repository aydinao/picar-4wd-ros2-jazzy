# CMake does not export imported targets created by pkg_check_modules, but
# picar_hw's exported link interface names PkgConfig::GPIOD. Every consumer
# must therefore recreate it. ament runs this file automatically as part of
# find_package(picar_hw).
find_package(PkgConfig REQUIRED)
if(NOT TARGET PkgConfig::GPIOD)
  pkg_check_modules(GPIOD REQUIRED IMPORTED_TARGET libgpiod)
endif()
