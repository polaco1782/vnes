#include <iostream>
#include <cstring>
#include "bus.h"
#include "cartridge.h"
#include "debugger.h"
#include "display.h"
#include "input.h"
#include "sound.h"

void printUsage(const char* program)
{
    std::cout << "VNES - Minimal NES Emulator" << std::endl;
    std::cout << "Usage: " << program << " [options] <rom.nes>" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -d, --debug    Start in debugger mode" << std::endl;
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    // Parse arguments
    bool debug_mode = false;
    const char* rom_file = nullptr;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--debug") == 0) {
            debug_mode = true;
        } else {
            rom_file = argv[i];
        }
    }

    if (!rom_file) {
        printUsage(argv[0]);
        return 1;
    }

    // Load ROM
    Cartridge cartridge;
    if (!cartridge.load(rom_file)) {
        return 1;
    }

    // Create system bus and connect cartridge
    Bus bus;
    bus.connect(&cartridge);
    bus.reset();

    std::cout << "\nSystem initialized!" << std::endl;
    std::cout << "CPU PC: 0x" << std::hex << bus.cpu.getPC() << std::dec << std::endl;

    // Normal execution with display
    std::cout << "\nStarting emulation..." << std::endl;
    std::cout << "Controls: Arrow Keys, Z=A, X=B, Enter=Start, RShift=Select" << std::endl;
    std::cout << "Press ESC to enter debugger, or close window to quit" << std::endl;
    
    Display display("VNES - NES Emulator");
    Debugger debugger;
    Input input;
    Sound sound;
    
    bus.connectInput(&input);
    debugger.connect(&bus);
    sound.connect(&bus.apu);
    bus.apu.connect(&sound);
    sound.start();
    
    bool in_debugger = false;
    
    while (display.isOpen()) {
        if (in_debugger) {
            // Run debugger
            std::cout << "\nEntering debugger (type 'c' or 'continue' to resume emulation, 'q' to quit)" << std::endl;
            debugger.run();
            in_debugger = false;
            std::cout << "Resuming emulation..." << std::endl;
        } else {
            // Process events
            display.pollEvents();
            
            // Check for ESC to enter debugger
            if (display.wasEscapePressed() && debug_mode) {
                in_debugger = true;
                continue;
            }
            
            // Update input state
            input.updateFromKeyboard();
            
            // Run one frame
            while (!bus.isFrameComplete()) {
                bus.clock();
            }
            bus.clearFrameComplete();
            
            // Update display
            display.update(bus.ppu.getFramebuffer());
        }
    }
    
    std::cout << "Emulation stopped." << std::endl;

    return 0;
}
