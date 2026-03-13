cmake_minimum_required(VERSION 3.16)
project(aarchvm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_executable(aarchvm
  src/main.cpp
  src/bus.cpp
    src/ram.cpp
  src/uart_pl011.cpp
  src/gicv3.cpp
  src/generic_timer.cpp
  src/system_registers.cpp
  src/cpu.cpp
  src/soc.cpp
  src/perf_mailbox.cpp
)

target_include_directories(aarchvm PRIVATE include)

if (MSVC)
  target_compile_options(aarchvm PRIVATE /W4 /permissive-)
else()
  target_compile_options(aarchvm PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
