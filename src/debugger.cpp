#include "debugger.h"
#include "bus.h"
#include "cpu.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <termios.h>
#include <unistd.h>
#include <cstring>

// ANSI color codes
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_BOLD    "\033[1m"

// Hex formatting helpers
static std::string hexByte(u8 val) {
    std::ostringstream ss;
    ss << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)val;
    return ss.str();
}

static std::string hexWord(u16 val) {
    std::ostringstream ss;
    ss << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << val;
    return ss.str();
}

// Get memory region name for an address
static std::string getMemoryRegion(u16 addr) {
    if (addr < 0x2000) return "RAM";
    if (addr < 0x4000) return "PPU";
    if (addr < 0x4018) return "APU/IO";
    if (addr < 0x6000) return "EXP";
    if (addr < 0x8000) return "SRAM";
    return "ROM";
}

// Map hardware addresses to symbolic names
static std::string getHardwareSymbol(u16 addr) {
    switch (addr) {
        // PPU Registers
        case 0x2000: return "PPU_CTRL";
        case 0x2001: return "PPU_MASK";
        case 0x2002: return "PPU_STATUS";
        case 0x2003: return "OAM_ADDR";
        case 0x2004: return "OAM_DATA";
        case 0x2005: return "PPU_SCROLL";
        case 0x2006: return "PPU_ADDR";
        case 0x2007: return "PPU_DATA";
        
        // APU Pulse 1
        case 0x4000: return "APU_PULSE1_CTRL";
        case 0x4001: return "APU_PULSE1_SWEEP";
        case 0x4002: return "APU_PULSE1_TIMER_LO";
        case 0x4003: return "APU_PULSE1_TIMER_HI";
        
        // APU Pulse 2
        case 0x4004: return "APU_PULSE2_CTRL";
        case 0x4005: return "APU_PULSE2_SWEEP";
        case 0x4006: return "APU_PULSE2_TIMER_LO";
        case 0x4007: return "APU_PULSE2_TIMER_HI";
        
        // APU Triangle
        case 0x4008: return "APU_TRIANGLE_CTRL";
        case 0x400A: return "APU_TRIANGLE_TIMER_LO";
        case 0x400B: return "APU_TRIANGLE_TIMER_HI";
        
        // APU Noise
        case 0x400C: return "APU_NOISE_CTRL";
        case 0x400E: return "APU_NOISE_PERIOD";
        case 0x400F: return "APU_NOISE_LENGTH";
        
        // APU DMC
        case 0x4010: return "APU_DMC_CTRL";
        case 0x4011: return "APU_DMC_OUTPUT";
        case 0x4012: return "APU_DMC_ADDR";
        case 0x4013: return "APU_DMC_LENGTH";
        
        // Other I/O
        case 0x4014: return "OAM_DMA";
        case 0x4015: return "APU_STATUS";
        case 0x4016: return "JOY1";
        case 0x4017: return "JOY2_FRAME_COUNTER";
        
        default:
            // Return empty string for non-hardware addresses
            return "";
    }
}

// Opcode names for disassembly
static const char* opcode_names[256] = {
    "BRK","ORA","???","???","???","ORA","ASL","???","PHP","ORA","ASL","???","???","ORA","ASL","???",
    "BPL","ORA","???","???","???","ORA","ASL","???","CLC","ORA","???","???","???","ORA","ASL","???",
    "JSR","AND","???","???","BIT","AND","ROL","???","PLP","AND","ROL","???","BIT","AND","ROL","???",
    "BMI","AND","???","???","???","AND","ROL","???","SEC","AND","???","???","???","AND","ROL","???",
    "RTI","EOR","???","???","???","EOR","LSR","???","PHA","EOR","LSR","???","JMP","EOR","LSR","???",
    "BVC","EOR","???","???","???","EOR","LSR","???","CLI","EOR","???","???","???","EOR","LSR","???",
    "RTS","ADC","???","???","???","ADC","ROR","???","PLA","ADC","ROR","???","JMP","ADC","ROR","???",
    "BVS","ADC","???","???","???","ADC","ROR","???","SEI","ADC","???","???","???","ADC","ROR","???",
    "???","STA","???","???","STY","STA","STX","???","DEY","???","TXA","???","STY","STA","STX","???",
    "BCC","STA","???","???","STY","STA","STX","???","TYA","STA","TXS","???","???","STA","???","???",
    "LDY","LDA","LDX","???","LDY","LDA","LDX","???","TAY","LDA","TAX","???","LDY","LDA","LDX","???",
    "BCS","LDA","???","???","LDY","LDA","LDX","???","CLV","LDA","TSX","???","LDY","LDA","LDX","???",
    "CPY","CMP","???","???","CPY","CMP","DEC","???","INY","CMP","DEX","???","CPY","CMP","DEC","???",
    "BNE","CMP","???","???","???","CMP","DEC","???","CLD","CMP","???","???","???","CMP","DEC","???",
    "CPX","SBC","???","???","CPX","SBC","INC","???","INX","SBC","NOP","???","CPX","SBC","INC","???",
    "BEQ","SBC","???","???","???","SBC","INC","???","SED","SBC","???","???","???","SBC","INC","???"
};

// Addressing mode types
enum AddrMode {
    IMP, ACC, IMM, ZP, ZPX, ZPY, ABS, ABX, ABY, IND, IZX, IZY, REL
};

static const AddrMode opcode_modes[256] = {
    IMP,IZX,IMP,IMP,IMP,ZP ,ZP ,IMP,IMP,IMM,ACC,IMP,IMP,ABS,ABS,IMP,
    REL,IZY,IMP,IMP,IMP,ZPX,ZPX,IMP,IMP,ABY,IMP,IMP,IMP,ABX,ABX,IMP,
    ABS,IZX,IMP,IMP,ZP ,ZP ,ZP ,IMP,IMP,IMM,ACC,IMP,ABS,ABS,ABS,IMP,
    REL,IZY,IMP,IMP,IMP,ZPX,ZPX,IMP,IMP,ABY,IMP,IMP,IMP,ABX,ABX,IMP,
    IMP,IZX,IMP,IMP,IMP,ZP ,ZP ,IMP,IMP,IMM,ACC,IMP,ABS,ABS,ABS,IMP,
    REL,IZY,IMP,IMP,IMP,ZPX,ZPX,IMP,IMP,ABY,IMP,IMP,IMP,ABX,ABX,IMP,
    IMP,IZX,IMP,IMP,IMP,ZP ,ZP ,IMP,IMP,IMM,ACC,IMP,IND,ABS,ABS,IMP,
    REL,IZY,IMP,IMP,IMP,ZPX,ZPX,IMP,IMP,ABY,IMP,IMP,IMP,ABX,ABX,IMP,
    IMP,IZX,IMP,IMP,ZP ,ZP ,ZP ,IMP,IMP,IMP,IMP,IMP,ABS,ABS,ABS,IMP,
    REL,IZY,IMP,IMP,ZPX,ZPX,ZPY,IMP,IMP,ABY,IMP,IMP,IMP,ABX,IMP,IMP,
    IMM,IZX,IMM,IMP,ZP ,ZP ,ZP ,IMP,IMP,IMM,IMP,IMP,ABS,ABS,ABS,IMP,
    REL,IZY,IMP,IMP,ZPX,ZPX,ZPY,IMP,IMP,ABY,IMP,IMP,ABX,ABX,ABY,IMP,
    IMM,IZX,IMP,IMP,ZP ,ZP ,ZP ,IMP,IMP,IMM,IMP,IMP,ABS,ABS,ABS,IMP,
    REL,IZY,IMP,IMP,IMP,ZPX,ZPX,IMP,IMP,ABY,IMP,IMP,IMP,ABX,ABX,IMP,
    IMM,IZX,IMP,IMP,ZP ,ZP ,ZP ,IMP,IMP,IMM,IMP,IMP,ABS,ABS,ABS,IMP,
    REL,IZY,IMP,IMP,IMP,ZPX,ZPX,IMP,IMP,ABY,IMP,IMP,IMP,ABX,ABX,IMP
};

Debugger::Debugger()
    : bus(nullptr)
    , prev_pc(0), prev_sp(0), prev_a(0), prev_x(0), prev_y(0), prev_status(0)
    , running(false), quit(false)
    , history_index(-1)
{
}

void Debugger::connect(Bus* b)
{
    bus = b;
}

std::string Debugger::formatFlags(u8 status)
{
    std::string flags;
    flags += (status & FLAG_N) ? 'N' : 'n';
    flags += (status & FLAG_V) ? 'V' : 'v';
    flags += (status & FLAG_U) ? 'U' : 'u';
    flags += (status & FLAG_B) ? 'B' : 'b';
    flags += (status & FLAG_D) ? 'D' : 'd';
    flags += (status & FLAG_I) ? 'I' : 'i';
    flags += (status & FLAG_Z) ? 'Z' : 'z';
    flags += (status & FLAG_C) ? 'C' : 'c';
    return flags;
}

void Debugger::saveRegisters()
{
    prev_pc = bus->cpu.getPC();
    prev_sp = bus->cpu.getSP();
    prev_a = bus->cpu.getA();
    prev_x = bus->cpu.getX();
    prev_y = bus->cpu.getY();
    prev_status = bus->cpu.getStatus();
}

void Debugger::printRegisters()
{
    u16 pc = bus->cpu.getPC();
    u8 sp = bus->cpu.getSP();
    u8 a = bus->cpu.getA();
    u8 x = bus->cpu.getX();
    u8 y = bus->cpu.getY();
    u8 status = bus->cpu.getStatus();

    std::cout << COLOR_CYAN << "PC:" << COLOR_RESET;
    if (pc != prev_pc) std::cout << COLOR_YELLOW;
    std::cout << std::hex << std::setfill('0') << std::setw(4) << pc;
    std::cout << COLOR_RESET << "  ";

    std::cout << COLOR_CYAN << "A:" << COLOR_RESET;
    if (a != prev_a) std::cout << COLOR_YELLOW;
    std::cout << std::setw(2) << (int)a;
    std::cout << COLOR_RESET << "  ";

    std::cout << COLOR_CYAN << "X:" << COLOR_RESET;
    if (x != prev_x) std::cout << COLOR_YELLOW;
    std::cout << std::setw(2) << (int)x;
    std::cout << COLOR_RESET << "  ";

    std::cout << COLOR_CYAN << "Y:" << COLOR_RESET;
    if (y != prev_y) std::cout << COLOR_YELLOW;
    std::cout << std::setw(2) << (int)y;
    std::cout << COLOR_RESET << "  ";

    std::cout << COLOR_CYAN << "SP:" << COLOR_RESET;
    if (sp != prev_sp) std::cout << COLOR_YELLOW;
    std::cout << std::setw(2) << (int)sp;
    std::cout << COLOR_RESET << "  ";

    std::cout << COLOR_CYAN << "P:" << COLOR_RESET;
    if (status != prev_status) std::cout << COLOR_YELLOW;
    std::cout << formatFlags(status) << " [" << std::setw(2) << (int)status << "]";
    std::cout << COLOR_RESET << std::dec << std::endl;
}

// Helper template to format operand based on addressing mode
template<typename Func>
std::string formatOperand(AddrMode mode, u8 lo, u16 abs_addr, Func formatter) {
    std::string symbol;
    
    switch (mode) {
        case IMM:
            return formatter("0x" + hexByte(lo));
        
        case ZP:
            return formatter("[0x" + hexByte(lo) + "]");
        
        case ZPX:
            return formatter("[0x" + hexByte(lo) + " + X]");
        
        case ZPY:
            return formatter("[0x" + hexByte(lo) + " + Y]");
        
        case ABS:
            symbol = getHardwareSymbol(abs_addr);
            if (!symbol.empty()) {
                return formatter("[" + symbol + "]");
            }
            return formatter("[0x" + hexWord(abs_addr) + "]");
        
        case ABX:
            symbol = getHardwareSymbol(abs_addr);
            if (!symbol.empty()) {
                return formatter("[" + symbol + " + X]");
            }
            return formatter("[0x" + hexWord(abs_addr) + " + X]");
        
        case ABY:
            symbol = getHardwareSymbol(abs_addr);
            if (!symbol.empty()) {
                return formatter("[" + symbol + " + Y]");
            }
            return formatter("[0x" + hexWord(abs_addr) + " + Y]");
        
        case IZX:
            return formatter("[[0x" + hexByte(lo) + " + X]]");
        
        case IZY:
            return formatter("[[0x" + hexByte(lo) + "] + Y]");
        
        default:
            return "";
    }
}

std::string Debugger::generatePseudoC(u8 opcode, int mode_int, u8 lo, u8 hi, u16 addr)
{
    std::ostringstream ss;
    AddrMode mode = static_cast<AddrMode>(mode_int);
    u16 abs_addr = (hi << 8) | lo;
    
    // Helper to format simple instructions with alignment and description
    auto simpleInstr = [](const std::string& code, const std::string& desc) {
        std::ostringstream result;
        result << std::left << std::setw(30) << std::setfill(' ') << code;
        result << " -- " << desc;
        return result.str();
    };
    
    // Helper to get addressing mode description
    auto getAddrModeDesc = [&](const std::string& action) -> std::string {
        switch (mode) {
            case IMM: return action + " immediate";
            case ABS: return action + " " + getMemoryRegion(abs_addr) + " (absolute)";
            case ABX: return action + " " + getMemoryRegion(abs_addr) + " (absolute,X)";
            case ABY: return action + " " + getMemoryRegion(abs_addr) + " (absolute,Y)";
            case ZP:  return action + " RAM (zero page)";
            case ZPX: return action + " RAM (zero page,X)";
            case ZPY: return action + " RAM (zero page,Y)";
            case IZX: return action + " " + getMemoryRegion(abs_addr) + " (indexed indirect)";
            case IZY: return action + " " + getMemoryRegion(abs_addr) + " (indirect indexed)";
            default:  return "";
        }
    };
    
    // Lambda helpers for common patterns
    auto loadInstr = [&](const char* reg) {
        return formatOperand(mode, lo, abs_addr, [&](const std::string& operand) {
            std::string base = std::string(reg) + " = " + operand;
            std::ostringstream result;
            result << std::left << std::setw(30) << std::setfill(' ') << base;
            result << " -- " << (mode == IMM ? "immediate load into " + std::string(reg) : getAddrModeDesc("load from"));
            return result.str();
        });
    };
    
    auto storeInstr = [&](const char* reg) {
        return formatOperand(mode, lo, abs_addr, [&](const std::string& operand) {
            std::string base = operand + " = " + reg;
            std::ostringstream result;
            result << std::left << std::setw(30) << std::setfill(' ') << base;
            result << " -- " << getAddrModeDesc("store to");
            return result.str();
        });
    };
    
    auto aluInstr = [&](const char* op, const std::string& suffix = "") {
        return formatOperand(mode, lo, abs_addr, [&](const std::string& operand) {
            std::string base = std::string("A ") + op + " " + operand + suffix;
            std::ostringstream result;
            result << std::left << std::setw(30) << std::setfill(' ') << base;
            result << " -- " << (mode == IMM ? "immediate load" : getAddrModeDesc("read from"));
            return result.str();
        });
    };
    
    auto cmpInstr = [&](const char* reg) {
        return formatOperand(mode, lo, abs_addr, [&](const std::string& operand) {
            std::string base = std::string("compare(") + reg + ", " + operand + ")";
            std::ostringstream result;
            result << std::left << std::setw(30) << std::setfill(' ') << base;
            result << " -- " << (mode == IMM ? "immediate load and compare with " + std::string(reg) : getAddrModeDesc("read from"));
            return result.str();
        });
    };
    
    auto memModInstr = [&](const char* op, const char* desc = nullptr) {
        if (mode == ACC) return desc ? simpleInstr(std::string("A ") + op, desc) : std::string("A ") + op;
        return formatOperand(mode, lo, abs_addr, [&](const std::string& operand) {
            std::string base = operand + " " + op;
            std::ostringstream result;
            result << std::left << std::setw(30) << std::setfill(' ') << base;
            result << " -- " << (desc ? std::string(desc) + ", " : "modify ");
            if (!desc) result << getMemoryRegion(abs_addr) << " ";
            result << "(";
            switch (mode) {
                case ABS: result << "absolute"; break;
                case ABX: result << "absolute,X"; break;
                case ABY: result << "absolute,Y"; break;
                case ZP:  result << "zero page"; break;
                case ZPX: result << "zero page,X"; break;
                case ZPY: result << "zero page,Y"; break;
                default: break;
            }
            result << ")";
            return result.str();
        });
    };
    
    auto branchInstr = [&](const char* cond) {
        s8 offset = (s8)lo;
        u16 target = addr + 2 + offset;
        std::ostringstream base;
        base << "if (" << cond << ") goto 0x" << std::hex << std::setw(4) << std::setfill('0') << target;
        return simpleInstr(base.str(), "branch (relative, offset=" + std::to_string((int)offset) + ")");
    };
    
    // Switch on opcode byte directly instead of string comparison
    switch (opcode) {
        // Load instructions
        case 0xA9: case 0xA5: case 0xB5: case 0xAD: case 0xBD: case 0xB9: case 0xA1: case 0xB1:
            return loadInstr("A");
        case 0xA2: case 0xA6: case 0xB6: case 0xAE: case 0xBE:
            return loadInstr("X");
        case 0xA0: case 0xA4: case 0xB4: case 0xAC: case 0xBC:
            return loadInstr("Y");
        
        // Store instructions
        case 0x85: case 0x95: case 0x8D: case 0x9D: case 0x99: case 0x81: case 0x91:
            return storeInstr("A");
        case 0x86: case 0x96: case 0x8E:
            return storeInstr("X");
        case 0x84: case 0x94: case 0x8C:
            return storeInstr("Y");
        
        // ALU operations
        case 0x69: case 0x65: case 0x75: case 0x6D: case 0x7D: case 0x79: case 0x61: case 0x71:
            return aluInstr("+=", " + C");
        case 0xE9: case 0xE5: case 0xF5: case 0xED: case 0xFD: case 0xF9: case 0xE1: case 0xF1:
            return aluInstr("-=", " - !C");
        case 0x29: case 0x25: case 0x35: case 0x2D: case 0x3D: case 0x39: case 0x21: case 0x31:
            return aluInstr("&=");
        case 0x09: case 0x05: case 0x15: case 0x0D: case 0x1D: case 0x19: case 0x01: case 0x11:
            return aluInstr("|=");
        case 0x49: case 0x45: case 0x55: case 0x4D: case 0x5D: case 0x59: case 0x41: case 0x51:
            return aluInstr("^=");
        
        // Compare instructions
        case 0xC9: case 0xC5: case 0xD5: case 0xCD: case 0xDD: case 0xD9: case 0xC1: case 0xD1:
            return cmpInstr("A");
        case 0xE0: case 0xE4: case 0xEC:
            return cmpInstr("X");
        case 0xC0: case 0xC4: case 0xCC:
            return cmpInstr("Y");
        
        // Branch instructions
        case 0xF0: return branchInstr("Z");
        case 0xD0: return branchInstr("!Z");
        case 0xB0: return branchInstr("C");
        case 0x90: return branchInstr("!C");
        case 0x30: return branchInstr("N");
        case 0x10: return branchInstr("!N");
        case 0x70: return branchInstr("V");
        case 0x50: return branchInstr("!V");
        
        // Jump instructions
        case 0x4C: {
            std::ostringstream base;
            base << "goto 0x" << std::hex << std::setw(4) << std::setfill('0') << abs_addr;
            return simpleInstr(base.str(), "absolute jump");
        }
        case 0x6C: {
            std::ostringstream base;
            base << "goto [0x" << std::hex << std::setw(4) << std::setfill('0') << abs_addr << "]";
            return simpleInstr(base.str(), "indirect jump");
        }
        case 0x20: {
            std::ostringstream base;
            base << "call(0x" << std::hex << std::setw(4) << std::setfill('0') << abs_addr << ")";
            return simpleInstr(base.str(), "jump to subroutine");
        }
        
        // Return instructions
        case 0x60: return simpleInstr("return", "return from subroutine");
        case 0x40: return simpleInstr("return_from_interrupt()", "return from interrupt");
        
        // Stack operations
        case 0x48: return simpleInstr("push(A)", "push accumulator to stack");
        case 0x08: return simpleInstr("push(P)", "push status flags to stack");
        case 0x68: return simpleInstr("A = pop()", "pop accumulator from stack");
        case 0x28: return simpleInstr("P = pop()", "pop status flags from stack");
        
        // Transfer instructions
        case 0xAA: return simpleInstr("X = A", "transfer A to X");
        case 0xA8: return simpleInstr("Y = A", "transfer A to Y");
        case 0x8A: return simpleInstr("A = X", "transfer X to A");
        case 0x98: return simpleInstr("A = Y", "transfer Y to A");
        case 0xBA: return simpleInstr("X = SP", "transfer SP to X");
        case 0x9A: return simpleInstr("SP = X", "transfer X to SP");
        
        // Increment/Decrement memory
        case 0xE6: case 0xF6: case 0xEE: case 0xFE:
            return memModInstr("++", "increment memory");
        case 0xC6: case 0xD6: case 0xCE: case 0xDE:
            return memModInstr("--", "decrement memory");
        
        // Increment/Decrement registers
        case 0xE8: return simpleInstr("X++", "increment X register");
        case 0xC8: return simpleInstr("Y++", "increment Y register");
        case 0xCA: return simpleInstr("X--", "decrement X register");
        case 0x88: return simpleInstr("Y--", "decrement Y register");
        
        // Shift and rotate instructions
        case 0x0A: case 0x06: case 0x16: case 0x0E: case 0x1E:
            return memModInstr("<<= 1", "arithmetic shift left");
        case 0x4A: case 0x46: case 0x56: case 0x4E: case 0x5E:
            return memModInstr(">>= 1", "logical shift right");
        case 0x2A: return simpleInstr("A = (A << 1) | C", "rotate left through carry");
        case 0x26: case 0x36: case 0x2E: case 0x3E:
            return memModInstr("= rol(...)", "rotate left through carry");
        case 0x6A: return simpleInstr("A = (A >> 1) | (C << 7)", "rotate right through carry");
        case 0x66: case 0x76: case 0x6E: case 0x7E:
            return memModInstr("= ror(...)", "rotate right through carry");
        
        // Flag operations
        case 0x18: return simpleInstr("C = 0", "clear carry flag");
        case 0x38: return simpleInstr("C = 1", "set carry flag");
        case 0x58: return simpleInstr("I = 0", "clear interrupt disable");
        case 0x78: return simpleInstr("I = 1", "set interrupt disable");
        case 0xB8: return simpleInstr("V = 0", "clear overflow flag");
        case 0xD8: return simpleInstr("D = 0", "clear decimal mode");
        case 0xF8: return simpleInstr("D = 1", "set decimal mode");
        
        // BIT test
        case 0x24: case 0x2C:
            return formatOperand(mode, lo, abs_addr, [&](const std::string& operand) {
                return simpleInstr("test(A, " + operand + ")", "bit test (sets N,V,Z)");
            });
        
        // Special instructions
        case 0xEA: return simpleInstr("/* no-op */", "no operation");
        case 0x00: return simpleInstr("break()", "software interrupt");
        
        default:
            return "(invalid opcode)";
    }
}

std::string Debugger::disassembleInstruction(u16 addr, int& length)
{
    std::ostringstream ss;
    
    u8 opcode = bus->cpuRead(addr);
    const char* name = opcode_names[opcode];
    AddrMode mode = opcode_modes[opcode];

    ss << std::hex << std::uppercase << std::setfill('0');
    ss << std::setw(4) << addr << ": ";

    // Print opcode bytes
    ss << std::setw(2) << (int)opcode << " ";

    u8 lo = 0, hi = 0;
    length = 1;

    switch (mode) {
        case IMP:
        case ACC:
            ss << "      ";
            break;
        case IMM:
        case ZP:
        case ZPX:
        case ZPY:
        case IZX:
        case IZY:
        case REL:
            lo = bus->cpuRead(addr + 1);
            ss << std::setw(2) << (int)lo << "    ";
            length = 2;
            break;
        case ABS:
        case ABX:
        case ABY:
        case IND:
            lo = bus->cpuRead(addr + 1);
            hi = bus->cpuRead(addr + 2);
            ss << std::setw(2) << (int)lo << " " << std::setw(2) << (int)hi << " ";
            length = 3;
            break;
    }

    ss << " " << COLOR_GREEN << name << COLOR_RESET << " ";

    // Format operand
    std::string operand;
    u16 abs_addr = (hi << 8) | lo;
    switch (mode) {
        case IMP:
            break;
        case ACC:
            operand = "A";
            break;
        case IMM:
            operand = "#$" + hexByte(lo);
            break;
        case ZP:
            operand = "$" + hexByte(lo);
            break;
        case ZPX:
            operand = "$" + hexByte(lo) + ",X";
            break;
        case ZPY:
            operand = "$" + hexByte(lo) + ",Y";
            break;
        case ABS:
            operand = "$" + hexWord(abs_addr);
            break;
        case ABX:
            operand = "$" + hexWord(abs_addr) + ",X";
            break;
        case ABY:
            operand = "$" + hexWord(abs_addr) + ",Y";
            break;
        case IND:
            operand = "($" + hexWord(abs_addr) + ")";
            break;
        case IZX:
            operand = "($" + hexByte(lo) + ",X)";
            break;
        case IZY:
            operand = "($" + hexByte(lo) + "),Y";
            break;
        case REL: {
            s8 offset = (s8)lo;
            u16 target = addr + 2 + offset;
            operand = "$" + hexWord(target);
            break;
        }
    }
    
    ss << std::left << std::setfill(' ') << std::setw(15) << operand;
    
    // Add pseudo-C code
    std::string pseudoC = generatePseudoC(opcode, mode, lo, hi, addr);
    if (!pseudoC.empty()) {
        ss << " ; " << pseudoC;
    }

    return ss.str();
}

void Debugger::cmdHelp()
{
    std::cout << COLOR_BOLD << "\nVNES Debugger Commands:" << COLOR_RESET << std::endl;
    std::cout << "  " << COLOR_CYAN << "s, step [n]" << COLOR_RESET << "           - Execute n instructions (default 1)" << std::endl;
    std::cout << "  " << COLOR_CYAN << "c, continue" << COLOR_RESET << "           - Run until breakpoint" << std::endl;
    std::cout << "  " << COLOR_CYAN << "r, regs" << COLOR_RESET << "               - Show CPU registers" << std::endl;
    std::cout << "  " << COLOR_CYAN << "d, dis [addr] [n]" << COLOR_RESET << "    - Disassemble n instructions at addr" << std::endl;
    std::cout << "  " << COLOR_CYAN << "mr, memread <addr> [n]" << COLOR_RESET << " - Read n bytes from memory at addr" << std::endl;
    std::cout << "  " << COLOR_CYAN << "mw, memwrite <addr> <val>" << COLOR_RESET << " - Write byte to memory" << std::endl;
    std::cout << "  " << COLOR_CYAN << "b, break <addr>" << COLOR_RESET << "       - Set breakpoint at addr" << std::endl;
    std::cout << "  " << COLOR_CYAN << "del <addr>" << COLOR_RESET << "            - Delete breakpoint at addr" << std::endl;
    std::cout << "  " << COLOR_CYAN << "bl" << COLOR_RESET << "                    - List all breakpoints" << std::endl;
    std::cout << "  " << COLOR_CYAN << "st, stack" << COLOR_RESET << "             - Show stack contents" << std::endl;
    std::cout << "  " << COLOR_CYAN << "reset" << COLOR_RESET << "                 - Reset the CPU" << std::endl;

    // register manipulation
    std::cout << std::endl;
    std::cout << COLOR_BOLD << "Register Manipulation:" << COLOR_RESET << std::endl;
    std::cout << "  " << COLOR_CYAN << "regset <reg> <val>" << COLOR_RESET << "   - Set CPU register (A,X,Y,SP,PC,P)" << std::endl;
    std::cout << "  " << COLOR_CYAN << "ppu" << COLOR_RESET << "                   - Show PPU registers" << std::endl;
    std::cout << "  " << COLOR_CYAN << "ppu <reg> <val>" << COLOR_RESET << "       - Set PPU register" << std::endl;
    std::cout << "  " << COLOR_CYAN << "apu" << COLOR_RESET << "                   - Show APU channel status" << std::endl;
    std::cout << "  " << COLOR_CYAN << "io" << COLOR_RESET << "                    - Show I/O status" << std::endl;
    std::cout << std::endl;
    std::cout << "  " << COLOR_CYAN << "q, quit" << COLOR_RESET << "               - Exit debugger" << std::endl;
    std::cout << "  " << COLOR_CYAN << "h, help" << COLOR_RESET << "               - Show this help" << std::endl;
    std::cout << std::endl;

    // memory regions
    std::cout << "\nMemory Regions:" << std::endl;
    std::cout << "  $0000-$1FFF : Internal RAM (mirrored)" << std::endl;
    std::cout << "  $2000-$3FFF : PPU Registers (mirrored)" << std::endl;
    std::cout << "  $4000-$4017 : APU and I/O Registers" << std::endl;
    std::cout << "  $4018-$5FFF : APU and I/O Functionality (disabled)" << std::endl;
    std::cout << "  $6000-$7FFF : Cartridge SRAM" << std::endl;
    std::cout << "  $8000-$FFFF : Cartridge PRG ROM" << std::endl;
    std::cout << std::endl;

    std::cout << "\nAddresses/values can be in hex (0x1234 or $1234) or decimal." << std::endl;

}

void Debugger::cmdStep(int count)
{
    for (int i = 0; i < count; i++) {
        saveRegisters();
        
        // Enable access logging and clear previous log
        bus->enableAccessLog(true);
        bus->clearAccessLog();
        
        // Execute one CPU instruction (need to clock until CPU completes)
        u64 start_cycles = bus->cpu.getCycles();
        do {
            bus->clock();
        } while (bus->cpu.getCycles() == start_cycles);

        bus->enableAccessLog(false);

        // Show current state
        int len;
        std::cout << disassembleInstruction(prev_pc, len) << std::endl;
        
        // Show memory accesses (skip instruction fetches from PRG ROM)
        const auto& log = bus->getAccessLog();
        bool first_access = true;
        for (const auto& access : log) {
            // Skip instruction fetch (reads from PRG at PC)
            if (access.type == MemAccess::READ && access.addr >= 0x8000 && 
                access.addr >= prev_pc && access.addr < prev_pc + len) {
                continue;
            }
            
            if (first_access) {
                std::cout << "  ";
                first_access = false;
            } else {
                std::cout << ", ";
            }
            
            if (access.type == MemAccess::READ) {
                std::cout << COLOR_BLUE << "R " << COLOR_RESET;
            } else {
                std::cout << COLOR_RED << "W " << COLOR_RESET;
            }
            
            std::cout << access.region << " = $" 
                      << std::hex << std::setw(2) << std::setfill('0') 
                      << (int)access.value << std::dec;
        }
        if (!first_access) {
            std::cout << std::endl;
        }
        
        printRegisters();

        // Check breakpoints
        if (breakpoints.count(bus->cpu.getPC()) && i < count - 1) {
            std::cout << COLOR_RED << "Breakpoint hit at $" 
                      << std::hex << std::setw(4) << std::setfill('0') 
                      << bus->cpu.getPC() << std::dec << COLOR_RESET << std::endl;
            break;
        }
    }
}

void Debugger::cmdContinue()
{
    running = true;
    int instructions = 0;
    
    while (running) {
        saveRegisters();
        
        u64 start_cycles = bus->cpu.getCycles();
        do {
            bus->clock();
        } while (bus->cpu.getCycles() == start_cycles);

        instructions++;

        // Check breakpoints
        if (breakpoints.count(bus->cpu.getPC())) {
            std::cout << COLOR_RED << "\nBreakpoint hit at $" 
                      << std::hex << std::setw(4) << std::setfill('0') 
                      << bus->cpu.getPC() << std::dec << COLOR_RESET << std::endl;
            running = false;
        }

        // Safety limit
        if (instructions > 1000000) {
            std::cout << "\nExecution limit reached (1M instructions)" << std::endl;
            running = false;
        }
    }

    int len;
    std::cout << disassembleInstruction(bus->cpu.getPC(), len) << std::endl;
    printRegisters();
    std::cout << "Executed " << instructions << " instructions" << std::endl;
}

void Debugger::cmdRegisters()
{
    std::cout << std::endl;
    printRegisters();
    std::cout << COLOR_CYAN << "Cycles: " << COLOR_RESET << bus->cpu.getCycles() << std::endl;
}

void Debugger::cmdMemory(u16 addr, int count)
{
    std::cout << std::hex << std::uppercase << std::setfill('0');

    std::cout << "Reading " << std::dec << count << std::hex << " byte(s) from $" 
            << std::setw(4) << addr << " [" << getMemoryRegion(addr) << "]:" << std::endl;

    for (int i = 0; i < count; i += 16) {
        std::cout << COLOR_CYAN << std::setw(4) << (addr + i) << COLOR_RESET << ": ";
        
        // Hex bytes
        for (int j = 0; j < 16 && (i + j) < count; j++) {
            std::cout << std::setw(2) << (int)bus->cpuRead(addr + i + j) << " ";
        }
        
        // ASCII
        std::cout << " |";
        for (int j = 0; j < 16 && (i + j) < count; j++) {
            u8 c = bus->cpuRead(addr + i + j);
            std::cout << (char)((c >= 32 && c < 127) ? c : '.');
        }
        std::cout << "|" << std::endl;
    }
    
    std::cout << std::dec;
}

void Debugger::cmdDisassemble(u16 addr, int count)
{
    for (int i = 0; i < count; i++) {
        int len;
        std::string line = disassembleInstruction(addr, len);
        
        // Highlight current PC
        if (addr == bus->cpu.getPC()) {
            std::cout << COLOR_BOLD << "> " << line << COLOR_RESET << std::endl;
        } else {
            std::cout << "  " << line << std::endl;
        }
        
        addr += len;
    }
}

void Debugger::cmdBreakpoint(u16 addr)
{
    breakpoints.insert(addr);
    std::cout << "Breakpoint set at $" << std::hex << std::setw(4) 
              << std::setfill('0') << addr << std::dec << std::endl;
}

void Debugger::cmdDeleteBreakpoint(u16 addr)
{
    if (breakpoints.erase(addr)) {
        std::cout << "Breakpoint deleted at $" << std::hex << std::setw(4) 
                  << std::setfill('0') << addr << std::dec << std::endl;
    } else {
        std::cout << "No breakpoint at $" << std::hex << std::setw(4) 
                  << std::setfill('0') << addr << std::dec << std::endl;
    }
}

void Debugger::cmdListBreakpoints()
{
    if (breakpoints.empty()) {
        std::cout << "No breakpoints set" << std::endl;
    } else {
        std::cout << "Breakpoints:" << std::endl;
        for (u16 addr : breakpoints) {
            std::cout << "  $" << std::hex << std::setw(4) 
                      << std::setfill('0') << addr << std::dec << std::endl;
        }
    }
}

void Debugger::cmdStack()
{
    u8 sp = bus->cpu.getSP();
    std::cout << "Stack (SP=$" << std::hex << std::setw(2) 
              << std::setfill('0') << (int)sp << "):" << std::dec << std::endl;
    
    std::cout << std::hex << std::setfill('0');
    for (int i = 0xFF; i > sp; i--) {
        u8 val = bus->cpuRead(0x0100 + i);
        std::cout << "  $01" << std::setw(2) << i << ": " 
                  << std::setw(2) << (int)val << std::endl;
    }
    std::cout << std::dec;
}

void Debugger::cmdReset()
{
    bus->reset();
    saveRegisters();
    std::cout << "CPU Reset" << std::endl;
    printRegisters();
}

bool Debugger::parseAddress(const std::string& str, u16& addr)
{
    try {
        size_t pos = 0;
        if (str.empty()) return false;
        
        std::string s = str;
        int base = 10;
        
        // Handle $ prefix (hex)
        if (s[0] == '$') {
            s = s.substr(1);
            base = 16;
        }
        // Handle 0x prefix (hex)
        else if (s.length() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
            s = s.substr(2);
            base = 16;
        }
        
        unsigned long val = std::stoul(s, &pos, base);
        if (pos != s.length()) return false;
        
        addr = (u16)val;
        return true;
    } catch (...) {
        return false;
    }
}

bool Debugger::parseValue8(const std::string& str, u8& val)
{
    u16 addr;
    if (parseAddress(str, addr)) {
        val = (u8)(addr & 0xFF);
        return true;
    }
    return false;
}

bool Debugger::parseValue16(const std::string& str, u16& val)
{
    return parseAddress(str, val);
}

void Debugger::cmdSetCpu(const std::string& reg, const std::string& value)
{
    std::string r = reg;
    for (auto& c : r) c = toupper(c);
    
    if (r == "A") {
        u8 val;
        if (parseValue8(value, val)) {
            bus->cpu.setA(val);
            std::cout << "A = $" << std::hex << std::setw(2) << std::setfill('0') 
                      << (int)val << std::dec << std::endl;
        } else {
            std::cout << "Invalid value" << std::endl;
        }
    }
    else if (r == "X") {
        u8 val;
        if (parseValue8(value, val)) {
            bus->cpu.setX(val);
            std::cout << "X = $" << std::hex << std::setw(2) << std::setfill('0') 
                      << (int)val << std::dec << std::endl;
        } else {
            std::cout << "Invalid value" << std::endl;
        }
    }
    else if (r == "Y") {
        u8 val;
        if (parseValue8(value, val)) {
            bus->cpu.setY(val);
            std::cout << "Y = $" << std::hex << std::setw(2) << std::setfill('0') 
                      << (int)val << std::dec << std::endl;
        } else {
            std::cout << "Invalid value" << std::endl;
        }
    }
    else if (r == "SP" || r == "S") {
        u8 val;
        if (parseValue8(value, val)) {
            bus->cpu.setSP(val);
            std::cout << "SP = $" << std::hex << std::setw(2) << std::setfill('0') 
                      << (int)val << std::dec << std::endl;
        } else {
            std::cout << "Invalid value" << std::endl;
        }
    }
    else if (r == "PC") {
        u16 val;
        if (parseValue16(value, val)) {
            bus->cpu.setPC(val);
            std::cout << "PC = $" << std::hex << std::setw(4) << std::setfill('0') 
                      << val << std::dec << std::endl;
        } else {
            std::cout << "Invalid value" << std::endl;
        }
    }
    else if (r == "P" || r == "FLAGS" || r == "STATUS") {
        u8 val;
        if (parseValue8(value, val)) {
            bus->cpu.setStatus(val);
            std::cout << "P = $" << std::hex << std::setw(2) << std::setfill('0') 
                      << (int)val << " [" << formatFlags(val) << "]" << std::dec << std::endl;
        } else {
            std::cout << "Invalid value" << std::endl;
        }
    }
    else {
        std::cout << "Unknown register: " << reg << std::endl;
        std::cout << "Valid registers: A, X, Y, SP, PC, P" << std::endl;
    }
}

void Debugger::cmdPpu()
{
    std::cout << std::hex << std::setfill('0');
    
    std::cout << COLOR_BOLD << "\nPPU Registers:" << COLOR_RESET << std::endl;
    
    u8 ctrl = bus->ppu.getCtrl();
    std::cout << "  " << COLOR_CYAN << "CTRL ($2000):" << COLOR_RESET << " $" << std::setw(2) << (int)ctrl;
    std::cout << "  NMI:" << ((ctrl & 0x80) ? "on" : "off");
    std::cout << "  Sprite:" << ((ctrl & 0x20) ? "8x16" : "8x8");
    std::cout << "  BG:" << ((ctrl & 0x10) ? "$1000" : "$0000");
    std::cout << "  SPR:" << ((ctrl & 0x08) ? "$1000" : "$0000");
    std::cout << "  Inc:" << ((ctrl & 0x04) ? "32" : "1");
    std::cout << "  NT:" << (ctrl & 0x03) << std::endl;
    
    u8 mask = bus->ppu.getMask();
    std::cout << "  " << COLOR_CYAN << "MASK ($2001):" << COLOR_RESET << " $" << std::setw(2) << (int)mask;
    std::cout << "  BG:" << ((mask & 0x08) ? "on" : "off");
    std::cout << "  SPR:" << ((mask & 0x10) ? "on" : "off");
    std::cout << "  L8-BG:" << ((mask & 0x02) ? "on" : "off");
    std::cout << "  L8-SPR:" << ((mask & 0x04) ? "on" : "off");
    std::cout << std::endl;
    
    u8 status = bus->ppu.getStatus();
    std::cout << "  " << COLOR_CYAN << "STATUS ($2002):" << COLOR_RESET << " $" << std::setw(2) << (int)status;
    std::cout << "  VBlank:" << ((status & 0x80) ? "yes" : "no");
    std::cout << "  Spr0:" << ((status & 0x40) ? "hit" : "no");
    std::cout << "  Overflow:" << ((status & 0x20) ? "yes" : "no");
    std::cout << std::endl;
    
    std::cout << "  " << COLOR_CYAN << "OAMADDR ($2003):" << COLOR_RESET << " $" << std::setw(2) << (int)bus->ppu.getOamAddr() << std::endl;
    
    std::cout << std::endl;
    std::cout << COLOR_BOLD << "PPU Internal State:" << COLOR_RESET << std::endl;
    std::cout << "  " << COLOR_CYAN << "V (VRAM addr):" << COLOR_RESET << " $" << std::setw(4) << bus->ppu.getVramAddr() << std::endl;
    std::cout << "  " << COLOR_CYAN << "T (Temp addr):" << COLOR_RESET << " $" << std::setw(4) << bus->ppu.getTempAddr() << std::endl;
    std::cout << "  " << COLOR_CYAN << "Fine X:" << COLOR_RESET << " " << std::dec << (int)bus->ppu.getFineX() << std::hex << std::endl;
    std::cout << "  " << COLOR_CYAN << "Write toggle:" << COLOR_RESET << " " << (bus->ppu.getWriteToggle() ? "1" : "0") << std::endl;
    std::cout << "  " << COLOR_CYAN << "Scanline:" << COLOR_RESET << " " << std::dec << bus->ppu.getScanline() << std::endl;
    std::cout << "  " << COLOR_CYAN << "Cycle:" << COLOR_RESET << " " << bus->ppu.getCycle() << std::endl;
    
    std::cout << std::dec;
}

void Debugger::cmdSetPpu(const std::string& reg, const std::string& value)
{
    std::string r = reg;
    for (auto& c : r) c = toupper(c);
    
    if (r == "CTRL" || r == "$2000" || r == "2000") {
        u8 val;
        if (parseValue8(value, val)) {
            bus->ppu.setCtrl(val);
            std::cout << "PPUCTRL = $" << std::hex << std::setw(2) << std::setfill('0') 
                      << (int)val << std::dec << std::endl;
        } else {
            std::cout << "Invalid value" << std::endl;
        }
    }
    else if (r == "MASK" || r == "$2001" || r == "2001") {
        u8 val;
        if (parseValue8(value, val)) {
            bus->ppu.setMask(val);
            std::cout << "PPUMASK = $" << std::hex << std::setw(2) << std::setfill('0') 
                      << (int)val << std::dec << std::endl;
        } else {
            std::cout << "Invalid value" << std::endl;
        }
    }
    else if (r == "STATUS" || r == "$2002" || r == "2002") {
        u8 val;
        if (parseValue8(value, val)) {
            bus->ppu.setStatus(val);
            std::cout << "PPUSTATUS = $" << std::hex << std::setw(2) << std::setfill('0') 
                      << (int)val << std::dec << std::endl;
        } else {
            std::cout << "Invalid value" << std::endl;  
        }
    }
    else if (r == "OAMADDR" || r == "$2003" || r == "2003") {
        u8 val;
        if (parseValue8(value, val)) {
            bus->ppu.setOamAddr(val);
            std::cout << "OAMADDR = $" << std::hex << std::setw(2) << std::setfill('0') 
                      << (int)val << std::dec << std::endl;
        } else {
            std::cout << "Invalid value" << std::endl;
        }
    }
    else if (r == "V" || r == "VRAM") {
        u16 val;
        if (parseValue16(value, val)) {
            bus->ppu.setVramAddr(val & 0x7FFF);
            std::cout << "V = $" << std::hex << std::setw(4) << std::setfill('0') 
                      << (val & 0x7FFF) << std::dec << std::endl;
        } else {
            std::cout << "Invalid value" << std::endl;
        }
    }
    else if (r == "T" || r == "TEMP") {
        u16 val;
        if (parseValue16(value, val)) {
            bus->ppu.setTempAddr(val & 0x7FFF);
            std::cout << "T = $" << std::hex << std::setw(4) << std::setfill('0') 
                      << (val & 0x7FFF) << std::dec << std::endl;
        } else {
            std::cout << "Invalid value" << std::endl;
        }
    }
    else if (r == "FINEX" || r == "FX") {
        u8 val;
        if (parseValue8(value, val)) {
            bus->ppu.setFineX(val);
            std::cout << "Fine X = " << (int)(val & 0x07) << std::endl;
        } else {
            std::cout << "Invalid value" << std::endl;
        }
    }
    else {
        std::cout << "Unknown PPU register: " << reg << std::endl;
        std::cout << "Valid registers: CTRL, MASK, OAMADDR, V, T, FINEX" << std::endl;
    }
}

void Debugger::cmdApu()
{
    std::cout << std::hex << std::setfill('0');
    
    std::cout << COLOR_BOLD << "\nAPU Channel Status:" << COLOR_RESET << std::endl;
    
    auto p1 = bus->apu.getPulse1Status();
    std::cout << "  " << COLOR_CYAN << "Pulse 1:" << COLOR_RESET;
    std::cout << "  Enabled:" << (p1.enabled ? "yes" : "no");
    std::cout << "  Vol:" << std::dec << (int)p1.volume;
    std::cout << "  Period:$" << std::hex << std::setw(3) << p1.period;
    std::cout << "  Length:" << std::dec << (int)p1.length << std::endl;
    
    auto p2 = bus->apu.getPulse2Status();
    std::cout << "  " << COLOR_CYAN << "Pulse 2:" << COLOR_RESET;
    std::cout << "  Enabled:" << (p2.enabled ? "yes" : "no");
    std::cout << "  Vol:" << std::dec << (int)p2.volume;
    std::cout << "  Period:$" << std::hex << std::setw(3) << p2.period;
    std::cout << "  Length:" << std::dec << (int)p2.length << std::endl;
    
    auto tri = bus->apu.getTriangleStatus();
    std::cout << "  " << COLOR_CYAN << "Triangle:" << COLOR_RESET;
    std::cout << " Enabled:" << (tri.enabled ? "yes" : "no");
    std::cout << "  Period:$" << std::hex << std::setw(3) << tri.period;
    std::cout << "  Length:" << std::dec << (int)tri.length << std::endl;
    
    auto noi = bus->apu.getNoiseStatus();
    std::cout << "  " << COLOR_CYAN << "Noise:" << COLOR_RESET;
    std::cout << "    Enabled:" << (noi.enabled ? "yes" : "no");
    std::cout << "  Vol:" << std::dec << (int)noi.volume;
    std::cout << "  Period:$" << std::hex << std::setw(3) << noi.period;
    std::cout << "  Length:" << std::dec << (int)noi.length << std::endl;
    
    auto dmc = bus->apu.getDMCStatus();
    std::cout << "  " << COLOR_CYAN << "DMC:" << COLOR_RESET;
    std::cout << "      Enabled:" << (dmc.enabled ? "yes" : "no");
    std::cout << "  Output:" << std::dec << (int)dmc.volume;
    std::cout << "  Rate:$" << std::hex << std::setw(3) << dmc.period << std::endl;
    
    std::cout << std::endl;
    std::cout << "  " << COLOR_CYAN << "Frame Counter:" << COLOR_RESET;
    std::cout << "  Mode:" << (bus->apu.getFrameCounterMode() ? "5-step" : "4-step");
    std::cout << "  IRQ Inhibit:" << (bus->apu.getIrqInhibit() ? "yes" : "no");
    std::cout << std::endl;
    
    std::cout << std::dec;
}

void Debugger::cmdIo()
{
    std::cout << COLOR_BOLD << "\nI/O Status:" << COLOR_RESET << std::endl;
    
    std::cout << "  " << COLOR_CYAN << "Controller 1 ($4016):" << COLOR_RESET << std::endl;
    std::cout << "    Directly write $4016 to strobe controllers" << std::endl;
    
    std::cout << "  " << COLOR_CYAN << "Controller 2 ($4017):" << COLOR_RESET << std::endl;
    std::cout << "    Also APU frame counter register" << std::endl;
    
    std::cout << std::endl;
    std::cout << "  " << COLOR_CYAN << "OAM DMA ($4014):" << COLOR_RESET << std::endl;
    std::cout << "    Write page number to trigger DMA" << std::endl;
    
    std::cout << std::endl;
    std::cout << "Use 'w $addr $val' to write to I/O registers:" << std::endl;
    std::cout << "  $4000-$4013: APU registers" << std::endl;
    std::cout << "  $4014: OAM DMA" << std::endl;
    std::cout << "  $4015: APU status" << std::endl;
    std::cout << "  $4016: Controller strobe" << std::endl;
    std::cout << "  $4017: Frame counter" << std::endl;
}

void Debugger::cmdWrite(u16 addr, u8 value)
{
    bus->cpuWrite(addr, value);
    std::cout << "Wrote $" << std::hex << std::setw(2) << std::setfill('0') << (int)value
              << " to $" << std::setw(4) << addr << " [" << getMemoryRegion(addr) << "]:" << std::dec << std::endl;
}

std::vector<std::string> Debugger::tokenize(const std::string& line)
{
    std::vector<std::string> tokens;
    std::istringstream iss(line);
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

std::string Debugger::readLineWithHistory()
{
    struct termios old_tio, new_tio;
    tcgetattr(STDIN_FILENO, &old_tio);
    new_tio = old_tio;
    new_tio.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);

    std::string line;
    int cursor_pos = 0;
    int temp_history_index = command_history.size();
    std::string temp_line = "";

    while (true) {
        char c;
        if (read(STDIN_FILENO, &c, 1) != 1) {
            break;
        }

        if (c == '\n') {
            std::cout << std::endl;
            break;
        }
        else if (c == 127 || c == 8) {  // Backspace
            if (cursor_pos > 0) {
                line.erase(cursor_pos - 1, 1);
                cursor_pos--;
                // Redraw line
                std::cout << "\r" << COLOR_BOLD << "> " << COLOR_RESET << line << " ";
                std::cout << "\r" << COLOR_BOLD << "> " << COLOR_RESET << line;
                // Move cursor to correct position
                for (int i = line.length(); i > cursor_pos; i--) {
                    std::cout << "\b";
                }
                std::cout.flush();
            }
        }
        else if (c == 27) {  // Escape sequence
            char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) != 1) continue;
            if (read(STDIN_FILENO, &seq[1], 1) != 1) continue;

            if (seq[0] == '[') {
                if (seq[1] == 'A') {  // Up arrow
                    if (temp_history_index > 0) {
                        if (temp_history_index == (int)command_history.size()) {
                            temp_line = line;  // Save current line
                        }
                        temp_history_index--;
                        line = command_history[temp_history_index];
                        cursor_pos = line.length();
                        // Redraw line
                        std::cout << "\r" << COLOR_BOLD << "> " << COLOR_RESET;
                        std::cout << std::string(temp_line.length() + 10, ' ');
                        std::cout << "\r" << COLOR_BOLD << "> " << COLOR_RESET << line;
                        std::cout.flush();
                    }
                }
                else if (seq[1] == 'B') {  // Down arrow
                    if (temp_history_index < (int)command_history.size()) {
                        temp_history_index++;
                        if (temp_history_index == (int)command_history.size()) {
                            line = temp_line;  // Restore saved line
                        } else {
                            line = command_history[temp_history_index];
                        }
                        cursor_pos = line.length();
                        // Redraw line
                        std::cout << "\r" << COLOR_BOLD << "> " << COLOR_RESET;
                        std::cout << std::string(50, ' ');
                        std::cout << "\r" << COLOR_BOLD << "> " << COLOR_RESET << line;
                        std::cout.flush();
                    }
                }
                else if (seq[1] == 'C') {  // Right arrow
                    if (cursor_pos < (int)line.length()) {
                        cursor_pos++;
                        std::cout << "\033[C";
                        std::cout.flush();
                    }
                }
                else if (seq[1] == 'D') {  // Left arrow
                    if (cursor_pos > 0) {
                        cursor_pos--;
                        std::cout << "\033[D";
                        std::cout.flush();
                    }
                }
            }
        }
        else if (c >= 32 && c < 127) {  // Printable character
            line.insert(cursor_pos, 1, c);
            cursor_pos++;
            // Redraw from cursor position
            std::cout << c;
            if (cursor_pos < (int)line.length()) {
                std::cout << line.substr(cursor_pos);
                for (int i = line.length(); i > cursor_pos; i--) {
                    std::cout << "\b";
                }
            }
            std::cout.flush();
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
    return line;
}

void Debugger::run()
{
    std::cout << COLOR_BOLD << "\n=== VNES Debugger ===" << COLOR_RESET << std::endl;
    std::cout << "Type 'h' or 'help' for commands\n" << std::endl;

    saveRegisters();
    
    // Show initial state
    int len;
    std::cout << disassembleInstruction(bus->cpu.getPC(), len) << std::endl;
    printRegisters();

    quit = false;
    std::string line;

    while (!quit) {
        std::cout << COLOR_BOLD << "\n> " << COLOR_RESET;
        std::cout.flush();
        
        line = readLineWithHistory();

        std::vector<std::string> tokens = tokenize(line);
        if (tokens.empty()) {
            // Repeat last command (step)
            cmdStep(1);
            continue;
        }
        
        // Add non-empty command to history
        if (!line.empty()) {
            // Avoid duplicate consecutive commands
            if (command_history.empty() || command_history.back() != line) {
                command_history.push_back(line);
            }
        }

        const std::string& cmd = tokens[0];

        if (cmd == "h" || cmd == "help") {
            cmdHelp();
        }
        else if (cmd == "s" || cmd == "step") {
            int count = 1;
            if (tokens.size() > 1) {
                count = std::stoi(tokens[1]);
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
            u16 addr = bus->cpu.getPC();
            int count = 10;
            if (tokens.size() > 1) {
                parseAddress(tokens[1], addr);
            }
            if (tokens.size() > 2) {
                count = std::stoi(tokens[2]);
            }
            cmdDisassemble(addr, count);
        }
        else if (cmd == "mr" || cmd == "memread") {
            if (tokens.size() < 2) {
                std::cout << "Usage: memread <addr> [count]" << std::endl;
            } else {
                u16 addr;
                int count = 64;
                if (parseAddress(tokens[1], addr)) {
                    if (tokens.size() > 2) {
                        count = std::stoi(tokens[2]);
                    }
                    cmdMemory(addr, count);
                } else {
                    std::cout << "Invalid address" << std::endl;
                }
            }
        }
        else if (cmd == "mw" || cmd == "memwrite") {
            if (tokens.size() < 3) {
                std::cout << "Usage: memwrite <addr> <value>" << std::endl;
            } else {
                u16 addr;
                u8 val;
                if (parseAddress(tokens[1], addr) && parseValue8(tokens[2], val)) {
                    cmdWrite(addr, val);
                } else {
                    std::cout << "Invalid address or value" << std::endl;
                }
            }
        }
        else if (cmd == "b" || cmd == "break") {
            if (tokens.size() < 2) {
                std::cout << "Usage: break <addr>" << std::endl;
            } else {
                u16 addr;
                if (parseAddress(tokens[1], addr)) {
                    cmdBreakpoint(addr);
                } else {
                    std::cout << "Invalid address" << std::endl;
                }
            }
        }
        else if (cmd == "del") {
            if (tokens.size() < 2) {
                std::cout << "Usage: del <addr>" << std::endl;
            } else {
                u16 addr;
                if (parseAddress(tokens[1], addr)) {
                    cmdDeleteBreakpoint(addr);
                } else {
                    std::cout << "Invalid address" << std::endl;
                }
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
                std::cout << "Usage: regset <reg> <value>" << std::endl;
                std::cout << "Registers: A, X, Y, SP, PC, P" << std::endl;
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
                std::cout << "Usage: ppu             - Show PPU registers" << std::endl;
                std::cout << "       ppu <reg> <val> - Set PPU register" << std::endl;
            }
        }
        else if (cmd == "apu") {
            cmdApu();
        }
        else if (cmd == "io") {
            cmdIo();
        }
        else if (cmd == "q" || cmd == "quit") {
            quit = true;
        }
        else {
            std::cout << "Unknown command: " << cmd << ". Type 'help' for commands." << std::endl;
        }
    }

    std::cout << "\nDebugger exited." << std::endl;
}
