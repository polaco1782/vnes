#include "bus.h"
#include "input.h"
#include <cstring>
#include <sstream>

Bus::Bus()
    : input(nullptr), cartridge(nullptr), system_cycles(0), log_accesses(false)
{
    std::memset(ram, 0, sizeof(ram));

    // Connect components
    cpu.connect(this);
    ppu.connect(this, nullptr);
}

std::string Bus::getRegionName(u16 addr) const
{
    if (addr < 0x0800) {
        std::ostringstream ss;
        ss << "RAM[$" << std::hex << addr << "]";
        return ss.str();
    }
    else if (addr < 0x2000) {
        std::ostringstream ss;
        ss << "RAM[$" << std::hex << (addr & 0x07FF) << "] (mirror)";
        return ss.str();
    }
    else if (addr < 0x4000) {
        static const char* ppu_regs[] = {
            "PPUCTRL", "PPUMASK", "PPUSTATUS", "OAMADDR",
            "OAMDATA", "PPUSCROLL", "PPUADDR", "PPUDATA"
        };
        return ppu_regs[addr & 0x07];
    }
    else if (addr < 0x4018) {
        switch (addr) {
            case 0x4000: return "SQ1_VOL";
            case 0x4001: return "SQ1_SWEEP";
            case 0x4002: return "SQ1_LO";
            case 0x4003: return "SQ1_HI";
            case 0x4004: return "SQ2_VOL";
            case 0x4005: return "SQ2_SWEEP";
            case 0x4006: return "SQ2_LO";
            case 0x4007: return "SQ2_HI";
            case 0x4008: return "TRI_LINEAR";
            case 0x400A: return "TRI_LO";
            case 0x400B: return "TRI_HI";
            case 0x400C: return "NOISE_VOL";
            case 0x400E: return "NOISE_LO";
            case 0x400F: return "NOISE_HI";
            case 0x4010: return "DMC_FREQ";
            case 0x4011: return "DMC_RAW";
            case 0x4012: return "DMC_START";
            case 0x4013: return "DMC_LEN";
            case 0x4014: return "OAMDMA";
            case 0x4015: return "APU_STATUS";
            case 0x4016: return "JOY1";
            case 0x4017: return "JOY2/FRAME";
            default: {
                std::ostringstream ss;
                ss << "IO[$" << std::hex << addr << "]";
                return ss.str();
            }
        }
    }
    else if (addr < 0x6000) {
        return "Expansion";
    }
    else if (addr < 0x8000) {
        std::ostringstream ss;
        ss << "SRAM[$" << std::hex << (addr - 0x6000) << "]";
        return ss.str();
    }
    else {
        std::ostringstream ss;
        ss << "PRG[$" << std::hex << (addr - 0x8000) << "]";
        return ss.str();
    }
}

void Bus::logAccess(MemAccess::Type type, u16 addr, u8 value)
{
    if (!log_accesses) return;
    
    MemAccess access;
    access.type = type;
    access.addr = addr;
    access.value = value;
    access.region = getRegionName(addr);
    access_log.push_back(access);
}

void Bus::connect(Cartridge* cart)
{
    cartridge = cart;
    ppu.connect(this, cart);
}

void Bus::connectInput(Input* input_device)
{
    input = input_device;
}

void Bus::reset()
{
    cpu.reset();
    ppu.reset();
    apu.reset();
    system_cycles = 0;
}

void Bus::clock()
{
    // PPU runs at 3x CPU speed
    ppu.step();

    if (system_cycles % 3 == 0) {
        cpu.step();
        apu.step();
    }

    // Handle NMI from PPU
    if (ppu.isNMI()) {
        ppu.clearNMI();
        cpu.nmi();
    }

    // Handle IRQ from APU
    if (apu.isIRQ()) {
        apu.clearIRQ();
        cpu.irq();
    }

    system_cycles++;
}

u8 Bus::cpuRead(u16 addr)
{
    u8 data = 0;
    
    if (addr < 0x2000) {
        // Internal RAM (mirrored every 0x800 bytes)
        data = ram[addr & 0x07FF];
    }
    else if (addr < 0x4000) {
        // PPU registers (mirrored every 8 bytes)
        data = ppu.readRegister(addr);
    }
    else if (addr < 0x4018) {
        // APU and I/O registers
        if (addr == 0x4016) {
            // Controller 1
            data = input ? input->read() : 0x40;
        }
        else if (addr == 0x4017) {
            // Controller 2 (not implemented)
            data = 0x40;
        }
        else {
            data = apu.readRegister(addr);
        }
    }
    else if (addr >= 0x4020) {
        // Cartridge space
        if (cartridge) {
            data = cartridge->readPrg(addr - 0x8000);
        }
    }

    logAccess(MemAccess::READ, addr, data);
    return data;
}

void Bus::cpuWrite(u16 addr, u8 data)
{
    logAccess(MemAccess::WRITE, addr, data);
    
    if (addr < 0x2000) {
        // Internal RAM
        ram[addr & 0x07FF] = data;
    }
    else if (addr < 0x4000) {
        // PPU registers
        ppu.writeRegister(addr, data);
    }
    else if (addr < 0x4018) {
        // APU and I/O registers
        if (addr == 0x4014) {
            // OAM DMA
            ppu.writeDMA(data);
        }
        else if (addr == 0x4016) {
            // Controller strobe
            if (input && (data & 0x01)) {
                input->strobe();
            }
        }
        else {
            apu.writeRegister(addr, data);
        }
    }
    else if (addr >= 0x4020) {
        // Cartridge space (some mappers have writable registers)
        // Currently not implemented for mapper 0
    }
}
