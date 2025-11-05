#ifndef CHIP8_OPCODES_H
#define CHIP8_OPCODES_H

#include <stdint.h>
#include <stdbool.h>

struct Chip8;

// Quirk flags container
typedef struct Chip8Quirks {
  bool shift_uses_vy;         // 8xy6/8xyE use Vy as source (false = use Vx)
  bool mem_ops_increment_i;   // Fx55/Fx65 increment I (original true); false = no inc
  bool jump_with_offset_uses_vx0; // Bnnn uses Vx (superchip) vs V0 (original false)
} Chip8Quirks;

// Install the standard fontset at 0x50
void chip8_install_fontset(struct Chip8* c8);

// Execute the given opcode on the Chip8 instance. Returns whether PC should auto-advance.
// The caller is expected to advance PC by 2 when this returns true. If an opcode modifies
// PC directly (e.g., JP, CALL, RET, skips), this returns false to prevent double advance.
bool chip8_execute_opcode(struct Chip8* c8, uint16_t opcode);

#endif // CHIP8_OPCODES_H


