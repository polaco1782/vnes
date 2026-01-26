#include "gui_console.h"
#include "bus.h"
#include "cpu.h"
#include "ppu.h"
#include "apu.h"
#include "cartridge.h"
#include "util.h"
#include "disasm.h"
#include <cstdio>
#include <charconv>
#include <algorithm>
#include <sstream>
#include <iomanip>

using vnes::util::parseInteger;
using vnes::util::hexByte;
using vnes::util::hexWord;
using namespace vnes::disasm;

GuiConsole::GuiConsole(Bus& bus)
    : bus_(bus)
    , running_(false)
    , scrollToBottom_(true)
    , focusInput_(true)
    , historyPos_(-1)
    , prevPc_(0), prevSp_(0), prevA_(0), prevX_(0), prevY_(0), prevStatus_(0)
{
    inputBuf_[0] = '\0';
    print("=== VNES Debugger Console ===");
    print("Type 'help' for available commands");
    print("");
    saveRegisters();
}

void GuiConsole::print(const std::string& text) {
    printColored(text, colorWhite);
}

void GuiConsole::printColored(const std::string& text, ImVec4 color) {
    outputLines_.emplace_back(text, color);
    while (outputLines_.size() > maxLines_) {
        outputLines_.pop_front();
    }
    scrollToBottom_ = true;
}

void GuiConsole::printError(const std::string& text) {
    printColored(text, colorRed);
}

void GuiConsole::printHighlight(const std::string& text) {
    printColored(text, colorYellow);
}

void GuiConsole::printInfo(const std::string& text) {
    printColored(text, colorCyan);
}

void GuiConsole::render(bool* open) {
    ImGui::SetNextWindowSize(ImVec2(700, 400), ImGuiCond_FirstUseEver);
    
    if (!ImGui::Begin("Debugger Console", open)) {
        ImGui::End();
        return;
    }
    
    // Output area
    float footerHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footerHeight), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
    
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
    for (const auto& line : outputLines_) {
        ImGui::PushStyleColor(ImGuiCol_Text, line.color);
        ImGui::TextUnformatted(line.text.c_str());
        ImGui::PopStyleColor();
    }
    
    if (scrollToBottom_) {
        ImGui::SetScrollHereY(1.0f);
        scrollToBottom_ = false;
    }
    
    ImGui::PopStyleVar();
    ImGui::EndChild();
    
    // Input line
    ImGui::Separator();
    
    ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue 
                                   | ImGuiInputTextFlags_CallbackHistory
                                   | ImGuiInputTextFlags_CallbackCompletion;
    
    bool reclaimFocus = false;
    ImGui::PushItemWidth(-1);
    
    if (ImGui::InputText("##Input", inputBuf_, sizeof(inputBuf_), inputFlags,
        [](ImGuiInputTextCallbackData* data) -> int {
            GuiConsole* console = static_cast<GuiConsole*>(data->UserData);
            if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
                if (data->EventKey == ImGuiKey_UpArrow) {
                    if (console->historyPos_ < (int)console->history_.size() - 1) {
                        console->historyPos_++;
                        std::string& historyStr = console->history_[console->history_.size() - 1 - console->historyPos_];
                        data->DeleteChars(0, data->BufTextLen);
                        data->InsertChars(0, historyStr.c_str());
                    }
                } else if (data->EventKey == ImGuiKey_DownArrow) {
                    if (console->historyPos_ > 0) {
                        console->historyPos_--;
                        std::string& historyStr = console->history_[console->history_.size() - 1 - console->historyPos_];
                        data->DeleteChars(0, data->BufTextLen);
                        data->InsertChars(0, historyStr.c_str());
                    } else if (console->historyPos_ == 0) {
                        console->historyPos_ = -1;
                        data->DeleteChars(0, data->BufTextLen);
                    }
                }
            }
            return 0;
        }, this))
    {
        std::string cmd(inputBuf_);
        if (!cmd.empty()) {
            printColored("> " + cmd, colorGreen);
            executeCommand(cmd);
            
            // Add to history (avoid duplicates)
            if (history_.empty() || history_.back() != cmd) {
                history_.push_back(cmd);
            }
            historyPos_ = -1;
        }
        inputBuf_[0] = '\0';
        reclaimFocus = true;
    }
    
    ImGui::PopItemWidth();
    
    // Auto-focus on window appearing
    if (focusInput_ || reclaimFocus) {
        ImGui::SetKeyboardFocusHere(-1);
        focusInput_ = false;
    }
    
    ImGui::End();
}

std::vector<std::string_view> GuiConsole::tokenize(const std::string& line) {
    std::vector<std::string_view> tokens;
    std::string_view sv(line);
    size_t i = 0, n = sv.size();
    while (i < n) {
        while (i < n && std::isspace(static_cast<unsigned char>(sv[i]))) ++i;
        if (i >= n) break;
        size_t j = i;
        while (j < n && !std::isspace(static_cast<unsigned char>(sv[j]))) ++j;
        tokens.emplace_back(sv.substr(i, j - i));
        i = j;
    }
    return tokens;
}

std::optional<u16> GuiConsole::parseAddress(std::string_view str) {
    if (str.empty()) return std::nullopt;
    auto opt = vnes::util::parseInteger(str);
    if (!opt) return std::nullopt;
    if (*opt > 0xFFFF) return std::nullopt;
    return static_cast<u16>(*opt);
}

bool GuiConsole::parseValue8(std::string_view str, u8& val) {
    if (auto opt = parseAddress(str)) {
        val = static_cast<u8>((*opt) & 0xFF);
        return true;
    }
    return false;
}

bool GuiConsole::parseValue16(std::string_view str, u16& val) {
    if (auto opt = parseAddress(str)) {
        val = *opt;
        return true;
    }
    return false;
}

void GuiConsole::saveRegisters() {
    // bus_ is always valid (reference)
    prevPc_ = bus_.cpu.getPC();
    prevSp_ = bus_.cpu.getSP();
    prevA_ = bus_.cpu.getA();
    prevX_ = bus_.cpu.getX();
    prevY_ = bus_.cpu.getY();
    prevStatus_ = bus_.cpu.getStatus();
}

void GuiConsole::printRegisters() {
    // bus_ is always valid (reference)
    
    u16 pc = bus_.cpu.getPC();
    u8 sp = bus_.cpu.getSP();
    u8 a = bus_.cpu.getA();
    u8 x = bus_.cpu.getX();
    u8 y = bus_.cpu.getY();
    u8 status = bus_.cpu.getStatus();
    
    std::ostringstream ss;
    ss << "PC:" << hexWord(pc) << "  ";
    ss << "A:" << hexByte(a) << "  ";
    ss << "X:" << hexByte(x) << "  ";
    ss << "Y:" << hexByte(y) << "  ";
    ss << "SP:" << hexByte(sp) << "  ";
    ss << "P:" << vnes::util::formatFlags(status) << " [" << hexByte(status) << "]";
    
    // Highlight if any register changed
    if (pc != prevPc_ || sp != prevSp_ || a != prevA_ || x != prevX_ || y != prevY_ || status != prevStatus_) {
        printHighlight(ss.str());
    } else {
        print(ss.str());
    }
}

void GuiConsole::executeCommand(const std::string& cmdLine) {
    auto tokens = tokenize(cmdLine);
    if (tokens.empty()) {
        // Empty command - repeat step
        cmdStep(1);
        return;
    }
    
    std::string_view cmd = tokens[0];
    
    if (cmd == "h" || cmd == "help") {
        cmdHelp();
    }
    else if (cmd == "s" || cmd == "step") {
        int count = 1;
        if (tokens.size() > 1) {
            int tmp = 0;
            auto r = std::from_chars(tokens[1].data(), tokens[1].data() + tokens[1].size(), tmp);
            if (r.ec == std::errc()) count = tmp;
        }
        cmdStep(count);
    }
    else if (cmd == "c" || cmd == "continue") {
        cmdContinue();
    }
    else if (cmd == "r" || cmd == "regs") {
        cmdRegisters();
    }
    else if (cmd == "d" || cmd == "dis") {
        u16 addr = bus_.cpu.getPC();
        int count = 10;
        if (tokens.size() > 1) {
            if (auto o = parseAddress(tokens[1]); o) addr = *o;
        }
        if (tokens.size() > 2) {
            int tmp = 0;
            auto r = std::from_chars(tokens[2].data(), tokens[2].data() + tokens[2].size(), tmp);
            if (r.ec == std::errc()) count = tmp;
        }
        cmdDisassemble(addr, count);
    }
    else if (cmd == "mr" || cmd == "memread") {
        if (tokens.size() < 2) {
            printError("Usage: memread <addr> [count]");
        } else if (auto o = parseAddress(tokens[1]); o) {
            int count = 64;
            if (tokens.size() > 2) {
                int tmp = 0;
                auto r = std::from_chars(tokens[2].data(), tokens[2].data() + tokens[2].size(), tmp);
                if (r.ec == std::errc()) count = tmp;
            }
            cmdMemory(*o, count);
        } else {
            printError("Invalid address");
        }
    }
    else if (cmd == "mw" || cmd == "memwrite") {
        if (tokens.size() < 3) {
            printError("Usage: memwrite <addr> <value>");
        } else if (auto o = parseAddress(tokens[1]); o) {
            u8 val;
            if (parseValue8(tokens[2], val)) {
                cmdWrite(*o, val);
            } else {
                printError("Invalid value");
            }
        } else {
            printError("Invalid address");
        }
    }
    else if (cmd == "b" || cmd == "break") {
        if (tokens.size() < 2) {
            printError("Usage: break <addr>");
        } else if (auto o = parseAddress(tokens[1]); o) {
            cmdBreakpoint(*o);
        } else {
            printError("Invalid address");
        }
    }
    else if (cmd == "del") {
        if (tokens.size() < 2) {
            printError("Usage: del <addr>");
        } else if (auto o = parseAddress(tokens[1]); o) {
            cmdDeleteBreakpoint(*o);
        } else {
            printError("Invalid address");
        }
    }
    else if (cmd == "bl") {
        cmdListBreakpoints();
    }
    else if (cmd == "st" || cmd == "stack") {
        cmdStack();
    }
    else if (cmd == "reset") {
        cmdReset();
    }
    else if (cmd == "regset") {
        if (tokens.size() < 3) {
            printError("Usage: regset <reg> <value>");
            print("Registers: A, X, Y, SP, PC, P");
        } else {
            cmdSetCpu(tokens[1], tokens[2]);
        }
    }
    else if (cmd == "ppu") {
        if (tokens.size() == 1) {
            cmdPpu();
        } else if (tokens.size() >= 3) {
            cmdSetPpu(tokens[1], tokens[2]);
        } else {
            print("Usage: ppu             - Show PPU registers");
            print("       ppu <reg> <val> - Set PPU register");
        }
    }
    else if (cmd == "apu") {
        cmdApu();
    }
    else if (cmd == "io") {
        cmdIo();
    }
    else if (cmd == "clear" || cmd == "cls") {
        cmdClear();
    }
    else {
        printError("Unknown command: " + std::string(cmd) + ". Type 'help' for commands.");
    }
}

void GuiConsole::cmdHelp() {
    printInfo("=== VNES Debugger Commands ===");
    print("");
    printInfo("Execution:");
    print("  s, step [n]        - Execute n instructions (default 1)");
    print("  c, continue        - Run until breakpoint");
    print("  reset              - Reset the CPU");
    print("");
    printInfo("Inspection:");
    print("  r, regs            - Show CPU registers");
    print("  d, dis [addr] [n]  - Disassemble n instructions at addr");
    print("  mr <addr> [n]      - Read n bytes from memory at addr");
    print("  st, stack          - Show stack contents");
    print("");
    printInfo("Breakpoints:");
    print("  b, break <addr>    - Set breakpoint at addr");
    print("  del <addr>         - Delete breakpoint at addr");
    print("  bl                 - List all breakpoints");
    print("");
    printInfo("Modification:");
    print("  mw <addr> <val>    - Write byte to memory");
    print("  regset <reg> <val> - Set CPU register (A,X,Y,SP,PC,P)");
    print("");
    printInfo("Hardware:");
    print("  ppu                - Show PPU registers");
    print("  ppu <reg> <val>    - Set PPU register");
    print("  apu                - Show APU channel status");
    print("  io                 - Show I/O status");
    print("");
    printInfo("Other:");
    print("  clear, cls         - Clear console");
    print("  h, help            - Show this help");
    print("");
    print("Addresses can be hex (0x1234 or $1234) or decimal.");
}

void GuiConsole::step(int count) {
    cmdStep(count);
}

void GuiConsole::stepFrame() {
    // bus_ is always valid (reference)
    saveRegisters();
    while (!bus_.ppu.isFrameComplete()) {
        bus_.clock();
    }
    bus_.ppu.clearFrameComplete();
    printInfo("Frame complete");
    printRegisters();
}

void GuiConsole::cmdStep(int count) {
    
    
    for (int i = 0; i < count; i++) {
        saveRegisters();
        
        bus_.enableAccessLog(true);
        bus_.clearAccessLog();
        
        u64 startCycles = bus_.cpu.getCycles();
        do {
            bus_.clock();
        } while (bus_.cpu.getCycles() == startCycles);
        
        bus_.enableAccessLog(false);
        
        // Show disassembly
        int len;
        std::string instr = disassembleInstruction(prevPc_, len);
        printHighlight(instr);
        
        // Show memory accesses
        const auto& log = bus_.getAccessLog();
        std::ostringstream accesses;
        bool hasAccesses = false;
        for (const auto& access : log) {
            if (access.type == MemAccess::READ && access.addr >= 0x8000 &&
                access.addr >= prevPc_ && access.addr < prevPc_ + len) {
                continue;
            }
            if (hasAccesses) accesses << ", ";
            accesses << (access.type == MemAccess::READ ? "R " : "W ");
            accesses << access.region << "=$" << hexByte(access.value);
            hasAccesses = true;
        }
        if (hasAccesses) {
            printColored("  " + accesses.str(), colorBlue);
        }
        
        printRegisters();
        
        // Check breakpoints
        if (breakpoints_.count(bus_.cpu.getPC()) && i < count - 1) {
            printError("Breakpoint hit at $" + hexWord(bus_.cpu.getPC()));
            break;
        }
    }
}

void GuiConsole::cmdContinue() {
    
    running_ = true;
    printInfo("Running... (use 'break' to set breakpoints)");
}

void GuiConsole::cmdRegisters() {
    
    print("");
    printRegisters();
    print("Cycles: " + std::to_string(bus_.cpu.getCycles()));
}

void GuiConsole::cmdMemory(u16 addr, int count) {
    
    
    std::ostringstream ss;
    ss << "Memory [" << getMemoryRegion(addr) << "] $" << hexWord(addr) << ":";
    printInfo(ss.str());
    
    for (int i = 0; i < count; i += 16) {
        std::ostringstream line;
        line << hexWord(addr + i) << ": ";
        
        for (int j = 0; j < 16 && (i + j) < count; j++) {
            line << hexByte(bus_.read(addr + i + j)) << " ";
        }
        
        line << " |";
        for (int j = 0; j < 16 && (i + j) < count; j++) {
            u8 c = bus_.read(addr + i + j);
            line << static_cast<char>((c >= 32 && c < 127) ? c : '.');
        }
        line << "|";
        
        print(line.str());
    }
}

void GuiConsole::cmdDisassemble(u16 addr, int count) {
    
    
    u16 pc = bus_.cpu.getPC();
    for (int i = 0; i < count; i++) {
        int len;
        std::string line = disassembleInstruction(addr, len);
        
        if (addr == pc) {
            printHighlight("> " + line);
        } else {
            print("  " + line);
        }
        
        addr += len;
    }
}

void GuiConsole::cmdBreakpoint(u16 addr) {
    breakpoints_.insert(addr);
    printInfo("Breakpoint set at $" + hexWord(addr));
}

void GuiConsole::cmdDeleteBreakpoint(u16 addr) {
    if (breakpoints_.erase(addr)) {
        printInfo("Breakpoint deleted at $" + hexWord(addr));
    } else {
        printError("No breakpoint at $" + hexWord(addr));
    }
}

void GuiConsole::cmdListBreakpoints() {
    if (breakpoints_.empty()) {
        print("No breakpoints set");
    } else {
        printInfo("Breakpoints:");
        for (u16 addr : breakpoints_) {
            print("  $" + hexWord(addr));
        }
    }
}

void GuiConsole::cmdStack() {
    
    
    u8 sp = bus_.cpu.getSP();
    printInfo("Stack (SP=$" + hexByte(sp) + "):");
    
    for (int i = 0xFF; i > sp; i--) {
        u8 val = bus_.read(0x0100 + i);
        print("  $01" + hexByte(i) + ": " + hexByte(val));
    }
}

void GuiConsole::cmdReset() {
    
    
    bus_.reset();
    saveRegisters();
    printInfo("CPU Reset");
    printRegisters();
}

void GuiConsole::cmdWrite(u16 addr, u8 value) {
    
    
    bus_.write(addr, value);
    printInfo("Wrote $" + hexByte(value) + " to $" + hexWord(addr) + " [" + std::string(getMemoryRegion(addr)) + "]");
}

void GuiConsole::cmdSetCpu(std::string_view reg, std::string_view value) {
    
    
    std::string r(reg);
    std::transform(r.begin(), r.end(), r.begin(), ::toupper);
    
    if (r == "A") {
        u8 val;
        if (parseValue8(value, val)) {
            bus_.cpu.setA(val);
            printInfo("A = $" + hexByte(val));
        } else {
            printError("Invalid value");
        }
    }
    else if (r == "X") {
        u8 val;
        if (parseValue8(value, val)) {
            bus_.cpu.setX(val);
            printInfo("X = $" + hexByte(val));
        } else {
            printError("Invalid value");
        }
    }
    else if (r == "Y") {
        u8 val;
        if (parseValue8(value, val)) {
            bus_.cpu.setY(val);
            printInfo("Y = $" + hexByte(val));
        } else {
            printError("Invalid value");
        }
    }
    else if (r == "SP" || r == "S") {
        u8 val;
        if (parseValue8(value, val)) {
            bus_.cpu.setSP(val);
            printInfo("SP = $" + hexByte(val));
        } else {
            printError("Invalid value");
        }
    }
    else if (r == "PC") {
        u16 val;
        if (parseValue16(value, val)) {
            bus_.cpu.setPC(val);
            printInfo("PC = $" + hexWord(val));
        } else {
            printError("Invalid value");
        }
    }
    else if (r == "P" || r == "FLAGS" || r == "STATUS") {
        u8 val;
        if (parseValue8(value, val)) {
            bus_.cpu.setStatus(val);
            printInfo("P = $" + hexByte(val) + " [" + vnes::util::formatFlags(val) + "]");
        } else {
            printError("Invalid value");
        }
    }
    else {
        printError("Unknown register: " + std::string(reg));
        print("Valid registers: A, X, Y, SP, PC, P");
    }
}

void GuiConsole::cmdPpu() {
    
    
    printInfo("=== PPU Registers ===");
    
    u8 ctrl = bus_.ppu.getCtrl();
    std::ostringstream ss;
    ss << "CTRL ($2000): $" << hexByte(ctrl);
    ss << "  NMI:" << ((ctrl & 0x80) ? "on" : "off");
    ss << "  Sprite:" << ((ctrl & 0x20) ? "8x16" : "8x8");
    ss << "  BG:" << ((ctrl & 0x10) ? "$1000" : "$0000");
    print(ss.str());
    
    u8 mask = bus_.ppu.getMask();
    ss.str(""); ss.clear();
    ss << "MASK ($2001): $" << hexByte(mask);
    ss << "  BG:" << ((mask & 0x08) ? "on" : "off");
    ss << "  SPR:" << ((mask & 0x10) ? "on" : "off");
    print(ss.str());
    
    u8 status = bus_.ppu.getStatus();
    ss.str(""); ss.clear();
    ss << "STATUS ($2002): $" << hexByte(status);
    ss << "  VBlank:" << ((status & 0x80) ? "yes" : "no");
    ss << "  Spr0:" << ((status & 0x40) ? "hit" : "no");
    print(ss.str());
    
    print("");
    printInfo("PPU Internal:");
    print("  V (VRAM): $" + hexWord(bus_.ppu.getVramAddr()));
    print("  T (Temp): $" + hexWord(bus_.ppu.getTempAddr()));
    print("  Fine X: " + std::to_string(bus_.ppu.getFineX()));
    print("  Scanline: " + std::to_string(bus_.ppu.getScanline()));
    print("  Cycle: " + std::to_string(bus_.ppu.getCycle()));
}

void GuiConsole::cmdSetPpu(std::string_view reg, std::string_view value) {
    
    
    std::string r(reg);
    std::transform(r.begin(), r.end(), r.begin(), ::toupper);
    
    if (r == "CTRL" || r == "$2000" || r == "2000") {
        u8 val;
        if (parseValue8(value, val)) {
            bus_.ppu.setCtrl(val);
            printInfo("PPUCTRL = $" + hexByte(val));
        } else {
            printError("Invalid value");
        }
    }
    else if (r == "MASK" || r == "$2001" || r == "2001") {
        u8 val;
        if (parseValue8(value, val)) {
            bus_.ppu.setMask(val);
            printInfo("PPUMASK = $" + hexByte(val));
        } else {
            printError("Invalid value");
        }
    }
    else if (r == "V" || r == "VRAM") {
        u16 val;
        if (parseValue16(value, val)) {
            bus_.ppu.setVramAddr(val & 0x7FFF);
            printInfo("V = $" + hexWord(val & 0x7FFF));
        } else {
            printError("Invalid value");
        }
    }
    else if (r == "T" || r == "TEMP") {
        u16 val;
        if (parseValue16(value, val)) {
            bus_.ppu.setTempAddr(val & 0x7FFF);
            printInfo("T = $" + hexWord(val & 0x7FFF));
        } else {
            printError("Invalid value");
        }
    }
    else {
        printError("Unknown PPU register: " + std::string(reg));
        print("Valid: CTRL, MASK, V, T");
    }
}

void GuiConsole::cmdApu() {
    
    
    printInfo("=== APU Channel Status ===");
    
    auto p1 = bus_.apu.getPulse1Status();
    std::ostringstream ss;
    ss << "Pulse 1: " << (p1.enabled ? "ON" : "OFF");
    ss << "  Vol:" << (int)p1.volume << "  Per:" << p1.period << "  Len:" << (int)p1.length;
    print(ss.str());
    
    auto p2 = bus_.apu.getPulse2Status();
    ss.str(""); ss.clear();
    ss << "Pulse 2: " << (p2.enabled ? "ON" : "OFF");
    ss << "  Vol:" << (int)p2.volume << "  Per:" << p2.period << "  Len:" << (int)p2.length;
    print(ss.str());
    
    auto tri = bus_.apu.getTriangleStatus();
    ss.str(""); ss.clear();
    ss << "Triangle: " << (tri.enabled ? "ON" : "OFF");
    ss << "  Per:" << tri.period << "  Len:" << (int)tri.length;
    print(ss.str());
    
    auto noi = bus_.apu.getNoiseStatus();
    ss.str(""); ss.clear();
    ss << "Noise: " << (noi.enabled ? "ON" : "OFF");
    ss << "  Vol:" << (int)noi.volume << "  Per:" << noi.period << "  Len:" << (int)noi.length;
    print(ss.str());
    
    auto dmc = bus_.apu.getDMCStatus();
    ss.str(""); ss.clear();
    ss << "DMC: " << (dmc.enabled ? "ON" : "OFF");
    ss << "  Out:" << (int)dmc.volume << "  Rate:" << dmc.period;
    print(ss.str());
    
    print("");
    ss.str(""); ss.clear();
    ss << "Frame Counter: " << (bus_.apu.getFrameCounterMode() ? "5-step" : "4-step");
    ss << "  IRQ Inhibit: " << (bus_.apu.getIrqInhibit() ? "yes" : "no");
    print(ss.str());
}

void GuiConsole::cmdIo() {
    printInfo("=== I/O Status ===");
    print("");
    print("Controller 1 ($4016): Write to strobe");
    print("Controller 2 ($4017): Also APU frame counter");
    print("OAM DMA ($4014): Write page number to trigger DMA");
    print("");
    print("Use 'mw $addr $val' to write to I/O registers:");
    print("  $4000-$4013: APU registers");
    print("  $4014: OAM DMA");
    print("  $4015: APU status");
    print("  $4016: Controller strobe");
    print("  $4017: Frame counter");
}

void GuiConsole::cmdClear() {
    outputLines_.clear();
}

std::string GuiConsole::disassembleInstruction(u16 addr, int& length) {
    
    
    u8 opcode = bus_.read(addr);
    const char* name = opcodeNames[opcode];
    AddrMode mode = addrModes[opcode];
    
    u8 lo = 0, hi = 0;
    length = 1;
    
    std::ostringstream line;
    line << hexWord(addr) << ": " << hexByte(opcode) << " ";
    
    switch (mode) {
        case IMP:
        case ACC:
            line << "      ";
            break;
        case IMM:
        case ZP:
        case ZPX:
        case ZPY:
        case IZX:
        case IZY:
        case REL:
            lo = bus_.read(addr + 1);
            line << hexByte(lo) << "    ";
            length = 2;
            break;
        case ABS:
        case ABX:
        case ABY:
        case IND:
            lo = bus_.read(addr + 1);
            hi = bus_.read(addr + 2);
            line << hexByte(lo) << " " << hexByte(hi) << " ";
            length = 3;
            break;
    }
    
    line << " " << name << " ";
    
    u16 absAddr = (hi << 8) | lo;
    std::string operand;
    
    switch (mode) {
        case IMP: break;
        case ACC: operand = "A"; break;
        case IMM: operand = "#$" + hexByte(lo); break;
        case ZP:  operand = "$" + hexByte(lo); break;
        case ZPX: operand = "$" + hexByte(lo) + ",X"; break;
        case ZPY: operand = "$" + hexByte(lo) + ",Y"; break;
        case ABS: operand = "$" + hexWord(absAddr); break;
        case ABX: operand = "$" + hexWord(absAddr) + ",X"; break;
        case ABY: operand = "$" + hexWord(absAddr) + ",Y"; break;
        case IND: operand = "($" + hexWord(absAddr) + ")"; break;
        case IZX: operand = "($" + hexByte(lo) + ",X)"; break;
        case IZY: operand = "($" + hexByte(lo) + "),Y"; break;
        case REL: {
            s8 offset = static_cast<s8>(lo);
            u16 target = addr + 2 + offset;
            operand = "$" + hexWord(target);
            break;
        }
    }
    
    line << operand;
    
    return line.str();
}

std::string GuiConsole::generatePseudoC(u8 opcode, int mode, u8 lo, u8 hi, u16 addr) {
    // Simplified version - can expand later if needed
    return "";
}
