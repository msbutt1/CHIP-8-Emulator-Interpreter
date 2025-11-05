add_executable(chip8_tests
  test_sample.c
)

target_link_libraries(chip8_tests
  PRIVATE
    chip8_core
    unity
)

add_test(NAME chip8_tests COMMAND chip8_tests)


