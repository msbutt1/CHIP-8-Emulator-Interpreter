#include "opcodes.h"

#include <string.h>

#include "chip8.h"

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

static inline uint8_t c8_rand(Chip8Impl* c8) {
  return c8->rng ? c8->rng(c8->rng_user) : 0;
}

void chip8_install_fontset(struct Chip8* c8p) {
  Chip8Impl* c8 = (Chip8Impl*)c8p;
  static const uint8_t fontset[80] = {
      0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
      0x20, 0x60, 0x20, 0x20, 0x70, // 1
      0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
      0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
      0x90, 0x90, 0xF0, 0x10, 0x10, // 4
      0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
      0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
      0xF0, 0x10, 0x20, 0x40, 0x40, // 7
      0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
      0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
      0xF0, 0x90, 0xF0, 0x90, 0x90, // A
      0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
      0xF0, 0x80, 0x80, 0x80, 0xF0, // C
      0xE0, 0x90, 0x90, 0x90, 0xE0, // D
      0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
      0xF0, 0x80, 0xF0, 0x80, 0x80  // F
  };
  memcpy(&c8->memory[0x50], fontset, sizeof(fontset));
}

static inline void op_cls(Chip8Impl* c8) {
  memset(c8->gfx, 0, sizeof(c8->gfx));
}

static inline void op_ret(Chip8Impl* c8) {
  if (c8->sp > 0) {
    c8->sp--;
    c8->pc = c8->stack[c8->sp];
  }
}

static inline void op_jp(Chip8Impl* c8, uint16_t addr) { c8->pc = addr; }

static inline void op_call(Chip8Impl* c8, uint16_t addr) {
  if (c8->sp < 16) {
    c8->stack[c8->sp++] = c8->pc;
    c8->pc = addr;
  }
}

static inline void op_se_byte(Chip8Impl* c8, uint8_t x, uint8_t kk, uint16_t* pc_adv) {
  if (c8->V[x] == kk) *pc_adv += 2;
}

static inline void op_sne_byte(Chip8Impl* c8, uint8_t x, uint8_t kk, uint16_t* pc_adv) {
  if (c8->V[x] != kk) *pc_adv += 2;
}

static inline void op_se_xy(Chip8Impl* c8, uint8_t x, uint8_t y, uint16_t* pc_adv) {
  if (c8->V[x] == c8->V[y]) *pc_adv += 2;
}

static inline void op_ld_byte(Chip8Impl* c8, uint8_t x, uint8_t kk) { c8->V[x] = kk; }
static inline void op_add_byte(Chip8Impl* c8, uint8_t x, uint8_t kk) { c8->V[x] += kk; }

static inline void op_alu(Chip8Impl* c8, uint8_t x, uint8_t y, uint8_t subcode) {
  uint16_t tmp;
  switch (subcode) {
    case 0x0: c8->V[x] = c8->V[y]; break;              // LD Vx, Vy
    case 0x1: c8->V[x] |= c8->V[y]; c8->V[0xF] = 0; break; // OR
    case 0x2: c8->V[x] &= c8->V[y]; c8->V[0xF] = 0; break; // AND
    case 0x3: c8->V[x] ^= c8->V[y]; c8->V[0xF] = 0; break; // XOR
    case 0x4:                                           // ADD
      tmp = (uint16_t)c8->V[x] + (uint16_t)c8->V[y];
      c8->V[0xF] = tmp > 0xFF;
      c8->V[x] = (uint8_t)tmp;
      break;
    case 0x5:                                           // SUB Vx = Vx - Vy
      c8->V[0xF] = (c8->V[x] > c8->V[y]);
      c8->V[x] = (uint8_t)(c8->V[x] - c8->V[y]);
      break;
    case 0x6: {                                         // SHR
      uint8_t src = c8->quirks.shift_uses_vy ? c8->V[y] : c8->V[x];
      c8->V[0xF] = src & 0x1;
      c8->V[x] = src >> 1;
      break;
    }
    case 0x7:                                           // SUBN Vx = Vy - Vx
      c8->V[0xF] = (c8->V[y] > c8->V[x]);
      c8->V[x] = (uint8_t)(c8->V[y] - c8->V[x]);
      break;
    case 0xE: {                                         // SHL
      uint8_t src = c8->quirks.shift_uses_vy ? c8->V[y] : c8->V[x];
      c8->V[0xF] = (src & 0x80) != 0;
      c8->V[x] = (uint8_t)(src << 1);
      break;
    }
  }
}

static inline void op_sne_xy(Chip8Impl* c8, uint8_t x, uint8_t y, uint16_t* pc_adv) {
  if (c8->V[x] != c8->V[y]) *pc_adv += 2;
}

static inline void op_ld_i(Chip8Impl* c8, uint16_t addr) { c8->I = addr; }

static inline void op_jp_v0(Chip8Impl* c8, uint16_t addr) {
  c8->pc = (uint16_t)(addr + c8->V[0]);
}

static inline void op_rnd(Chip8Impl* c8, uint8_t x, uint8_t kk) {
  c8->V[x] = (uint8_t)(c8_rand(c8) & kk);
}

static inline void op_drw(Chip8Impl* c8, uint8_t x, uint8_t y, uint8_t n) {
  uint8_t vx = c8->V[x] % FB_WIDTH;
  uint8_t vy = c8->V[y] % FB_HEIGHT;
  c8->V[0xF] = 0;
  for (uint8_t row = 0; row < n; ++row) {
    if (vy + row >= FB_HEIGHT) break; // wrap vertically optional; here stop
    uint8_t sprite = c8->memory[c8->I + row];
    for (uint8_t col = 0; col < 8; ++col) {
      uint8_t px = (vx + col) % FB_WIDTH;
      uint8_t bit = (sprite >> (7 - col)) & 1u;
      uint16_t idx = (uint16_t)(vy + row) * FB_WIDTH + px;
      uint8_t prev = c8->gfx[idx];
      uint8_t newv = prev ^ bit;
      c8->gfx[idx] = newv;
      if (prev == 1 && bit == 1) c8->V[0xF] = 1;
    }
  }
}

static inline void op_skp(Chip8Impl* c8, uint8_t x, uint16_t* pc_adv) {
  if (c8->keypad[c8->V[x] & 0xF]) *pc_adv += 2;
}
static inline void op_sknp(Chip8Impl* c8, uint8_t x, uint16_t* pc_adv) {
  if (!c8->keypad[c8->V[x] & 0xF]) *pc_adv += 2;
}

bool chip8_execute_opcode(struct Chip8* c8p, uint16_t opcode) {
  Chip8Impl* c8 = (Chip8Impl*)c8p;

  // If waiting for key (Fx0A), only handle key events externally; here we stall PC advance.
  if (c8->waiting_for_key) {
    return false; // do not auto-advance; platform should call key_down to resume
  }

  uint16_t pc_advance = 2;
  uint8_t n1 = (opcode >> 12) & 0xF;
  uint8_t x = (opcode >> 8) & 0xF;
  uint8_t y = (opcode >> 4) & 0xF;
  uint8_t n = opcode & 0xF;
  uint8_t kk = opcode & 0xFF;
  uint16_t nnn = opcode & 0x0FFF;

  switch (n1) {
    case 0x0:
      switch (kk) {
        case 0xE0: op_cls(c8); break;                  // 00E0
        case 0xEE: op_ret(c8); return false;           // 00EE
        default: /* 0nnn - ignored */ break;
      }
      break;
    case 0x1: op_jp(c8, nnn); return false;           // 1nnn
    case 0x2: op_call(c8, nnn); return false;         // 2nnn
    case 0x3: op_se_byte(c8, x, kk, &pc_advance); break;       // 3xkk
    case 0x4: op_sne_byte(c8, x, kk, &pc_advance); break;      // 4xkk
    case 0x5: if (n == 0) op_se_xy(c8, x, y, &pc_advance); break; // 5xy0
    case 0x6: op_ld_byte(c8, x, kk); break;                     // 6xkk
    case 0x7: op_add_byte(c8, x, kk); break;                    // 7xkk
    case 0x8: op_alu(c8, x, y, n); break;                       // 8xyN
    case 0x9: if (n == 0) op_sne_xy(c8, x, y, &pc_advance); break; // 9xy0
    case 0xA: op_ld_i(c8, nnn); break;                          // Annn
    case 0xB:                                                  // Bnnn
      if (c8->quirks.jump_with_offset_uses_vx0)
        c8->pc = (uint16_t)(nnn + c8->V[x]);
      else
        c8->pc = (uint16_t)(nnn + c8->V[0]);
      return false;
    case 0xC: op_rnd(c8, x, kk); break;                        // Cxkk
    case 0xD: op_drw(c8, x, y, n); break;                      // Dxyn
    case 0xE:                                                  // Ex9E / ExA1
      switch (kk) {
        case 0x9E: op_skp(c8, x, &pc_advance); break;
        case 0xA1: op_sknp(c8, x, &pc_advance); break;
      }
      break;
    case 0xF:
      switch (kk) {
        case 0x07: c8->V[x] = c8->delay_timer; break;          // Fx07
        case 0x0A:                                             // Fx0A
          c8->waiting_for_key = true;
          c8->wait_key_reg = x;
          return false;
        case 0x15: c8->delay_timer = c8->V[x]; break;          // Fx15
        case 0x18: c8->sound_timer = c8->V[x]; break;          // Fx18
        case 0x1E: c8->I = (uint16_t)(c8->I + c8->V[x]); break; // Fx1E
        case 0x29: c8->I = (uint16_t)(0x50 + (c8->V[x] & 0xF) * 5); break; // Fx29
        case 0x33: {                                           // Fx33
          uint8_t v = c8->V[x];
          c8->memory[c8->I + 0] = (uint8_t)(v / 100);
          c8->memory[c8->I + 1] = (uint8_t)((v / 10) % 10);
          c8->memory[c8->I + 2] = (uint8_t)(v % 10);
          break;
        }
        case 0x55: {                                           // Fx55
          for (uint8_t i = 0; i <= x; ++i) c8->memory[c8->I + i] = c8->V[i];
          if (c8->quirks.mem_ops_increment_i) c8->I = (uint16_t)(c8->I + x + 1);
          break;
        }
        case 0x65: {                                           // Fx65
          for (uint8_t i = 0; i <= x; ++i) c8->V[i] = c8->memory[c8->I + i];
          if (c8->quirks.mem_ops_increment_i) c8->I = (uint16_t)(c8->I + x + 1);
          break;
        }
      }
      break;
  }

  // Inform caller to auto-advance PC by pc_advance
  return (pc_advance == 2) || (pc_advance == 4);
}


