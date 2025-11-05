cmake_minimum_required(VERSION 3.20)

project(chip8-c VERSION 0.1.0 LANGUAGES C)

# C standard and global definitions
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Expose version macro to all targets
add_compile_definitions(CHIP8_VERSION="${PROJECT_VERSION}")

# Global warnings/optimization
if(MSVC)
  add_compile_options(/W4 /WX /O2)
else()
  add_compile_options(-Wall -Wextra -Werror -O2 -pedantic)
endif()

# Sanitizers in Debug for GCC/Clang
if(NOT MSVC)
  add_compile_options($<$<CONFIG:Debug>:-fsanitize=address,undefined -fno-omit-frame-pointer>)
  add_link_options($<$<CONFIG:Debug>:-fsanitize=address,undefined>)
endif()

# Put third_party content here for FetchContent
set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/third_party")

# Testing framework helper (Unity)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(CTest)
enable_testing()
include(Unity)

add_subdirectory(core)
add_subdirectory(src)
add_subdirectory(tests)


