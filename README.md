# vNES — Nintendo Entertainment System Emulator

A cycle-accurate NES emulator written in C++20, featuring hardware-accurate emulation of the CPU, PPU, and APU, an in-app ImGui debugger, HQ2x pixel-art upscaling, Game Genie support, and a built-in web server for remote debugging.

---

## Features

- **CPU** — Full MOS 6502 emulation (official opcodes)
- **PPU** — Scanline-accurate Picture Processing Unit with correct sprite/background rendering
- **APU** — Full audio emulation: 2× pulse, triangle, noise, and DMC channels with DC-blocking filter
- **Mapper support** — iNES mapper infrastructure with five mappers implemented:
  | # | Name | Games |
  |---|------|-------|
  | 0 | NROM | Super Mario Bros., Donkey Kong, Excitebike |
  | 1 | MMC1 (SxROM) | The Legend of Zelda, Metroid, Mega Man 2 |
  | 2 | UxROM | Mega Man, Castlevania, Contra |
  | 4 | MMC3 (TxROM) | Super Mario Bros. 2 & 3, Mega Man 3–6 |
  | 9 | MMC2 (PxROM) | Mike Tyson's Punch-Out!! |
- **HQ2x upscaling** — Pixel-art scaling runs in a dedicated background thread
- **In-app GUI** — ImGui-based overlay (press **ESC**) with:
  - CPU register & disassembly viewer
  - PPU pattern table, nametable, palette & OAM viewers
  - APU channel viewer
  - Hex memory viewer/editor
  - Debugger REPL console
  - ROM file browser
  - Cartridge info panel
  - Pause, step-by-step, and step-frame execution
  - Emulator-in-window docking mode
- **Game Genie** — 6- and 8-character code entry
- **Battery SRAM** — Auto-saves PRG-RAM to disk on games with battery backup
- **Web debugger** — Lightweight HTTP server on port 18080 (powered by Crow)
- **ROM database** — SQLite-backed DB populated via curl + No-Intro XML

---

## Architecture

```
┌────────────────────────────────────────────────┐
│                     Bus                        │
│   (memory map, timing, CPU↔PPU↔APU wiring)     │
│                                                │
│  ┌────────┐  ┌─────────┐  ┌─────────────────┐  │
│  │  CPU   │  │   PPU   │  │      APU        │  │
│  │ 6502   │  │scanline │  │ pulse×2+tri+    │  │
│  │        │  │renderer │  │ noise+DMC       │  │
│  └────────┘  └────┬────┘  └───────┬─────────┘  │
│                   │               │            │
│  ┌────────────────▼───────────────▼──────────┐ │
│  │              Cartridge                    │ │
│  │    PRG ROM / CHR ROM / PRG RAM            │ │
│  │    Mapper (000/001/002/004/009)           │ │
│  └───────────────────────────────────────────┘ │
└────────────────────────────────────────────────┘
         │                │              │
    ┌────▼────┐     ┌─────▼────┐   ┌────▼────┐
    │ Display │     │  Input   │   │  Sound  │
    │ HQ2x    │     │ Keyboard │   │ Ring    │
    │ thread  │     │ $4016    │   │ buffer  │
    └────┬────┘     └──────────┘   └─────────┘
         │
    ┌────▼────┐     ┌──────────┐
    │   GUI   │     │  Web     │
    │ ImGui   │     │ Server   │
    │ overlay │     │ :18080   │
    └─────────┘     └──────────┘
```

### Key components

| Component | File(s) | Responsibility |
|-----------|---------|----------------|
| `Bus` | `bus.cpp/h` | Central hub — CPU/PPU/APU wiring, 2KB RAM, memory-mapped I/O, address decoding |
| `CPU` | `cpu.cpp/h` | MOS 6502 interpreter, interrupt handling, cycle counting |
| `PPU` | `ppu.cpp/h` | Background/sprite rendering, VRAM, OAM, palette, framebuffer output |
| `APU` | `apu.cpp/h` | Audio synthesis, frame counter, IRQ, sample generation |
| `Cartridge` | `cartridge.cpp/h` | iNES parser, PRG/CHR/PRG-RAM storage, mapper instantiation, SRAM save, Game Genie patches |
| `Mapper` | `mapper*.cpp/h` | Bank switching, mirroring, scanline IRQ (MMC3), CHR latching (MMC2) |
| `Display` | `display.cpp/h` | SFML window, HQ2x scaling thread, double-buffered frame hand-off |
| `Gui` | `gui.cpp/h` | ImGui menu, debugger panels, file browser, action queue |
| `GuiConsole` | `gui_console.cpp/h` | REPL debugger — read/write memory, step, disassemble, breakpoints |
| `Input` | `input.cpp/h` | SFML keyboard → NES controller shift register ($4016/$4017) |
| `Sound` | `sound.cpp/h` | `sf::SoundStream` subclass, ring buffer, DC-blocking filter |
| `WebServer` | `web_server.cpp/h` | Crow HTTP server serving `web_debugger.html` on port 18080 |
| `RomDB` | `romdb.cpp/h` | Fetch No-Intro XML via curl, parse with tinyxml2, store in SQLite |

### Timing model

The main loop drives the system one **CPU clock** at a time. The PPU runs at **3× the CPU clock rate** — every `bus.clock()` call ticks the CPU once and the PPU three times. The APU is stepped in lock-step with the CPU. A frame completes when the PPU signals vertical blank.

```
bus.clock()
  ├── cpu.clock()          // 1 CPU cycle
  ├── ppu.clock()          // 3 PPU cycles (×3 per cpu cycle)
  └── apu.clock()          // 1 APU cycle
```

### Display pipeline

1. PPU writes a `u32` ARGB framebuffer (256×240) each frame.
2. `Display::queueFrame()` copies it to a pending buffer and wakes the scaler thread.
3. The scaler thread runs **HQ2x** to produce a 512×480 buffer.
4. `Display::consumeScaledFrame()` swaps the completed buffers into the main thread.
5. The SFML texture is updated and drawn; ImGui renders on top before `window.display()`.

---

## Controls

| NES Button | Key |
|-----------|-----|
| D-Pad | Arrow Keys **or** WASD |
| A | Z **or** J |
| B | X **or** K |
| Start | Enter **or** Space |
| Select | Left Shift |
| GUI menu | ESC |
| Pause (in menu) | P |

---

## Building

### Windows (Visual Studio + vcpkg)

1. Install [vcpkg](https://github.com/microsoft/vcpkg) and integrate it:
   ```powershell
   vcpkg integrate install
   ```
2. Install dependencies (vcpkg manifest mode — `vcpkg.json` is included):
   ```powershell
   vcpkg install
   ```
3. Open `Vnes.vcxproj` (or the solution) in Visual Studio 2022+ and build.

**Dependencies (managed by vcpkg):**

| Library | Purpose |
|---------|---------|
| SFML 3.x | Window, graphics, audio |
| imgui-sfml | ImGui + SFML backend |
| Crow | Embedded HTTP server |
| libcurl | ROM database downloads |
| SQLite3 | ROM database storage |
| tinyxml2 | No-Intro XML parsing |

### Linux / macOS (Makefile)

```bash
# Install SFML development libraries (e.g. on Ubuntu):
sudo apt install libsfml-dev

make          # produces bin/vnes
```

Build flags: `-Wall -Wextra -Werror -O0 -g` (see `Makefile`).

---

## Running

```bash
# Launch with a ROM
./bin/vnes roms/super_mario_bros.nes

# Launch without a ROM (use GUI to browse and load)
./bin/vnes

# Help
./bin/vnes --help
```

---

## In-App Debugger (GUI)

Press **ESC** at any time to open the overlay menu. Available panels:

- **File → Load ROM** — file browser to pick a `.nes` file
- **Emulation → Pause / Resume / Reset**
- **Emulation → Step** — advance one CPU instruction
- **Emulation → Step Frame** — advance one full PPU frame
- **Debug → CPU** — registers (A, X, Y, PC, SP, flags) and disassembly
- **Debug → PPU** — pattern tables, nametables, palettes, OAM sprite list
- **Debug → APU** — per-channel volume, frequency, length counter
- **Debug → Memory** — hex viewer for CPU/PPU/OAM address spaces
- **Debug → Console** — REPL (type `help` for command list)
- **Cheats → Game Genie** — enter 6- or 8-character codes
- **Info → Cartridge** — mapper number, PRG/CHR sizes, mirroring, battery flag

---

## Web Debugger

A lightweight HTTP server starts automatically on **port 18080**. Open `http://localhost:18080` in a browser. If `web_debugger.html` is not present beside the binary, the server returns a notice redirecting you to the in-app console.

---

## Adding a New Mapper

1. Create `src/mapper_XXX.h` and `src/mapper_XXX.cpp` following the pattern of an existing mapper.
2. Override `readPrg`, `writePrg`, `readChr`, `writeChr`, and optionally `scanline()` (IRQ) / `notifyPpuAddr()` (CHR latch).
3. Register it in `MapperFactory::create()` inside `src/mapper.cpp`.

---

## ROM Format

vNES accepts **iNES** (`.nes`) files. NES 2.0 headers are detected but treated as iNES 1.0. Battery-backed SRAM is saved to `<rom_name>.sav` in the same directory as the ROM.

---

## License

See individual source files for third-party license notices (HQ2x — Apache 2.0).
