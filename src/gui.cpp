#include "gui.h"
#include "bus.h"
#include "cpu.h"
#include "ppu.h"
#include "apu.h"
#include "cartridge.h"
#include "disasm.h"
#include <imgui.h>
#include <ImGui-SFML.h>
#include <SFML/Graphics.hpp>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <filesystem>
#include <vector>

using namespace vnes::disasm;

Gui::Gui(Bus& bus) 
    : bus_(bus)
    , console_(bus)
    , menuVisible_(false)
    , paused_(false)
    , showCpuDebugger_(false)
    , showPpuViewer_(false)
    , showPaletteViewer_(false)
    , showPatternViewer_(false)
    , showNametableViewer_(false)
    , showOamViewer_(false)
    , showMemoryViewer_(false)
    , showApuViewer_(false)
    , showGameGenie_(false)
    , showCartridgeInfo_(false)
    , showEmulatorWindow_(false)
    , showFileDialog_(false)
    , showConsole_(false)
    , ggSubmitted_(false)
    , memoryViewAddress_(0)
    , memoryViewType_(0)
    , patternTablePalette_(0)
    , emulatorTextureInitialized_(false)
    , selectedFileIndex_(-1)
{
    ggInput_[0] = '\0';
    memorySearchAddr_[0] = '\0';
    pathInput_[0] = '\0';
    currentPath_ = std::filesystem::current_path().string();
}

Gui::~Gui() {
    ImGui::SFML::Shutdown();
}

void Gui::initialize(sf::RenderWindow& window) {
    ImGui::SFML::Init(window);

    // Disable saving window positions/sizes to ini file
    ImGui::GetIO().IniFilename = nullptr;

    // Configure ImGui style
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 2.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.1f, 0.95f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);

    // Initialize emulator texture
    emulatorTexture_ = sf::Texture(sf::Vector2u{256, 240});
    emulatorTextureInitialized_ = true;
}

void Gui::processEvent(sf::RenderWindow& window, const sf::Event& event) {
    ImGui::SFML::ProcessEvent(window, event);
}

void Gui::update(sf::RenderWindow& window, float dt) {
    ImGui::SFML::Update(window, sf::seconds(dt));
}

void Gui::updateEmulatorTexture(const u32* framebuffer, unsigned width, unsigned height) {
    if (!emulatorTextureInitialized_) return;

    if (emulatorTexture_.getSize().x != width || emulatorTexture_.getSize().y != height) {
        emulatorTexture_ = sf::Texture(sf::Vector2u{ width, height });
    }

    // Convert framebuffer to RGBA
    std::vector<u8> pixels(width * height * 4);
    for (unsigned i = 0; i < width * height; i++) {
        u32 color = framebuffer[i];
        pixels[i * 4 + 0] = static_cast<u8>((color >> 16) & 0xFF);  // R
        pixels[i * 4 + 1] = static_cast<u8>((color >> 8) & 0xFF);   // G
        pixels[i * 4 + 2] = static_cast<u8>(color & 0xFF);          // B
        pixels[i * 4 + 3] = 255;  // A
    }

    sf::Image img(sf::Vector2u{ width, height }, pixels.data());
    [[maybe_unused]] bool loaded = emulatorTexture_.loadFromImage(img);
}

ImU32 Gui::nesColorToImU32(u8 colorIndex) const {
    u32 c = nesPalette[colorIndex & 0x3F];
    return IM_COL32((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF, 255);
}

std::string Gui::disassembleInstruction(u16 addr, int& length) {
    u8 opcode = bus_.read(addr);
    AddrMode mode = addrModes[opcode];
    length = modeLengths[static_cast<int>(mode)];

    char buf[64];
    const char* name = opcodeNames[opcode];

    u8 lo = (length > 1) ? bus_.read(addr + 1) : 0;
    u8 hi = (length > 2) ? bus_.read(addr + 2) : 0;
    u16 absAddr = (hi << 8) | lo;

    switch (mode) {
        case AddrMode::IMP:
        case AddrMode::ACC:
            snprintf(buf, sizeof(buf), "%s", name); 
            break;
        case AddrMode::IMM:  
            snprintf(buf, sizeof(buf), "%s #$%02X", name, lo); 
            break;
        case AddrMode::ZP:   
            snprintf(buf, sizeof(buf), "%s $%02X", name, lo); 
            break;
        case AddrMode::ZPX:  
            snprintf(buf, sizeof(buf), "%s $%02X,X", name, lo); 
            break;
        case AddrMode::ZPY:  
            snprintf(buf, sizeof(buf), "%s $%02X,Y", name, lo); 
            break;
        case AddrMode::ABS:  
            snprintf(buf, sizeof(buf), "%s $%04X", name, absAddr); 
            break;
        case AddrMode::ABX:  
            snprintf(buf, sizeof(buf), "%s $%04X,X", name, absAddr); 
            break;
        case AddrMode::ABY:  
            snprintf(buf, sizeof(buf), "%s $%04X,Y", name, absAddr); 
            break;
        case AddrMode::IND:  
            snprintf(buf, sizeof(buf), "%s ($%04X)", name, absAddr); 
            break;
        case AddrMode::IZX:  
            snprintf(buf, sizeof(buf), "%s ($%02X,X)", name, lo); 
            break;
        case AddrMode::IZY:  
            snprintf(buf, sizeof(buf), "%s ($%02X),Y", name, lo); 
            break;
        case AddrMode::REL: {
            s8 offset = static_cast<s8>(lo);
            u16 target = static_cast<u16>(addr + 2 + offset);
            snprintf(buf, sizeof(buf), "%s $%04X", name, target);
            break;
        }
        default: 
            snprintf(buf, sizeof(buf), "%s", name); 
            break;
    }

    return std::string(buf);
}

void Gui::render(sf::RenderWindow& window) {
    if (menuVisible_) {
        renderMenuBar();

        // Render debug windows
        if (showCpuDebugger_) renderCpuDebugger();
        if (showPpuViewer_) renderPpuViewer();
        if (showPaletteViewer_) renderPaletteViewer();
        if (showPatternViewer_) renderPatternTableViewer();
        if (showNametableViewer_) renderNametableViewer();
        if (showOamViewer_) renderOamViewer();
        if (showMemoryViewer_) renderMemoryViewer();
        if (showApuViewer_) renderApuViewer();
        if (showGameGenie_) renderGameGenie();
        if (showCartridgeInfo_) renderCartridgeInfo();
        if (showEmulatorWindow_) renderEmulatorWindow();
        if (showFileDialog_) renderFileDialog();
        if (showConsole_) renderConsole();
    }

    ImGui::SFML::Render(window);
}

void Gui::renderMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Load ROM...", "Ctrl+O")) {
                showFileDialog_ = true;
                currentPath_ = std::filesystem::current_path().string();
                directoryContents_.clear();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Reset", "Ctrl+R")) {
                pendingAction_.type = GuiAction::Reset;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                pendingAction_.type = GuiAction::Quit;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Emulation")) {
            if (paused_) {
                if (ImGui::MenuItem("Resume", "P")) {
                    paused_ = false;
                    pendingAction_.type = GuiAction::Resume;
                }
            } else {
                if (ImGui::MenuItem("Pause", "P")) {
                    paused_ = true;
                    pendingAction_.type = GuiAction::Pause;
                }
            }
            if (ImGui::MenuItem("Step Instruction", "F7", false, paused_)) {
                pendingAction_.type = GuiAction::Step;
            }
            if (ImGui::MenuItem("Step Frame", "F8", false, paused_)) {
                pendingAction_.type = GuiAction::StepFrame;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Debug")) {
            ImGui::MenuItem("Console", nullptr, &showConsole_);
            ImGui::Separator();
            ImGui::MenuItem("CPU Debugger", nullptr, &showCpuDebugger_);
            ImGui::MenuItem("PPU Viewer", nullptr, &showPpuViewer_);
            ImGui::MenuItem("APU Viewer", nullptr, &showApuViewer_);
            ImGui::Separator();
            ImGui::MenuItem("Memory Viewer", nullptr, &showMemoryViewer_);
            ImGui::MenuItem("Palette Viewer", nullptr, &showPaletteViewer_);
            ImGui::MenuItem("Pattern Tables", nullptr, &showPatternViewer_);
            ImGui::MenuItem("Nametables", nullptr, &showNametableViewer_);
            ImGui::MenuItem("OAM/Sprites", nullptr, &showOamViewer_);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Tools")) {
            ImGui::MenuItem("Game Genie", nullptr, &showGameGenie_);
            ImGui::MenuItem("Cartridge Info", nullptr, &showCartridgeInfo_);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Emulator in Window", nullptr, &showEmulatorWindow_);
            ImGui::EndMenu();
        }

        // Status indicator on the right
        float statusWidth = 150.0f;
        ImGui::SameLine(ImGui::GetWindowWidth() - statusWidth);
        if (paused_) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "PAUSED");
        } else {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "RUNNING");
        }

        ImGui::EndMainMenuBar();
    }
}

void Gui::renderCpuDebugger() {
    ImGui::SetNextWindowSize(ImVec2(400, 500), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("CPU Debugger", &showCpuDebugger_)) {
        // Registers section
        ImGui::Text("Registers:");
        ImGui::Separator();

        ImGui::Columns(2, "regs", false);
        ImGui::Text("PC: $%04X", bus_.cpu.getPC());
        ImGui::Text("SP: $%02X", bus_.cpu.getSP());
        ImGui::Text("A:  $%02X (%d)", bus_.cpu.getA(), bus_.cpu.getA());
        ImGui::NextColumn();
        ImGui::Text("X:  $%02X (%d)", bus_.cpu.getX(), bus_.cpu.getX());
        ImGui::Text("Y:  $%02X (%d)", bus_.cpu.getY(), bus_.cpu.getY());
        ImGui::Text("Cycles: %llu", bus_.cpu.getCycles());
        ImGui::Columns(1);

        ImGui::Spacing();

        // Flags
        ImGui::Text("Flags: ");
        ImGui::SameLine();
        u8 status = bus_.cpu.getStatus();
        auto flagColor = [](bool set) {
            return set ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
        };
        ImGui::TextColored(flagColor(status & 0x80), "N"); ImGui::SameLine();
        ImGui::TextColored(flagColor(status & 0x40), "V"); ImGui::SameLine();
        ImGui::TextColored(flagColor(status & 0x20), "-"); ImGui::SameLine();
        ImGui::TextColored(flagColor(status & 0x10), "B"); ImGui::SameLine();
        ImGui::TextColored(flagColor(status & 0x08), "D"); ImGui::SameLine();
        ImGui::TextColored(flagColor(status & 0x04), "I"); ImGui::SameLine();
        ImGui::TextColored(flagColor(status & 0x02), "Z"); ImGui::SameLine();
        ImGui::TextColored(flagColor(status & 0x01), "C");

        ImGui::Spacing();
        ImGui::Separator();

        // Disassembly
        ImGui::Text("Disassembly:");
        ImGui::BeginChild("Disasm", ImVec2(0, 200), ImGuiChildFlags_Borders);

        u16 addr = bus_.cpu.getPC();
        for (int i = 0; i < 15; i++) {
            int len;
            std::string instr = disassembleInstruction(addr, len);

            if (i == 0) {
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "> $%04X: %s", addr, instr.c_str());
            } else {
                ImGui::Text("  $%04X: %s", addr, instr.c_str());
            }
            addr += len;
        }
        ImGui::EndChild();

        // Stack view
        ImGui::Spacing();
        ImGui::Text("Stack (top 8 bytes):");
        ImGui::BeginChild("Stack", ImVec2(0, 60), ImGuiChildFlags_Borders);
        u8 sp = bus_.cpu.getSP();
        for (int i = 0; i < 8 && (sp + i + 1) <= 0xFF; i++) {
            u16 stackAddr = 0x0100 + sp + i + 1;
            ImGui::Text("$%04X: $%02X", stackAddr, bus_.read(stackAddr));
            ImGui::SameLine();
        }
        ImGui::EndChild();
    }
    ImGui::End();
}

void Gui::renderPpuViewer() {
    ImGui::SetNextWindowSize(ImVec2(350, 300), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("PPU Viewer", &showPpuViewer_)) {
        
            ImGui::Text("PPU State:");
            ImGui::Separator();

            ImGui::Columns(2, "ppuregs", false);
            ImGui::Text("Scanline: %d", bus_.ppu.getScanline());
            ImGui::Text("Cycle: %d", bus_.ppu.getCycle());
            ImGui::Text("VRAM Addr: $%04X", bus_.ppu.getVramAddr());
            ImGui::Text("Temp Addr: $%04X", bus_.ppu.getTempAddr());

            ImGui::NextColumn();
            ImGui::Text("Fine X: %d", bus_.ppu.getFineX());
            ImGui::Text("Write Toggle: %s", bus_.ppu.getWriteToggle() ? "1" : "0");
            ImGui::Text("Frame Complete: %s", bus_.ppu.isFrameComplete() ? "Yes" : "No");
            ImGui::Text("NMI Occurred: %s", bus_.ppu.isNMI() ? "Yes" : "No");
            ImGui::Columns(1);

            ImGui::Spacing();
            ImGui::Separator();

            // PPUCTRL ($2000)
            u8 ctrl = bus_.ppu.getCtrl();
            ImGui::Text("PPUCTRL ($2000): $%02X", ctrl);
            ImGui::Text("  Base NT: $%04X", 0x2000 + (ctrl & 0x03) * 0x400);
            ImGui::Text("  VRAM Inc: %s", (ctrl & 0x04) ? "+32" : "+1");
            ImGui::Text("  Sprite PT: $%04X", (ctrl & 0x08) ? 0x1000 : 0x0000);
            ImGui::Text("  BG PT: $%04X", (ctrl & 0x10) ? 0x1000 : 0x0000);
            ImGui::Text("  Sprite Size: %s", (ctrl & 0x20) ? "8x16" : "8x8");
            ImGui::Text("  NMI Enable: %s", (ctrl & 0x80) ? "Yes" : "No");

            ImGui::Spacing();

            // PPUMASK ($2001)
            u8 mask = bus_.ppu.getMask();
            ImGui::Text("PPUMASK ($2001): $%02X", mask);
            ImGui::Text("  Grayscale: %s", (mask & 0x01) ? "Yes" : "No");
            ImGui::Text("  Show BG Left 8: %s", (mask & 0x02) ? "Yes" : "No");
            ImGui::Text("  Show Sprites Left 8: %s", (mask & 0x04) ? "Yes" : "No");
            ImGui::Text("  Show BG: %s", (mask & 0x08) ? "Yes" : "No");
            ImGui::Text("  Show Sprites: %s", (mask & 0x10) ? "Yes" : "No");

            ImGui::Spacing();

            // PPUSTATUS ($2002)
            u8 status = bus_.ppu.getStatus();
            ImGui::Text("PPUSTATUS ($2002): $%02X", status);
            ImGui::Text("  Sprite Overflow: %s", (status & 0x20) ? "Yes" : "No");
            ImGui::Text("  Sprite 0 Hit: %s", (status & 0x40) ? "Yes" : "No");
            ImGui::Text("  VBlank: %s", (status & 0x80) ? "Yes" : "No");

    }
    ImGui::End();
}

void Gui::renderPaletteViewer() {
    ImGui::SetNextWindowSize(ImVec2(320, 200), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Palette Viewer", &showPaletteViewer_)) {
        
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 pos = ImGui::GetCursorScreenPos();
            float boxSize = 24.0f;
            float spacing = 2.0f;

            ImGui::Text("Background Palettes:");
            pos = ImGui::GetCursorScreenPos();
            for (int pal = 0; pal < 4; pal++) {
                for (int col = 0; col < 4; col++) {
                    u16 addr = 0x3F00 + pal * 4 + col;
                    u8 colorIdx = bus_.read(addr);
                    ImU32 color = nesColorToImU32(colorIdx);

                    float x = pos.x + (pal * 4 + col) * (boxSize + spacing);
                    float y = pos.y;

                    drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + boxSize, y + boxSize), color);
                    drawList->AddRect(ImVec2(x, y), ImVec2(x + boxSize, y + boxSize), IM_COL32(128, 128, 128, 255));

                    // Tooltip
                    if (ImGui::IsMouseHoveringRect(ImVec2(x, y), ImVec2(x + boxSize, y + boxSize))) {
                        ImGui::BeginTooltip();
                        ImGui::Text("$%04X: $%02X", addr, colorIdx);
                        ImGui::EndTooltip();
                    }
                }
            }
            ImGui::Dummy(ImVec2(0, boxSize + 10));

            ImGui::Text("Sprite Palettes:");
            pos = ImGui::GetCursorScreenPos();
            for (int pal = 0; pal < 4; pal++) {
                for (int col = 0; col < 4; col++) {
                    u16 addr = 0x3F10 + pal * 4 + col;
                    u8 colorIdx = bus_.read(addr);
                    ImU32 color = nesColorToImU32(colorIdx);

                    float x = pos.x + (pal * 4 + col) * (boxSize + spacing);
                    float y = pos.y;

                    drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + boxSize, y + boxSize), color);
                    drawList->AddRect(ImVec2(x, y), ImVec2(x + boxSize, y + boxSize), IM_COL32(128, 128, 128, 255));

                    if (ImGui::IsMouseHoveringRect(ImVec2(x, y), ImVec2(x + boxSize, y + boxSize))) {
                        ImGui::BeginTooltip();
                        ImGui::Text("$%04X: $%02X", addr, colorIdx);
                        ImGui::EndTooltip();
                    }
                }
            }
            ImGui::Dummy(ImVec2(0, boxSize + 10));

            // Full NES palette reference
            ImGui::Separator();
            ImGui::Text("NES Master Palette:");
            pos = ImGui::GetCursorScreenPos();
            float smallBox = 16.0f;
            for (int row = 0; row < 4; row++) {
                for (int col = 0; col < 16; col++) {
                    int idx = row * 16 + col;
                    ImU32 color = nesColorToImU32(idx);

                    float x = pos.x + col * (smallBox + 1);
                    float y = pos.y + row * (smallBox + 1);

                    drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + smallBox, y + smallBox), color);

                    if (ImGui::IsMouseHoveringRect(ImVec2(x, y), ImVec2(x + smallBox, y + smallBox))) {
                        ImGui::BeginTooltip();
                        ImGui::Text("Index: $%02X", idx);
                        ImGui::EndTooltip();
                    }
                }
            }
            ImGui::Dummy(ImVec2(0, 4 * (smallBox + 1)));

    }
    ImGui::End();
}

void Gui::renderPatternTableViewer() {
    ImGui::SetNextWindowSize(ImVec2(550, 350), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Pattern Tables", &showPatternViewer_)) {
        
            ImGui::SliderInt("Palette", &patternTablePalette_, 0, 7);

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            float scale = 2.0f;
            float tileSize = 8.0f * scale;

            // Two pattern tables: $0000-$0FFF and $1000-$1FFF
            for (int table = 0; table < 2; table++) {
                ImGui::Text("Pattern Table %d ($%04X):", table, table * 0x1000);
                ImVec2 pos = ImGui::GetCursorScreenPos();

                for (int tileY = 0; tileY < 16; tileY++) {
                    for (int tileX = 0; tileX < 16; tileX++) {
                        int tileIdx = tileY * 16 + tileX;
                        u16 tileAddr = table * 0x1000 + tileIdx * 16;

                        for (int row = 0; row < 8; row++) {
                            u8 lo = bus_.cartridge.readChr(tileAddr + row);
                            u8 hi = bus_.cartridge.readChr(tileAddr + row + 8);

                            for (int col = 0; col < 8; col++) {
                                u8 bit = 7 - col;
                                u8 pixel = ((lo >> bit) & 1) | (((hi >> bit) & 1) << 1);

                                // Get color from selected palette
                                u16 palAddr = 0x3F00 + (patternTablePalette_ % 4) * 4;
                                if (patternTablePalette_ >= 4) palAddr = 0x3F10 + (patternTablePalette_ % 4) * 4;
                                u8 colorIdx = bus_.read(palAddr + pixel);
                                ImU32 color = nesColorToImU32(colorIdx);

                                float x = pos.x + (tileX * 8 + col) * scale + table * (128 * scale + 20);
                                float y = pos.y + (tileY * 8 + row) * scale;

                                drawList->AddRectFilled(
                                    ImVec2(x, y),
                                    ImVec2(x + scale, y + scale),
                                    color
                                );
                            }
                        }
                    }
                }

                if (table == 0) {
                    ImGui::SameLine();
                    ImGui::Dummy(ImVec2(128 * scale + 20, 0));
                }
            }
            ImGui::Dummy(ImVec2(0, 128 * scale + 10));

    }
    ImGui::End();
}

void Gui::renderNametableViewer() {
    ImGui::SetNextWindowSize(ImVec2(600, 550), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Nametables", &showNametableViewer_)) {
        
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            float scale = 1.0f;
            ImVec2 pos = ImGui::GetCursorScreenPos();

            // Draw 4 nametables in 2x2 grid
            for (int ntY = 0; ntY < 2; ntY++) {
                for (int ntX = 0; ntX < 2; ntX++) {
                    u16 ntBase = 0x2000 + (ntY * 2 + ntX) * 0x400;

                    for (int tileY = 0; tileY < 30; tileY++) {
                        for (int tileX = 0; tileX < 32; tileX++) {
                            u16 ntAddr = ntBase + tileY * 32 + tileX;
                            u8 tileIdx = bus_.read(ntAddr);

                            // Get attribute
                            u16 atAddr = ntBase + 0x3C0 + (tileY / 4) * 8 + (tileX / 4);
                            u8 attr = bus_.read(atAddr);
                            int shift = ((tileY & 2) ? 4 : 0) + ((tileX & 2) ? 2 : 0);
                            u8 palIdx = (attr >> shift) & 0x03;

                            // Get BG pattern table from PPUCTRL
                            u16 ptBase = (bus_.ppu.getCtrl() & 0x10) ? 0x1000 : 0x0000;
                            u16 tileAddr = ptBase + tileIdx * 16;

                            // Draw tile (simplified - just color blocks)
                            for (int row = 0; row < 8; row++) {
                                u8 lo = bus_.cartridge.readChr(tileAddr + row);
                                u8 hi = bus_.cartridge.readChr(tileAddr + row + 8);

                                for (int col = 0; col < 8; col++) {
                                    u8 bit = 7 - col;
                                    u8 pixel = ((lo >> bit) & 1) | (((hi >> bit) & 1) << 1);

                                    u16 palAddr = 0x3F00 + palIdx * 4 + pixel;
                                    u8 colorIdx = bus_.read(palAddr);
                                    ImU32 color = nesColorToImU32(colorIdx);

                                    float x = pos.x + (ntX * 256 + tileX * 8 + col) * scale;
                                    float y = pos.y + (ntY * 240 + tileY * 8 + row) * scale;

                                    drawList->AddRectFilled(
                                        ImVec2(x, y),
                                        ImVec2(x + scale, y + scale),
                                        color
                                    );
                                }
                            }
                        }
                    }
                }
            }

            ImGui::Dummy(ImVec2(512 * scale, 480 * scale));

    }
    ImGui::End();
}

void Gui::renderOamViewer() {
    ImGui::SetNextWindowSize(ImVec2(500, 450), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("OAM/Sprites", &showOamViewer_)) {
        
            ImGui::Text("OAM Sprite Data (64 sprites):");
            ImGui::Separator();

            if (ImGui::BeginTable("OAMTable", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg, ImVec2(0, 320))) {
                ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 30);
                ImGui::TableSetupColumn("Y", ImGuiTableColumnFlags_WidthFixed, 45);
                ImGui::TableSetupColumn("Tile", ImGuiTableColumnFlags_WidthFixed, 50);
                ImGui::TableSetupColumn("Attr", ImGuiTableColumnFlags_WidthFixed, 50);
                ImGui::TableSetupColumn("X", ImGuiTableColumnFlags_WidthFixed, 45);
                ImGui::TableSetupColumn("Flags", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableHeadersRow();

                const u8* oam = bus_.ppu.getOam();
                for (int i = 0; i < 64; i++) {
                    u8 y = oam[i * 4 + 0];
                    u8 tile = oam[i * 4 + 1];
                    u8 attr = oam[i * 4 + 2];
                    u8 x = oam[i * 4 + 3];

                    // Skip sprites off-screen (Y >= 0xEF typically means hidden)
                    bool visible = (y < 0xEF);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); 
                    if (visible) {
                        ImGui::Text("%02d", i);
                    } else {
                        ImGui::TextDisabled("%02d", i);
                    }

                    ImGui::TableNextColumn(); 
                    if (visible) ImGui::Text("$%02X", y); else ImGui::TextDisabled("$%02X", y);

                    ImGui::TableNextColumn(); 
                    if (visible) ImGui::Text("$%02X", tile); else ImGui::TextDisabled("$%02X", tile);

                    ImGui::TableNextColumn(); 
                    if (visible) ImGui::Text("$%02X", attr); else ImGui::TextDisabled("$%02X", attr);

                    ImGui::TableNextColumn(); 
                    if (visible) ImGui::Text("$%02X", x); else ImGui::TextDisabled("$%02X", x);

                    ImGui::TableNextColumn();
                    // Decode attribute flags
                    char flags[32];
                    snprintf(flags, sizeof(flags), "Pal:%d %s %s %s",
                        attr & 0x03,
                        (attr & 0x20) ? "BG" : "FG",
                        (attr & 0x40) ? "FlipH" : "",
                        (attr & 0x80) ? "FlipV" : "");
                    if (visible) ImGui::Text("%s", flags); else ImGui::TextDisabled("%s", flags);
                }
                ImGui::EndTable();
            }

            // Sprite size info
            bool is8x16 = (bus_.ppu.getCtrl() & 0x20) != 0;
            ImGui::Text("Sprite Size: %s", is8x16 ? "8x16" : "8x8");
            if (!is8x16) {
                ImGui::Text("Sprite Pattern Table: $%04X", (bus_.ppu.getCtrl() & 0x08) ? 0x1000 : 0x0000);
            }

    }
    ImGui::End();
}

void Gui::renderMemoryViewer() {
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Memory Viewer", &showMemoryViewer_)) {
        
            // Memory type selector
            ImGui::RadioButton("CPU Memory", &memoryViewType_, 0);
            ImGui::SameLine();
            ImGui::RadioButton("PPU Memory", &memoryViewType_, 1);

            // Address input
            ImGui::SetNextItemWidth(100);
            if (ImGui::InputText("Address", memorySearchAddr_, sizeof(memorySearchAddr_), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
                int addr;
                if (sscanf(memorySearchAddr_, "%x", &addr) == 1) {
                    memoryViewAddress_ = addr & 0xFFF0;  // Align to 16
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Go")) {
                int addr;
                if (sscanf(memorySearchAddr_, "%x", &addr) == 1) {
                    memoryViewAddress_ = addr & 0xFFF0;
                }
            }

            ImGui::Separator();

            // Memory display
            ImGui::BeginChild("MemoryView", ImVec2(0, 0), ImGuiChildFlags_Borders);

            int maxAddr = (memoryViewType_ == 0) ? 0xFFFF : 0x3FFF;
            int displayLines = 16;

            for (int line = 0; line < displayLines; line++) {
                int lineAddr = (memoryViewAddress_ + line * 16) & maxAddr;
                ImGui::Text("%04X: ", lineAddr);
                ImGui::SameLine();

                // Hex display
                for (int i = 0; i < 16; i++) {
                    int addr = (lineAddr + i) & maxAddr;
                    u8 val;
                    if (memoryViewType_ == 0) {
                        val = bus_.read(addr);
                    } else {
                        val = bus_.cartridge.readChr(addr);
                    }
                    ImGui::Text("%02X", val);
                    ImGui::SameLine();
                    if (i == 7) {
                        ImGui::Text(" ");
                        ImGui::SameLine();
                    }
                }

                // ASCII display
                ImGui::Text(" |");
                ImGui::SameLine();
                for (int i = 0; i < 16; i++) {
                    int addr = (lineAddr + i) & maxAddr;
                    u8 val;
                    if (memoryViewType_ == 0) {
                        val = bus_.read(addr);
                    } else {
                        val = bus_.cartridge.readChr(addr);
                    }
                    char c = (val >= 32 && val < 127) ? static_cast<char>(val) : '.';
                    ImGui::Text("%c", c);
                    ImGui::SameLine();
                }
                ImGui::Text("|");
            }

            ImGui::EndChild();

            // Navigation
            if (ImGui::Button("<<")) memoryViewAddress_ = std::max(0, memoryViewAddress_ - 0x100);
            ImGui::SameLine();
            if (ImGui::Button("<")) memoryViewAddress_ = std::max(0, memoryViewAddress_ - 0x10);
            ImGui::SameLine();
            if (ImGui::Button(">")) memoryViewAddress_ = std::min(maxAddr - 0xFF, memoryViewAddress_ + 0x10);
            ImGui::SameLine();
            if (ImGui::Button(">>")) memoryViewAddress_ = std::min(maxAddr - 0xFF, memoryViewAddress_ + 0x100);

    }
    ImGui::End();
}

void Gui::renderApuViewer() {
    ImGui::SetNextWindowSize(ImVec2(350, 350), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("APU Viewer", &showApuViewer_)) {
        
            ImGui::Text("Audio Processing Unit Status:");
            ImGui::Separator();

            // Pulse 1
            auto p1 = bus_.apu.getPulse1Status();
            ImGui::Text("Pulse 1:");
            ImGui::ProgressBar(p1.enabled ? (p1.volume / 15.0f) : 0.0f, ImVec2(100, 14));
            ImGui::SameLine();
            ImGui::Text("Vol:%d Per:%d Len:%d %s", p1.volume, p1.period, p1.length, p1.enabled ? "ON" : "OFF");

            // Pulse 2
            auto p2 = bus_.apu.getPulse2Status();
            ImGui::Text("Pulse 2:");
            ImGui::ProgressBar(p2.enabled ? (p2.volume / 15.0f) : 0.0f, ImVec2(100, 14));
            ImGui::SameLine();
            ImGui::Text("Vol:%d Per:%d Len:%d %s", p2.volume, p2.period, p2.length, p2.enabled ? "ON" : "OFF");

            // Triangle
            auto tri = bus_.apu.getTriangleStatus();
            ImGui::Text("Triangle:");
            ImGui::ProgressBar(tri.enabled ? 0.5f : 0.0f, ImVec2(100, 14));
            ImGui::SameLine();
            ImGui::Text("Per:%d Len:%d %s", tri.period, tri.length, tri.enabled ? "ON" : "OFF");

            // Noise
            auto noise = bus_.apu.getNoiseStatus();
            ImGui::Text("Noise:");
            ImGui::ProgressBar(noise.enabled ? (noise.volume / 15.0f) : 0.0f, ImVec2(100, 14));
            ImGui::SameLine();
            ImGui::Text("Vol:%d Per:%d Len:%d %s", noise.volume, noise.period, noise.length, noise.enabled ? "ON" : "OFF");

            // DMC
            auto dmc = bus_.apu.getDMCStatus();
            ImGui::Text("DMC:");
            ImGui::ProgressBar(dmc.enabled ? (dmc.volume / 127.0f) : 0.0f, ImVec2(100, 14));
            ImGui::SameLine();
            ImGui::Text("Out:%d Rate:%d %s", dmc.volume, dmc.period, dmc.enabled ? "ON" : "OFF");

            ImGui::Separator();
            ImGui::Text("Frame Counter Mode: %d", bus_.apu.getFrameCounterMode());
            ImGui::Text("IRQ Inhibit: %s", bus_.apu.getIrqInhibit() ? "Yes" : "No");
            ImGui::Text("IRQ Pending: %s", bus_.apu.isIRQ() ? "Yes" : "No");

    }
    ImGui::End();
}

void Gui::renderGameGenie() {
    ImGui::SetNextWindowSize(ImVec2(350, 300), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Game Genie", &showGameGenie_)) {
        ImGui::Text("Enter Game Genie Codes:");
        ImGui::Separator();

        // Input field
        if (ImGui::InputText("Code", ggInput_, sizeof(ggInput_), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsUppercase)) {
            if (ggInput_[0]) {
                ggCodes_.push_back(ggInput_);
                ggSubmitted_ = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Add")) {
            if (ggInput_[0]) {
                ggCodes_.push_back(ggInput_);
                ggSubmitted_ = true;
            }
        }

        ImGui::Spacing();
        ImGui::Text("Active Codes:");
        ImGui::BeginChild("GGCodes", ImVec2(0, 150), ImGuiChildFlags_Borders);

        int toRemove = -1;
        for (size_t i = 0; i < ggCodes_.size(); i++) {
            ImGui::Text("%s", ggCodes_[i].c_str());
            ImGui::SameLine();
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::SmallButton("X")) {
                toRemove = static_cast<int>(i);
            }
            ImGui::PopID();
        }

        if (toRemove >= 0) {
            ggCodes_.erase(ggCodes_.begin() + toRemove);
        }

        ImGui::EndChild();

        ImGui::Spacing();
        ImGui::TextWrapped("Format: 6 or 8 character codes (e.g., SXIOPO or SXIOPOTV)");
    }
    ImGui::End();
}

void Gui::renderCartridgeInfo() {
    ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Cartridge Info", &showCartridgeInfo_)) {
        if (bus_.cartridge.isLoaded()) {
            ImGui::Text("Cartridge Information:");
            ImGui::Separator();

            ImGui::Text("Mapper: %d (%s)", 
                bus_.cartridge.getMapperNumber(),
                bus_.cartridge.getMapperName());

            const char* mirrorNames[] = { "Horizontal", "Vertical", "Single (Lower)", "Single (Upper)", "Four-Screen" };
            int mirrorIdx = static_cast<int>(bus_.cartridge.getMirroring());
            if (mirrorIdx >= 0 && mirrorIdx < 5) {
                ImGui::Text("Mirroring: %s", mirrorNames[mirrorIdx]);
            }

            ImGui::Text("Battery: %s", bus_.cartridge.hasBattery() ? "Yes" : "No");
            ImGui::Text("PRG ROM: %zu KB", bus_.cartridge.getPrgRom().size() / 1024);
            ImGui::Text("CHR ROM: %zu KB", bus_.cartridge.getChrRom().size() / 1024);
        } else {
            ImGui::Text("No cartridge loaded");
        }
    }
    ImGui::End();
}

void Gui::renderEmulatorWindow() {
    ImGui::SetNextWindowSize(ImVec2(540, 520), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Emulator Screen", &showEmulatorWindow_, ImGuiWindowFlags_NoScrollbar)) {
        if (emulatorTextureInitialized_) {
            ImVec2 contentSize = ImGui::GetContentRegionAvail();
            sf::Vector2u textureSize = emulatorTexture_.getSize();

            // Calculate scale to fit while maintaining aspect ratio
            float scaleX = contentSize.x / static_cast<float>(textureSize.x);
            float scaleY = contentSize.y / static_cast<float>(textureSize.y);
            float scale = std::min(scaleX, scaleY);

            float displayWidth = static_cast<float>(textureSize.x) * scale;
            float displayHeight = static_cast<float>(textureSize.y) * scale;

            // Center the image
            float offsetX = (contentSize.x - displayWidth) / 2.0f;
            float offsetY = (contentSize.y - displayHeight) / 2.0f;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offsetY);

            // Use ImGui-SFML's texture wrapper
            ImGui::Image(emulatorTexture_, sf::Vector2f(displayWidth, displayHeight));
        }
    }
    ImGui::End();
}

void Gui::renderFileDialog() {
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Load ROM", &showFileDialog_)) {
        // Current path display
        ImGui::Text("Path: %s", currentPath_.c_str());

        // Path input
        strncpy(pathInput_, currentPath_.c_str(), sizeof(pathInput_) - 1);
        pathInput_[sizeof(pathInput_) - 1] = '\0';
        if (ImGui::InputText("##path", pathInput_, sizeof(pathInput_), ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (std::filesystem::exists(pathInput_) && std::filesystem::is_directory(pathInput_)) {
                currentPath_ = pathInput_;
                directoryContents_.clear();
                selectedFileIndex_ = -1;
            }
        }

        // Refresh directory listing if needed
        if (directoryContents_.empty()) {
            try {
                directoryContents_.push_back("..");  // Parent directory
                for (const auto& entry : std::filesystem::directory_iterator(currentPath_)) {
                    std::string name = entry.path().filename().string();
                    if (entry.is_directory()) {
                        directoryContents_.push_back("[DIR] " + name);
                    } else {
                        std::string ext = entry.path().extension().string();
                        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                        if (ext == ".nes" || ext == ".rom") {
                            directoryContents_.push_back(name);
                        }
                    }
                }
            } catch (...) {
                directoryContents_.push_back("(Error reading directory)");
            }
        }

        // File listing
        ImGui::BeginChild("FileList", ImVec2(0, -60), ImGuiChildFlags_Borders);
        for (size_t i = 0; i < directoryContents_.size(); i++) {
            bool isSelected = (selectedFileIndex_ == static_cast<int>(i));
            if (ImGui::Selectable(directoryContents_[i].c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick)) {
                selectedFileIndex_ = static_cast<int>(i);

                if (ImGui::IsMouseDoubleClicked(0)) {
                    std::string selected = directoryContents_[i];
                    if (selected == "..") {
                        std::filesystem::path p(currentPath_);
                        if (p.has_parent_path()) {
                            currentPath_ = p.parent_path().string();
                            directoryContents_.clear();
                            selectedFileIndex_ = -1;
                        }
                    } else if (selected.substr(0, 6) == "[DIR] ") {
                        std::string dirName = selected.substr(6);
                        currentPath_ = (std::filesystem::path(currentPath_) / dirName).string();
                        directoryContents_.clear();
                        selectedFileIndex_ = -1;
                    } else {
                        // It's a file - load it
                        std::string fullPath = (std::filesystem::path(currentPath_) / selected).string();
                        pendingAction_.type = GuiAction::LoadRom;
                        pendingAction_.romPath = fullPath;
                        showFileDialog_ = false;
                    }
                }
            }
        }
        ImGui::EndChild();

        // Buttons
        if (ImGui::Button("Open", ImVec2(80, 0))) {
            if (selectedFileIndex_ >= 0 && selectedFileIndex_ < static_cast<int>(directoryContents_.size())) {
                std::string selected = directoryContents_[selectedFileIndex_];
                if (selected != ".." && selected.substr(0, 6) != "[DIR] ") {
                    std::string fullPath = (std::filesystem::path(currentPath_) / selected).string();
                    pendingAction_.type = GuiAction::LoadRom;
                    pendingAction_.romPath = fullPath;
                    showFileDialog_ = false;
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 0))) {
            showFileDialog_ = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Refresh", ImVec2(80, 0))) {
            directoryContents_.clear();
            selectedFileIndex_ = -1;
        }
    }
    ImGui::End();
}

void Gui::renderConsole() {
    console_.render(&showConsole_);
}

bool Gui::isMenuVisible() const {
    return menuVisible_;
}

void Gui::toggleMenu() {
    menuVisible_ = !menuVisible_;
}

bool Gui::getGameGenieCode(std::string& code) {
    if (ggSubmitted_ && !ggCodes_.empty()) {
        code = ggCodes_.back();
        ggInput_[0] = '\0';
        ggSubmitted_ = false;
        return true;
    }
    return false;
}

GuiAction Gui::pollAction() {
    GuiAction action = pendingAction_;
    pendingAction_ = GuiAction{};
    return action;
}
