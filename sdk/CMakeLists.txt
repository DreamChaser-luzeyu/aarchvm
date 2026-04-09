cmake_minimum_required(VERSION 3.16)
project(aarchvm_plugin_sdk LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(aarchvm_plugin_sdk_headers INTERFACE)
target_include_directories(aarchvm_plugin_sdk_headers
  INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(AarchvmPlugin)

add_subdirectory(examples/register_bank)
add_subdirectory(tests/abi_smoke)
add_subdirectory(tests/runtime_reset)
