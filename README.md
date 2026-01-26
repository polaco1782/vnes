# vNES - A "Minimal" NES Emulator

Because who doesn't want to spend an hour vibe-coding a 1983 gaming console from scratch? 🎮

Yes this was 95% vibe coded, believe me. Of course I know the technical stack of NES system, as I've written other emulators from scratch before..

No, it was not prompted like: "Write me a NES emulator with SOUND, VIDEO, INPUT an a DEBUGGER". It's not like that.

## What Is This?

A Nintendo Entertainment System (NES) emulator written in C++11 that actually, somehow, runs games. Not well. Not perfectly. But it runs them. Whether you intended it to or not.

Built with the philosophy of "just make it work" and "I'll fix the segfaults later" (spoiler: we did fix them, mostly).

## Language & Build

- **Language:** C++11 (because C++20 is for overachievers)
- **Build System:** Makefile (yes, really)
- **Dependencies:** SFML 2.x (for graphics, audio, and input)
- **Compiler Flags:** `-Wall -Wextra -Werror -O0 -g` (fail fast, debug later)

```bash
make        # Builds the emulator
make clean  # Pretend the audio crackling never happened
```

## What Actually Works

### CPU
- Full MOS 6502 processor with all 151 official opcodes
- Includes a 6502 debugger because stepping through code one instruction at a time is fun

### PPU (Graphics)
- 2C02 NTSC rendering pipeline
- Background scrolling with loopy registers (and yes, we implemented them correctly)
- Sprite rendering: 8x8 *and* 8x16 sprites (took us a while)
- Sprite 0 hit detection (only triggers once per frame, we learned this the hard way)
- Odd frame cycle skip (a feature so obscure, only real NES devs care)
- Proper attribute handling and pattern table addressing

### APU (Audio)
- 5 audio channels: 2 pulse waves, triangle, noise, and DMC
- Proper waveform generation with envelope control
- Frame counter with IRQ support
- Thread-safe audio streaming at 44.1 kHz
- No crackling (anymore)

### Input
- Full NES controller emulation via keyboard
- Arrow keys = D-pad
- Z = A button, X = B button
- Enter = Start, Right Shift = Select
- Actually fixed the bug where it kept pressing right

### ROM Support
- **Mapper 0 (NROM):** Simple games, original support
- **Mapper 1 (MMC1):** Bank switching, dynamic mirroring, PRG RAM at $6000-$7FFF
- iNES format loader with header validation
- Both horizontal and vertical mirroring (plus single-screen modes for the rebels)

### RAM
- 2KB internal CPU RAM
- 2KB nametable VRAM
- 256B sprite OAM
- 32B palette RAM
- 8KB PRG RAM (for games that need it)
- CHR RAM support (for games that write to patterns)

### Debugger
- Interactive REPL for stepping through code
- Register inspection and manipulation
- Memory dump and disassembly
- Command history with arrow key navigation
- Perfect for 3 AM debugging sessions

## What Doesn't Work

- Mappers 2-255 (too much effort)
- Save states (your progress is a memory leak away from disappearing)
- TAS support (but you can press buttons really fast)
- Netplay (play with your friends? use emulators that support it)
- Perfect timing accuracy (it's "close enough")
- That one NES ROM you really like probably (but try it anyway)

## Performance

Runs at 60 FPS on modern hardware. May vary if your CPU is from 2003.

## Known Issues Fixed

- ✅ Audio crackling (complete APU rewrite) wait, no it still buggy
- ✅ Segfaults with audio threading (atomic flags are your friend)
- ✅ Input stuck on right button (off-by-one fix)
- ✅ Missing 8x16 sprite support (now vibe-coded correctly)
- ❌ Your favorite ROM not working (probably a mapper thing)

## Usage

```bash
./bin/nes roms/your_game.nes
```

Then enjoy pixel-perfect nostalgia (or lag).

## Future Plans

- Add more mappers (when motivation strikes)
- Improve PPU accuracy (it's "good enough")
- Actually implement save states
- Stop finding new bugs

## License

MIT. Do whatever you want with this disaster.

---

YES this README.md was also VIBE CODED. (mostly)

*"It works on my machine" — This project, probably*
