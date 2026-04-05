#include <iostream>
#include <cstring>
#include "bus.h"
#include "cartridge.h"
#include "display.h"
#include "input.h"
#include "sound.h"
#include "web_server.h"
#include "gui.h"

void printUsage(const char* program)
{
    std::cout << "VNES - Minimal NES Emulator" << std::endl;
    std::cout << "Usage: " << program << " [options] [rom.nes]" << std::endl;
    std::cout << std::endl;
    std::cout << "If no ROM is specified, use File->Load ROM in the GUI (press ESC)" << std::endl;
}

int main(int argc, char* argv[])
{
    // Parse arguments
    const char* rom_file = nullptr;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        }
        rom_file = argv[i];
    }

    // Create system bus
    Bus bus;
    bool romLoaded = false;

    // Load ROM if provided
    if (rom_file) {
        if (!bus.loadCartridge(rom_file)) {
            std::cerr << "Failed to load ROM: " << rom_file << std::endl;
            std::cerr << "Starting without ROM - use GUI to load" << std::endl;
        } else {
            romLoaded = true;
            bus.reset();
            std::cout << "\nSystem initialized!" << std::endl;
            std::cout << "CPU PC: 0x" << std::hex << bus.cpu.getPC() << std::dec << std::endl;
        }
    }

    // Start web server
    WebServer web;
    web.start(18080);

    // Normal execution with display
    std::cout << "\nStarting emulation..." << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  Movement: Arrow Keys or WASD" << std::endl;
    std::cout << "  A Button: Z or J" << std::endl;
    std::cout << "  B Button: X or K" << std::endl;
    std::cout << "  Start: Enter or Space" << std::endl;
    std::cout << "  Select: Shift" << std::endl;
    std::cout << "  (Multiple key bindings provided to avoid keyboard ghosting)" << std::endl;
    std::cout << "Press ESC to toggle GUI menu" << std::endl;

    Display display("VNES - NES Emulator", bus);
    sf::RenderWindow& window = display.getWindow();

    // Emulation state
    bool paused = !romLoaded;  // Start paused if no ROM

    // If no ROM loaded, show the GUI menu
    if (!romLoaded) {
        display.getGui().toggleMenu();
        display.getGui().setPaused(true);
    }

    while (display.isOpen()) {
        // Process events (handled by Display, which forwards to GUI)
        display.pollEvents();

        // Poll GUI actions
        GuiAction action = display.getGui().pollAction();
        switch (action.type) {
            case GuiAction::LoadRom:
                std::cout << "Loading ROM: " << action.romPath << std::endl;
                if (bus.loadCartridge(action.romPath)) {
                    bus.reset();
                    romLoaded = true;
                    paused = false;
                    display.getGui().setPaused(false);
                    std::cout << "ROM loaded successfully!" << std::endl;
                    std::cout << "CPU PC: 0x" << std::hex << bus.cpu.getPC() << std::dec << std::endl;
                } else {
                    std::cerr << "Failed to load ROM: " << action.romPath << std::endl;
                }
                break;

            case GuiAction::Reset:
                if (romLoaded) {
                    bus.reset();
                    std::cout << "System reset!" << std::endl;
                }
                break;

            case GuiAction::Pause:
                paused = true;
                break;

            case GuiAction::Resume:
                paused = false;
                break;

            case GuiAction::Step:
                if (romLoaded && paused) {
                    bus.clock();
                }
                break;

            case GuiAction::StepFrame:
                if (romLoaded && paused) {
                    while (!bus.ppu.isFrameComplete()) {
                        bus.clock();
                    }
                    bus.ppu.clearFrameComplete();
                }
                break;

            case GuiAction::Quit:
                window.close();
                break;

            case GuiAction::None:
            default:
                break;
        }

        // Sync pause state with GUI
        paused = display.getGui().isPaused();

        // Update input state
        bus.updateInput();

        // Run one frame (only if ROM loaded and not paused)
        if (romLoaded && !paused) {
            while (!bus.ppu.isFrameComplete()) {
                bus.clock();
            }
            bus.ppu.clearFrameComplete();

            // Notify cartridge that frame is complete (for SRAM auto-save)
            bus.cartridge.signalFrameComplete();
        }

        // Update display
        if (romLoaded) {
            display.update(bus.ppu.getFramebuffer());
        }

        // Handle Game Genie input from GUI (forwarded by Display)
        std::string ggcode;
        if (display.getGameGenieCode(ggcode)) {
            if (romLoaded) {
                if (!bus.cartridge.addGGCode(ggcode)) {
                    std::cerr << "Failed to add Game Genie code: " << ggcode << std::endl;
                }
                else {
                    std::cout << "Game Genie code added: " << ggcode << std::endl;
                }
            } else {
                std::cerr << "Cannot add Game Genie code: no ROM loaded" << std::endl;
            }
        }
        // Present frame (display sprite + ImGui on top)
        display.present();
    }

    // Final SRAM flush on exit
    if (romLoaded) {
        bus.cartridge.flushSRAM();
    }
    web.stop();

    std::cout << "Emulation stopped." << std::endl;

    return 0;
}
