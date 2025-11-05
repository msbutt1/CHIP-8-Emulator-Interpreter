/**
 * CHIP-8 core API (platform-agnostic, no SDL or timing). This module exposes
 * an opaque `Chip8` instance that holds CPU state, memory, keypad, timers,
 * and a 64x32 1bpp frame buffer. The platform is responsible for providing a
 * deterministic RNG function and for calling chip8_tick_60hz() to decrement
 * timers at 60Hz.
 */

#ifndef CHIP8_CORE_H
#define CHIP8_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "chip8_state.h"

#ifndef CHIP8_VERSION
#define CHIP8_VERSION "0.0.0"
#endif

typedef struct Chip8 Chip8; // Opaque state

// Deterministic RNG provider used by RND opcode (Cxkk)
typedef uint8_t (*chip8_rand_func)(void* user);

// Create/destroy lifecycle. rng may be NULL, which behaves as constant 0.
Chip8* chip8_create(chip8_rand_func rng, void* rng_user);
void chip8_destroy(Chip8*);

// Reset CPU, memory (keeps fontset installed), registers, timers, display and keypad.
void chip8_reset(Chip8*);

// Load a ROM into memory starting at 0x200. Returns false if it would overflow memory.
bool chip8_load_rom(Chip8*, const uint8_t* data, size_t size);

// Execute one fetch-decode-execute CPU cycle. Does not tick timers.
void chip8_step(Chip8*);

// Tick timers at 60Hz: if delay/sound timers > 0, decrement by 1.
void chip8_tick_60hz(Chip8*);

// Keypad input: hex_key is 0x0..0xF. Keys outside range are ignored.
void chip8_key_down(Chip8*, uint8_t hex_key);
void chip8_key_up(Chip8*, uint8_t hex_key);

// Access the 64x32 monochrome frame buffer (values are 0 or 1 per pixel).
const uint8_t* chip8_framebuffer(const Chip8*);

// Extract a compact snapshot for tests.
void chip8_get_snapshot(const Chip8*, Chip8Snapshot* out);

// Returns the Chip-8 core version string.
const char* chip8_core_version(void);

#endif // CHIP8_CORE_H


