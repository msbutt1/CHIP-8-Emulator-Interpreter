# chip8-c

A modern, minimal C17 Chip-8 emulator with a clean separation between a pure core library and an SDL2 platform. Built with CMake, includes unit test scaffolding (Unity), runs on Windows and Linux, and ships with CI.

## Highlights
- **Core design**: `chip8_core` is platform-agnostic and deterministic (RNG injected), exposing a compact API.
- **SDL2 platform**: `chip8` executable provides rendering, audio, input, and timing (700 Hz CPU, 60 Hz timers).
- **Tooling**: C17, strict warnings, sanitizers in Debug, clang-format, Unity tests, and GitHub Actions CI.
- **Simple UX**: CLI flags for scale, speed, vsync, logging, and quirks; clear key mappings and controls.

## Targets
- `chip8` (executable): SDL-based emulator front-end.
- `chip8_core` (static library): pure CHIP-8 core (no SDL, deterministic, testable).
- `chip8_tests` (executable): Unity-based unit tests (sample included).

Tooling:
- C17
- Warnings: `-Wall -Wextra -Werror -pedantic` (or `/W4 /WX` on MSVC)
- Optimization: `-O2`
- Address/UB sanitizers in Debug on GCC/Clang

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

### Windows (generator uses --config)
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug -j
ctest --test-dir build --output-on-failure --build-config Debug
```

## Run
```bash
./build/chip8 ./assets/your.rom --scale 10 --hz 700 --vsync
```

Unity is fetched automatically via CMake into `third_party/`.

## CLI Options
- `--scale N` (default 10): integer upscale factor (64×32 → N×)
- `--hz N` (default 700): CPU cycles per second
- `--vsync`: enable vsync on the renderer
- `--log`: reserved for extra logging (minimal now)
- `--delay-quirk on|off`: accepted but currently not used by the core
- `--mem-quirk on|off`: accepted; core defaults to original increment-I semantics

## Key Mapping (PC → CHIP-8)
```
1 2 3 4      → 1 2 3 C
Q W E R      → 4 5 6 D
A S D F      → 7 8 9 E
Z X C V      → A 0 B F
```

## Controls
- Esc: Quit
- P: Pause
- N: Single-step one instruction (when paused)
- F1 / F5: Reset core and reload the ROM
- F12: Dump snapshot (PC, I, DT, ST, SP, stack top, hash, V registers) to stdout

## Layout

- `CMakeLists.txt` – root build and global tooling flags
- `cmake/` – CMake helpers (Unity fetch)
- `core/` – CHIP-8 core (`chip8.c/.h`, `opcodes.c/.h`, `chip8_state.h`)
- `src/` – SDL platform (`platform_sdl.c/.h`) and `main.c`
- `tests/` – Unity test runner and samples
- `third_party/` – fetched dependencies
- `assets/` – ROMs (empty placeholder)

## Core API (chip8_core)
The core is a single-cycle fetch-decode-execute engine with a small, test-friendly API and deterministic RNG injection. Timers are externally ticked at 60 Hz.

Key entry points:
- `chip8_create(chip8_rand_func rng, void* user)` / `chip8_destroy`
- `chip8_reset`, `chip8_load_rom(data, size)` (loads at 0x200)
- `chip8_step()` – one CPU cycle; no timer decrement inside
- `chip8_tick_60hz()` – decrements delay/sound timers if > 0
- `chip8_key_down/up(hexKey)` – keypad 0x0–0xF
- `chip8_framebuffer()` – 64×32 1bpp buffer (0/1 per pixel)
- `chip8_get_snapshot(Chip8Snapshot*)` – compact state for tests

Implemented opcodes include the standard CHIP-8 set (CLS, RET, JP, CALL, SE/SNE, LD/ADD, ALU 8xy*, SNE 9xy0, LD I, JP V0, RND, DRW with wrapping and collision in VF, SKP/SKNP, timers and memory ops Fx1E/Fx29/Fx33/Fx55/Fx65). SCHIP quirks are off by default; internal flags exist for future tuning.

## SDL2 Platform
- Rendering: 64×32 monochrome framebuffer uploaded as grayscale texture and scaled by `--scale` (default 10 → 640×320).
- Audio: simple square-wave beep while `sound_timer > 0`.
- Timing: ~`--hz` CPU pacing via accumulator; 60 Hz timers via `SDL_AddTimer` posting a user event.

## Development Tooling
- Language: C17
- Warnings/optimization: `-Wall -Wextra -Werror -pedantic`, `/W4 /WX` on MSVC, `-O2`
- Debug sanitizers (GCC/Clang): Address + Undefined Behavior
- Formatting: `.clang-format` (Google-ish)
- Tests: Unity fetched by CMake; `ctest` integration
- CI: GitHub Actions workflow builds and runs tests on Windows and Linux (Debug/Release)

## Timeline
- Milestone 1 — CMake scaffolding
  - Root project with strict flags, sanitizers (Debug), and `chip8_core`/`chip8`/`chip8_tests` targets.
  - Unity fetched via CMake; sample test integrated with CTest.
- Milestone 2 — CHIP-8 core
  - Public API (`chip8.h`, `chip8_state.h`) with deterministic RNG and snapshot support.
  - Opcode implementation and fast decode path; fontset installed at 0x50.
- Milestone 3 — SDL platform & app
  - Window/renderer/texture (64×32 → scaled), audio beep, input mapping, and timing.
  - CLI flags for scale, speed, vsync, and quirks placeholders; snapshot dumping.
- Milestone 4 — CI & tooling polish
  - GitHub Actions CI (Windows/Linux), clang-format, and improved README.

## Next Steps
- Expose quirk toggles via public API (e.g., shift source, I increment semantics).
- Add comprehensive unit tests and ROM-based behavior checks.
- Optional: add ROM selector UI, on-screen HUD, or debugger (disassembly/step/inspect).

---
Built with ❤️ in C, with a focus on clarity, determinism, and testability.


