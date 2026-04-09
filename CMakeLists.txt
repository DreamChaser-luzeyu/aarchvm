cmake_minimum_required(VERSION 3.16)
project(aarchvm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

add_executable(aarchvm
  src/main.cpp
  src/block_mmio.cpp
  src/bus.cpp
  src/framebuffer_sdl.cpp
  src/ram.cpp
  src/uart_pl011.cpp
  src/gicv3.cpp
  src/generic_timer.cpp
  src/system_registers.cpp
  src/cpu.cpp
  src/soc.cpp
  src/perf_mailbox.cpp
  src/pl050_kmi.cpp
  src/rtc_pl031.cpp
  src/virtio_blk_mmio.cpp
  src/virtio_net_mmio.cpp
)

target_include_directories(aarchvm PRIVATE include ${SDL2_INCLUDE_DIRS})
target_link_directories(aarchvm PRIVATE ${SDL2_LIBRARY_DIRS})
target_link_libraries(aarchvm PRIVATE ${SDL2_LIBRARIES})
target_compile_options(aarchvm PRIVATE ${SDL2_CFLAGS_OTHER})

add_executable(aarchvm_unit_cpu_cache_consistency
  tests/unit_cpu_cache_consistency.cpp
  src/block_mmio.cpp
  src/bus.cpp
  src/perf_mailbox.cpp
  src/ram.cpp
  src/uart_pl011.cpp
  src/virtio_blk_mmio.cpp
  src/virtio_net_mmio.cpp
  src/gicv3.cpp
  src/generic_timer.cpp
  src/system_registers.cpp
  src/cpu.cpp
)

target_include_directories(aarchvm_unit_cpu_cache_consistency PRIVATE include)

if (MSVC)
  target_compile_options(aarchvm PRIVATE /W4 /permissive-)
  target_compile_options(aarchvm_unit_cpu_cache_consistency PRIVATE /W4 /permissive-)
else()
  target_compile_options(aarchvm PRIVATE -Wall -Wextra -Wpedantic -Werror)
  target_compile_options(aarchvm_unit_cpu_cache_consistency PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
