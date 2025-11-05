#include "chip8.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "opcodes.h"

#define MEM_SIZE 4096
#define FB_WIDTH 64
#define FB_HEIGHT 32

typedef struct Chip8 {
  // Memory and registers
  uint8_t memory[MEM_SIZE];
  uint8_t V[16];
  uint16_t I;
  uint16_t pc;

  // Stack
  uint16_t stack[16];
  uint8_t sp;

  // Timers
  uint8_t delay_timer;
  uint8_t sound_timer;

  // Frame buffer and keypad
  uint8_t gfx[FB_WIDTH * FB_HEIGHT];
  uint8_t keypad[16];

  // RNG
  chip8_rand_func rng;
  void* rng_user;

  // Execution state
  bool waiting_for_key;
  uint8_t wait_key_reg;

  // Quirks
  Chip8Quirks quirks;
} Chip8Impl;

static void c8_clear(Chip8Impl* c8) {
  memset(c8->memory, 0, sizeof(c8->memory));
  memset(c8->V, 0, sizeof(c8->V));
  memset(c8->stack, 0, sizeof(c8->stack));
  memset(c8->gfx, 0, sizeof(c8->gfx));
  memset(c8->keypad, 0, sizeof(c8->keypad));
  c8->I = 0;
  c8->pc = 0x200;
  c8->sp = 0;
  c8->delay_timer = 0;
  c8->sound_timer = 0;
  c8->waiting_for_key = false;
  c8->wait_key_reg = 0;
}

Chip8* chip8_create(chip8_rand_func rng, void* rng_user) {
  Chip8Impl* c8 = (Chip8Impl*)malloc(sizeof(Chip8Impl));
  if (!c8) return NULL;
  memset(c8, 0, sizeof(*c8));
  c8->rng = rng;
  c8->rng_user = rng_user;
  c8->quirks.shift_uses_vy = false;
  c8->quirks.mem_ops_increment_i = true; // original semantics increment I
  c8->quirks.jump_with_offset_uses_vx0 = false; // original Bnnn uses V0
  c8_clear(c8);
  chip8_install_fontset((Chip8*)c8);
  return (Chip8*)c8;
}

void chip8_destroy(Chip8* c8p) {
  Chip8Impl* c8 = (Chip8Impl*)c8p;
  if (c8) free(c8);
}

void chip8_reset(Chip8* c8p) {
  Chip8Impl* c8 = (Chip8Impl*)c8p;
  // Preserve fontset area, so save it before clear and restore
  uint8_t font_copy[80];
  memcpy(font_copy, &c8->memory[0x50], 80);
  c8_clear(c8);
  memcpy(&c8->memory[0x50], font_copy, 80);
}

bool chip8_load_rom(Chip8* c8p, const uint8_t* data, size_t size) {
  Chip8Impl* c8 = (Chip8Impl*)c8p;
  if (!data && size > 0) return false;
  if (0x200 + size > MEM_SIZE) return false;
  memcpy(&c8->memory[0x200], data, size);
  c8->pc = 0x200;
  return true;
}

void chip8_step(Chip8* c8p) {
  Chip8Impl* c8 = (Chip8Impl*)c8p;
  if (c8->waiting_for_key) return; // stall
  uint16_t opcode = (uint16_t)c8->memory[c8->pc] << 8 | c8->memory[c8->pc + 1];
  bool auto_advance = chip8_execute_opcode((Chip8*)c8, opcode);
  if (auto_advance) c8->pc = (uint16_t)(c8->pc + 2);
}

void chip8_tick_60hz(Chip8* c8p) {
  Chip8Impl* c8 = (Chip8Impl*)c8p;
  if (c8->delay_timer > 0) c8->delay_timer--;
  if (c8->sound_timer > 0) c8->sound_timer--;
}

void chip8_key_down(Chip8* c8p, uint8_t hex_key) {
  Chip8Impl* c8 = (Chip8Impl*)c8p;
  if ((hex_key & 0xF) != hex_key) return;
  c8->keypad[hex_key] = 1;
  if (c8->waiting_for_key) {
    c8->V[c8->wait_key_reg] = hex_key;
    c8->waiting_for_key = false;
  }
}

void chip8_key_up(Chip8* c8p, uint8_t hex_key) {
  Chip8Impl* c8 = (Chip8Impl*)c8p;
  if ((hex_key & 0xF) != hex_key) return;
  c8->keypad[hex_key] = 0;
}

const uint8_t* chip8_framebuffer(const Chip8* c8p) {
  const Chip8Impl* c8 = (const Chip8Impl*)c8p;
  return c8->gfx;
}

void chip8_get_snapshot(const Chip8* c8p, Chip8Snapshot* out) {
  const Chip8Impl* c8 = (const Chip8Impl*)c8p;
  if (!out) return;
  out->pc = c8->pc;
  out->I = c8->I;
  memcpy(out->V, c8->V, sizeof(out->V));
  out->delay_timer = c8->delay_timer;
  out->sound_timer = c8->sound_timer;
  out->sp = c8->sp;
  out->stack_top = c8->sp ? c8->stack[c8->sp - 1] : 0;
  // Simple hash of display buffer
  uint32_t h = 2166136261u; // FNV-1a seed
  for (size_t i = 0; i < FB_WIDTH * FB_HEIGHT; ++i) {
    h ^= c8->gfx[i];
    h *= 16777619u;
  }
  out->display_hash = h;
}

const char* chip8_core_version(void) { return CHIP8_VERSION; }


