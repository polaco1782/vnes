#pragma once
#include <SFML/Graphics.hpp>
#include <imgui.h>
#include <ImGui-SFML.h>
#include <string>
#include <vector>
#include <functional>
#include "types.h"
#include "gui_console.h"

// Forward declarations
class Bus;
class CPU;
class PPU;
class APU;
class Cartridge;

// GUI Actions that main loop should handle
struct GuiAction {
    enum Type {
        None,
        LoadRom,
        Reset,
        Pause,
        Resume,
        Step,
        StepFrame,
        Quit
    };
    Type type = None;
    std::string romPath;  // For LoadRom
};

class Gui {
public:
    explicit Gui(Bus& bus);
    ~Gui();
    void initialize(sf::RenderWindow& window);
    void processEvent(sf::RenderWindow& window, const sf::Event& event);
    void update(sf::RenderWindow& window, float dt);
    void render(sf::RenderWindow& window);
    bool isMenuVisible() const;
    void toggleMenu();

    // Update emulator screen texture (called each frame)
    void updateEmulatorTexture(const u32* framebuffer, unsigned width, unsigned height);

    // Returns true if a new Game Genie code was entered
    bool getGameGenieCode(std::string& code);

    // Check for pending actions
    GuiAction pollAction();

    // Emulation control
    bool isPaused() const { return paused_; }
    void setPaused(bool paused) { paused_ = paused; }

    // Check if emulator screen should be rendered in window
    bool isEmulatorInWindow() const { return showEmulatorWindow_; }

    // Check if emulator texture needs updating (GUI window visible)
    bool needsEmulatorTextureUpdate() const { return menuVisible_ && showEmulatorWindow_; }

    // Get console for breakpoint checking
    GuiConsole& getConsole() { return console_; }

private:
    void renderMenuBar();
    void renderCpuDebugger();
    void renderPpuViewer();
    void renderPaletteViewer();
    void renderPatternTableViewer();
    void renderNametableViewer();
    void renderOamViewer();
    void renderMemoryViewer();
    void renderApuViewer();
    void renderGameGenie();
    void renderCartridgeInfo();
    void renderEmulatorWindow();
    void renderFileDialog();
    void renderConsole();

    // Helper to convert NES color index to ImGui color
    ImU32 nesColorToImU32(u8 colorIndex) const;

    // Disassemble instructions
    std::string disassembleInstruction(u16 addr, int& length);

    bool menuVisible_;
    bool paused_;

    // Window visibility flags
    bool showCpuDebugger_;
    bool showPpuViewer_;
    bool showPaletteViewer_;
    bool showPatternViewer_;
    bool showNametableViewer_;
    bool showOamViewer_;
    bool showMemoryViewer_;
    bool showApuViewer_;
    bool showGameGenie_;
    bool showCartridgeInfo_;
    bool showEmulatorWindow_;
    bool showFileDialog_;
    bool showConsole_;

    // Game Genie state
    char ggInput_[32];
    bool ggSubmitted_;
    std::vector<std::string> ggCodes_;

    // Memory viewer state
    int memoryViewAddress_;
    int memoryViewType_;  // 0=CPU, 1=PPU
    char memorySearchAddr_[16];

    // Pattern table viewer state
    int patternTablePalette_;

    // Emulator components
    Bus& bus_;

    // Emulator screen texture for windowed mode
    sf::Texture emulatorTexture_;
    bool emulatorTextureInitialized_;

    // Pending action
    GuiAction pendingAction_;

    // File dialog state
    std::string currentPath_;
    std::vector<std::string> directoryContents_;
    int selectedFileIndex_;
    char pathInput_[512];

    // Debugger console
    GuiConsole console_;
};
