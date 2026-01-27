# Copilot instructions for vNES

## Project overview
- vNES is a C++11 NES emulator built as a single binary (Makefile) with SFML for graphics/audio/input.
- Main loop wiring lives in src/main.cpp: ROM load → Bus connect/reset → Display/Input/Sound/Debugger loop.

## Architecture (key components & data flow)
- Bus is the hub for CPU/PPU/APU and handles CPU memory mapping and timing. PPU runs 3x CPU; APU steps with CPU (see src/bus.cpp).
- Cartridge owns PRG/CHR/PRG-RAM and instantiates a Mapper via MapperFactory (src/cartridge.cpp, src/mapper.cpp).
- CPU/PPU/APU talk to memory only through Bus and Cartridge interfaces (cpuRead/cpuWrite, readPrg/writePrg, readChr/writeChr).
- Input reads from SFML and is serialized via $4016 reads (src/input.cpp).
- Audio samples are pushed from APU into Sound’s ring buffer; Sound is an sf::SoundStream (src/sound.cpp).

## Mapper conventions
- Add new mappers by creating mapper_XXX.{h,cpp} and registering in MapperFactory::create (src/mapper.cpp).
- Mapper init is passed PRG/CHR/PRG-RAM vectors and initial mirroring; follow that ownership (Mapper stores pointers only).
- Optional mapper hooks: scanline() for MMC3-style IRQs; notifyPpuAddr() for MMC2/MMC4.

## Developer workflows
- Build: make (outputs bin/vnes). Flags are -Wall -Wextra -Werror -O0 -g (see Makefile).
- Run: ./bin/vnes roms/your_game.nes
- Debugger: start with -d/--debug, then press ESC to enter REPL (src/main.cpp, src/debugger.cpp).
- Static analysis: make analyze runs cppcheck for unused functions.

## Project-specific patterns
- Address decoding is centralized in Bus with memory-region logging for debugger (src/bus.cpp).
- PPU/CPU register access is modeled as explicit getters/setters for debugger manipulation (src/ppu.h, src/cpu.h).
- Frame loop runs until Bus reports PPU frame complete, then Display updates with PPU framebuffer (src/main.cpp).

## Integration points & dependencies
- SFML 2.x is required (graphics/window/system/audio), linked in Makefile.
- ROMs must be iNES; NES 2.0 headers are accepted but treated as iNES 1.0 (src/cartridge.cpp).

## Coding conventions
- Use u8/u16/u32/u64 for fixed-width integers (typedefs in src/types.h).
- Use nullptr instead of NULL or 0 for pointers.
- Use std::vector for dynamic arrays; avoid raw pointers except in performance-critical paths.
- Use std::lock_guard<std::mutex> for thread-safe audio buffer access (src/sound.cpp).
- Keep the code simple and readable; avoid clever optimizations unless absolutely necessary for performance.
- Avoid using template metaprogramming or advanced C++ features beyond C++11 to maintain code clarity.