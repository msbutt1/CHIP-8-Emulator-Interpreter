find_package(SDL2 REQUIRED)

add_library(platform_sdl STATIC
  platform_sdl.c
  platform_sdl.h
)
target_link_libraries(platform_sdl PRIVATE SDL2::SDL2)
target_include_directories(platform_sdl PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(chip8
  main.c
)

target_link_libraries(chip8
  PRIVATE
    chip8_core
    platform_sdl
    SDL2::SDL2
    SDL2::SDL2main
)


