add_library(chip8_core STATIC
  chip8.c
  opcodes.c
)

target_include_directories(chip8_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)


