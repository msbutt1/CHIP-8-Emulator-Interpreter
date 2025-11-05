#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "platform_sdl.h"
#include "../core/chip8.h"
#include "../core/chip8_state.h"

typedef struct Args {
  const char* rom_path;
  int scale;
  int hz;
  bool log;
  bool vsync;
  bool delay_quirk; // accepted but not used currently
  bool mem_quirk;   // controls Fx55/Fx65 increment I
} Args;

static uint8_t default_rng(void* user) {
  (void)user;
  static uint32_t s = 0x12345678u;
  s ^= s << 13;
  s ^= s >> 17;
  s ^= s << 5;
  return (uint8_t)(s & 0xFF);
}

static void print_usage(const char* prog) {
  printf("Usage: %s rom.ch8 [--scale N] [--hz N] [--log] [--vsync] [--delay-quirk on|off] [--mem-quirk on|off]\n", prog);
}

static bool parse_args(int argc, char** argv, Args* out) {
  memset(out, 0, sizeof(*out));
  out->scale = 10;
  out->hz = 700;
  out->vsync = false;
  out->delay_quirk = false;
  out->mem_quirk = true; // original increments I

  if (argc < 2) return false;
  out->rom_path = argv[1];
  for (int i = 2; i < argc; ++i) {
    if (strcmp(argv[i], "--scale") == 0 && i + 1 < argc) { out->scale = atoi(argv[++i]); }
    else if (strcmp(argv[i], "--hz") == 0 && i + 1 < argc) { out->hz = atoi(argv[++i]); }
    else if (strcmp(argv[i], "--log") == 0) { out->log = true; }
    else if (strcmp(argv[i], "--vsync") == 0) { out->vsync = true; }
    else if (strcmp(argv[i], "--delay-quirk") == 0 && i + 1 < argc) {
      const char* v = argv[++i]; out->delay_quirk = (strcmp(v, "on") == 0);
    } else if (strcmp(argv[i], "--mem-quirk") == 0 && i + 1 < argc) {
      const char* v = argv[++i]; out->mem_quirk = (strcmp(v, "on") == 0);
    } else {
      printf("Unknown option: %s\n", argv[i]);
      return false;
    }
  }
  return true;
}

static bool load_file(const char* path, uint8_t** data_out, size_t* size_out) {
  FILE* f = fopen(path, "rb");
  if (!f) return false;
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (sz <= 0) { fclose(f); return false; }
  uint8_t* buf = (uint8_t*)malloc((size_t)sz);
  if (!buf) { fclose(f); return false; }
  size_t rd = fread(buf, 1, (size_t)sz, f);
  fclose(f);
  if (rd != (size_t)sz) { free(buf); return false; }
  *data_out = buf; *size_out = (size_t)sz; return true;
}

static void dump_snapshot(struct Chip8* c8) {
  Chip8Snapshot s; chip8_get_snapshot(c8, &s);
  printf("PC=%03X I=%03X DT=%u ST=%u SP=%u TOP=%03X HASH=%08X\n",
         s.pc, s.I, s.delay_timer, s.sound_timer, s.sp, s.stack_top, s.display_hash);
  printf("V:");
  for (int i = 0; i < 16; ++i) printf(" %02X", s.V[i]);
  printf("\n");
}

static void set_mem_quirk(struct Chip8* c8, bool on) {
  // Not exposed; rely on default true for original behavior. No-op here.
  (void)c8; (void)on;
}

static int key_to_hex(SDL_Keycode key) {
  switch (key) {
    case SDLK_1: return 0x1; case SDLK_2: return 0x2; case SDLK_3: return 0x3; case SDLK_4: return 0xC;
    case SDLK_q: return 0x4; case SDLK_w: return 0x5; case SDLK_e: return 0x6; case SDLK_r: return 0xD;
    case SDLK_a: return 0x7; case SDLK_s: return 0x8; case SDLK_d: return 0x9; case SDLK_f: return 0xE;
    case SDLK_z: return 0xA; case SDLK_x: return 0x0; case SDLK_c: return 0xB; case SDLK_v: return 0xF;
    default: return -1;
  }
}

int main(int argc, char** argv) {
  Args args;
  if (!parse_args(argc, argv, &args)) { print_usage(argv[0]); return 1; }

  uint8_t* rom_data = NULL; size_t rom_size = 0;
  if (!load_file(args.rom_path, &rom_data, &rom_size)) {
    printf("Failed to read ROM: %s\n", args.rom_path);
    return 1;
  }

  Chip8* c8 = chip8_create(default_rng, NULL);
  if (!c8) { free(rom_data); return 1; }
  if (!chip8_load_rom(c8, rom_data, rom_size)) { printf("ROM too large\n"); free(rom_data); chip8_destroy(c8); return 1; }
  set_mem_quirk(c8, args.mem_quirk);

  PlatformSDL plat;
  if (!platform_sdl_init(&plat, "chip8-c", args.scale, args.vsync, c8)) {
    printf("SDL init failed\n");
    free(rom_data);
    chip8_destroy(c8);
    return 1;
  }
  SDL_TimerID t60 = platform_sdl_add_60hz_timer(c8);

  bool running = true;
  bool paused = false;
  uint32_t last = SDL_GetTicks();
  double cycles_accum = 0.0;
  const double cycles_per_ms = (double)args.hz / 1000.0;

  while (running) {
    // Events
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) { running = false; }
      else if (e.type == SDL_KEYDOWN) {
        if (e.key.keysym.sym == SDLK_ESCAPE) running = false;
        else if (e.key.keysym.sym == SDLK_p) paused = !paused;
        else if (e.key.keysym.sym == SDLK_n && paused) chip8_step(c8);
        else if (e.key.keysym.sym == SDLK_F1) { chip8_reset(c8); chip8_load_rom(c8, rom_data, rom_size); }
        else if (e.key.keysym.sym == SDLK_F5) { chip8_reset(c8); chip8_load_rom(c8, rom_data, rom_size); }
        else if (e.key.keysym.sym == SDLK_F12) { dump_snapshot(c8); }
        int hx = key_to_hex(e.key.keysym.sym);
        if (hx >= 0) chip8_key_down(c8, (uint8_t)hx);
      } else if (e.type == SDL_KEYUP) {
        int hx = key_to_hex(e.key.keysym.sym);
        if (hx >= 0) chip8_key_up(c8, (uint8_t)hx);
      } else if (e.type == SDL_USEREVENT && e.user.code == 1) {
        chip8_tick_60hz((Chip8*)e.user.data1);
      }
    }

    uint32_t now = SDL_GetTicks();
    uint32_t elapsed_ms = now - last;
    last = now;
    cycles_accum += elapsed_ms * cycles_per_ms;

    if (!paused) {
      int steps = (int)cycles_accum;
      if (steps > 0) {
        for (int i = 0; i < steps; ++i) chip8_step(c8);
        cycles_accum -= steps;
      }
    }

    platform_sdl_render(&plat, chip8_framebuffer(c8));

    // Small sleep to avoid 100% CPU when vsync off
    SDL_Delay(1);
  }

  SDL_RemoveTimer(t60);
  platform_sdl_shutdown(&plat);
  free(rom_data);
  chip8_destroy(c8);
  return 0;
}


