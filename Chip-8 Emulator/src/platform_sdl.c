#include "platform_sdl.h"

#include <SDL.h>
#include <string.h>

#include "../core/chip8.h"

#define FB_WIDTH 64
#define FB_HEIGHT 32

static void audio_callback(void* userdata, Uint8* stream, int len) {
  PlatformSDL* p = (PlatformSDL*)userdata;
  memset(stream, 0, (size_t)len);
  if (!p || !p->chip8) return;
  // Simple square wave when sound_timer > 0
  // We can't read timer directly via API; we use snapshot
  Chip8Snapshot s = {0};
  chip8_get_snapshot(p->chip8, &s);
  if (s.sound_timer == 0) return;

  // Generate a fixed-frequency square wave ~440Hz
  const int sample_rate = 48000; // SDL default often 48k
  static int phase = 0;
  int16_t* out = (int16_t*)stream;
  int frames = len / (int)sizeof(int16_t);
  int period = sample_rate / 440;
  for (int i = 0; i < frames; ++i) {
    int16_t sample = (phase < period / 2) ? 12000 : -12000;
    out[i] = sample;
    phase = (phase + 1) % period;
  }
}

bool platform_sdl_init(PlatformSDL* p, const char* title, int scale, bool vsync, struct Chip8* c8) {
  if (!p) return false;
  memset(p, 0, sizeof(*p));
  p->scale = (scale > 0) ? scale : 10;
  p->vsync = vsync;
  p->chip8 = c8;

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
    return false;
  }

  int win_w = FB_WIDTH * p->scale;
  int win_h = FB_HEIGHT * p->scale;
  p->window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, win_w, win_h, SDL_WINDOW_SHOWN);
  if (!p->window) return false;

  Uint32 renderer_flags = SDL_RENDERER_ACCELERATED | (vsync ? SDL_RENDERER_PRESENTVSYNC : 0);
  p->renderer = SDL_CreateRenderer(p->window, -1, renderer_flags);
  if (!p->renderer) return false;

  p->texture = SDL_CreateTexture(p->renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, FB_WIDTH, FB_HEIGHT);
  if (!p->texture) return false;

  // Setup audio
  SDL_AudioSpec want, have;
  SDL_zero(want);
  want.freq = 48000;
  want.format = AUDIO_S16SYS;
  want.channels = 1;
  want.samples = 1024;
  want.callback = audio_callback;
  want.userdata = p;
  p->audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
  if (p->audio_device != 0) SDL_PauseAudioDevice(p->audio_device, 0);

  return true;
}

void platform_sdl_shutdown(PlatformSDL* p) {
  if (!p) return;
  if (p->audio_device) SDL_CloseAudioDevice(p->audio_device);
  if (p->texture) SDL_DestroyTexture(p->texture);
  if (p->renderer) SDL_DestroyRenderer(p->renderer);
  if (p->window) SDL_DestroyWindow(p->window);
  SDL_Quit();
}

void platform_sdl_render(PlatformSDL* p, const uint8_t* framebuffer) {
  if (!p || !p->renderer || !p->texture || !framebuffer) return;

  // Convert 64x32 1bpp to RGBA8888 grayscale
  uint32_t pixels[FB_WIDTH * FB_HEIGHT];
  for (int i = 0; i < FB_WIDTH * FB_HEIGHT; ++i) {
    uint8_t v = framebuffer[i] ? 0xFF : 0x00;
    pixels[i] = (0xFFu << 24) | (v << 16) | (v << 8) | v;
  }
  SDL_UpdateTexture(p->texture, NULL, pixels, FB_WIDTH * (int)sizeof(uint32_t));

  SDL_RenderClear(p->renderer);
  SDL_Rect dst = {0, 0, FB_WIDTH * p->scale, FB_HEIGHT * p->scale};
  SDL_RenderCopy(p->renderer, p->texture, NULL, &dst);
  SDL_RenderPresent(p->renderer);
}

static Uint32 timer_60hz_cb(Uint32 interval, void* userdata) {
  SDL_Event e;
  SDL_zero(e);
  e.type = SDL_USEREVENT;
  e.user.code = 1; // 60Hz tick code
  e.user.data1 = userdata;
  SDL_PushEvent(&e);
  return interval; // reschedule
}

Uint32 platform_sdl_add_60hz_timer(void* userdata) {
  return SDL_AddTimer(1000 / 60, timer_60hz_cb, userdata);
}


