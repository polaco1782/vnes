#pragma once

#include <imgui.h>
#include <string>
#include <vector>
#include <deque>
#include <set>
#include <optional>
#include <functional>
#include "types.h"

class Bus;

// Console output line with optional color
struct ConsoleLine {
    std::string text;
    ImVec4 color;
    
    ConsoleLine(const std::string& t, ImVec4 c = ImVec4(1.0f, 1.0f, 1.0f, 1.0f))
        : text(t), color(c) {}
};

// GUI Console - provides debugger functionality in an ImGui window
class GuiConsole {
public:
    explicit GuiConsole(Bus& bus);
    void render(bool* open);
    
    // Step execution (called from main loop when stepping)
    void step(int count = 1);
    void stepFrame();
    
    // Check if running (for continue mode)
    bool isRunning() const { return running_; }
    void stopRunning() { running_ = false; }
    
    // Breakpoint management
    const std::set<u16>& getBreakpoints() const { return breakpoints_; }
    bool hasBreakpoint(u16 addr) const { return breakpoints_.count(addr) > 0; }

private:
    // Command processing
    void executeCommand(const std::string& cmdLine);
    std::vector<std::string_view> tokenize(const std::string& line);
    
    // Commands
    void cmdHelp();
    void cmdStep(int count = 1);
    void cmdContinue();
    void cmdRegisters();
    void cmdMemory(u16 addr, int count = 64);
    void cmdDisassemble(u16 addr, int count = 10);
    void cmdBreakpoint(u16 addr);
    void cmdDeleteBreakpoint(u16 addr);
    void cmdListBreakpoints();
    void cmdStack();
    void cmdReset();
    void cmdWrite(u16 addr, u8 value);
    void cmdSetCpu(std::string_view reg, std::string_view value);
    void cmdPpu();
    void cmdSetPpu(std::string_view reg, std::string_view value);
    void cmdApu();
    void cmdIo();
    void cmdClear();
    
    // Disassembly
    std::string disassembleInstruction(u16 addr, int& length);
    std::string generatePseudoC(u8 opcode, int mode, u8 lo, u8 hi, u16 addr);
    
    // Helpers
    std::optional<u16> parseAddress(std::string_view str);
    bool parseValue8(std::string_view str, u8& val);
    bool parseValue16(std::string_view str, u16& val);
    void saveRegisters();

    // Output functions
    void print(const std::string& text);
    void printColored(const std::string& text, ImVec4 color);
    void printError(const std::string& text);
    void printHighlight(const std::string& text);
    void printInfo(const std::string& text);
    void printRegisters();

    // State
    Bus& bus_;
    bool running_;
    
    // Output buffer
    std::deque<ConsoleLine> outputLines_;
    static constexpr size_t maxLines_ = 1000;
    
    // Input
    char inputBuf_[256];
    bool scrollToBottom_;
    bool focusInput_;
    
    // Command history
    std::vector<std::string> history_;
    int historyPos_;
    
    // Breakpoints
    std::set<u16> breakpoints_;
    
    // Previous register state for change highlighting
    u16 prevPc_;
    u8 prevSp_;
    u8 prevA_;
    u8 prevX_;
    u8 prevY_;
    u8 prevStatus_;
    
    // Colors
    static constexpr ImVec4 colorWhite = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    static constexpr ImVec4 colorRed = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
    static constexpr ImVec4 colorGreen = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
    static constexpr ImVec4 colorYellow = ImVec4(1.0f, 1.0f, 0.4f, 1.0f);
    static constexpr ImVec4 colorCyan = ImVec4(0.4f, 1.0f, 1.0f, 1.0f);
    static constexpr ImVec4 colorBlue = ImVec4(0.4f, 0.6f, 1.0f, 1.0f);
    static constexpr ImVec4 colorGray = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
};
