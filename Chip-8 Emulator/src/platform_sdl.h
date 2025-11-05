#ifndef PLATFORM_SDL_H
#define PLATFORM_SDL_H

#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>

struct Chip8;

typedef struct PlatformSDL {
  SDL_Window* window;
  SDL_Renderer* renderer;
  SDL_Texture* texture;   // grayscale 64x32
  SDL_AudioDeviceID audio_device;
  int scale;              // integer scale
  bool vsync;
  struct Chip8* chip8;    // for audio beep state
} PlatformSDL;

bool platform_sdl_init(PlatformSDL* p, const char* title, int scale, bool vsync, struct Chip8* c8);
void platform_sdl_shutdown(PlatformSDL* p);

// Update texture from framebuffer data (64x32 bytes: 0/1), then render
void platform_sdl_render(PlatformSDL* p, const uint8_t* framebuffer);

// Install a 60Hz SDL timer that pushes a user event; main loop should handle it to call chip8_tick_60hz
Uint32 platform_sdl_add_60hz_timer(void* userdata);

#endif // PLATFORM_SDL_H


