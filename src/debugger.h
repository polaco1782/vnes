#ifndef DEBUGGER_H
#define DEBUGGER_H

#include "types.h"
#include <string>
#include <vector>
#include <set>

// Forward declarations
class Bus;
class CPU;

class Debugger {
public:
    Debugger();

    void connect(Bus* bus);
    void run();

private:
    // Commands
    void cmdHelp();
    void cmdStep(int count = 1);
    void cmdContinue();
    void cmdRegisters();
    void cmdMemory(u16 addr, int count = 16);
    void cmdDisassemble(u16 addr, int count = 10);
    void cmdBreakpoint(u16 addr);
    void cmdDeleteBreakpoint(u16 addr);
    void cmdListBreakpoints();
    void cmdStack();
    void cmdReset();
    
    // Register manipulation commands
    void cmdSetCpu(const std::string& reg, const std::string& value);
    void cmdFlag(const std::string& flag, const std::string& value);
    void cmdPpu();
    void cmdSetPpu(const std::string& reg, const std::string& value);
    void cmdApu();
    void cmdIo();
    void cmdWrite(u16 addr, u8 value);

    // Helpers
    std::string disassembleInstruction(u16 addr, int& length);
    std::string formatFlags(u8 status);
    void printRegisters();
    void printRegisterChanges();
    void saveRegisters();
    bool parseAddress(const std::string& str, u16& addr);
    bool parseValue8(const std::string& str, u8& val);
    bool parseValue16(const std::string& str, u16& val);
    std::vector<std::string> tokenize(const std::string& line);
    std::string readLineWithHistory();

    // State
    Bus* bus;
    
    // Previous register state for highlighting changes
    u16 prev_pc;
    u8 prev_sp;
    u8 prev_a;
    u8 prev_x;
    u8 prev_y;
    u8 prev_status;

    // Breakpoints
    std::set<u16> breakpoints;
    
    // Running state
    bool running;
    bool quit;
    
    // Command history
    std::vector<std::string> command_history;
    int history_index;
};

#endif // DEBUGGER_H
