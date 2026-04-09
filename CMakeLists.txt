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

add_library(aarchvm_plugin_host STATIC
  src/plugin_config.cpp
  src/plugin_protocol.cpp
  src/plugin_child_runtime.cpp
  src/external_device_proxy.cpp
)

target_include_directories(aarchvm_plugin_host
  PUBLIC
    include
    sdk/include
)
target_link_libraries(aarchvm_plugin_host PUBLIC ${CMAKE_DL_LIBS})

add_executable(aarchvm
  src/main.cpp
  src/block_mmio.cpp
  src/bus.cpp
  src/external_device_proxy.cpp
  src/framebuffer_sdl.cpp
  src/ram.cpp
  src/uart_pl011.cpp
  src/gicv3.cpp
  src/generic_timer.cpp
  src/plugin_child_runtime.cpp
  src/plugin_config.cpp
  src/plugin_protocol.cpp
  src/system_registers.cpp
  src/cpu.cpp
  src/soc.cpp
  src/perf_mailbox.cpp
  src/pl050_kmi.cpp
  src/rtc_pl031.cpp
  src/virtio_blk_mmio.cpp
)

target_include_directories(aarchvm PRIVATE include sdk/include ${SDL2_INCLUDE_DIRS})
target_link_directories(aarchvm PRIVATE ${SDL2_LIBRARY_DIRS})
target_link_libraries(aarchvm PRIVATE ${SDL2_LIBRARIES} ${CMAKE_DL_LIBS})
target_compile_options(aarchvm PRIVATE ${SDL2_CFLAGS_OTHER})

add_executable(aarchvm_unit_cpu_cache_consistency
  tests/unit_cpu_cache_consistency.cpp
  src/block_mmio.cpp
  src/bus.cpp
  src/perf_mailbox.cpp
  src/ram.cpp
  src/uart_pl011.cpp
  src/virtio_blk_mmio.cpp
  src/gicv3.cpp
  src/generic_timer.cpp
  src/system_registers.cpp
  src/cpu.cpp
)

target_include_directories(aarchvm_unit_cpu_cache_consistency PRIVATE include)

add_executable(aarchvm_unit_external_plugin
  tests/unit_external_plugin.cpp
)

target_include_directories(aarchvm_unit_external_plugin PRIVATE include sdk/include)
target_link_libraries(aarchvm_unit_external_plugin PRIVATE aarchvm_plugin_host)
target_compile_definitions(aarchvm_unit_external_plugin PRIVATE
  AARCHVM_SDK_BUILD_DIR="${CMAKE_BINARY_DIR}/sdk"
)

if (MSVC)
  target_compile_options(aarchvm PRIVATE /W4 /permissive-)
  target_compile_options(aarchvm_unit_cpu_cache_consistency PRIVATE /W4 /permissive-)
  target_compile_options(aarchvm_unit_external_plugin PRIVATE /W4 /permissive-)
  target_compile_options(aarchvm_plugin_host PRIVATE /W4 /permissive-)
else()
  target_compile_options(aarchvm PRIVATE -Wall -Wextra -Wpedantic -Werror)
  target_compile_options(aarchvm_unit_cpu_cache_consistency PRIVATE -Wall -Wextra -Wpedantic -Werror)
  target_compile_options(aarchvm_unit_external_plugin PRIVATE -Wall -Wextra -Wpedantic -Werror)
  target_compile_options(aarchvm_plugin_host PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
