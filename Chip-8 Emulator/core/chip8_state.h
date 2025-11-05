#ifndef CHIP8_STATE_H
#define CHIP8_STATE_H

#include <stdint.h>

// Chip8Snapshot is a small POD structure intended for tests and debugging.
// It captures the main CPU registers and a tiny hash of the display so tests
// can validate execution deterministically without storing the entire frame buffer.
typedef struct Chip8Snapshot {
  uint16_t pc;
  uint16_t I;
  uint8_t V[16];
  uint8_t delay_timer;
  uint8_t sound_timer;
  uint8_t sp;            // stack pointer index (0-15)
  uint16_t stack_top;    // top value on stack if sp>0, else 0
  uint32_t display_hash; // simple rolling hash of 64x32 frame buffer
} Chip8Snapshot;

#endif // CHIP8_STATE_H


