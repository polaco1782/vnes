#ifndef BUS_H
#define BUS_H

#include "types.h"
#include "cpu.h"
#include "ppu.h"
#include "apu.h"
#include "cartridge.h"
#include <vector>
#include <string>

// Forward declarations
class Input;

// Memory access record for debugging
struct MemAccess {
    enum Type { READ, WRITE };
    Type type;
    u16 addr;
    u8 value;
    std::string region;  // Description of memory region
};

class Bus {
public:
    Bus();

    void connect(Cartridge* cart);
    void reset();

    // Clock the entire system
    void clock();

    // CPU memory interface
    u8 cpuRead(u16 addr);
    void cpuWrite(u16 addr, u8 data);

    // Components (public for direct access)
    CPU cpu;
    PPU ppu;
    APU apu;

    // Input connection
    void connectInput(Input* input_device);
    
    // Frame status
    bool isFrameComplete() const { return ppu.isFrameComplete(); }
    void clearFrameComplete() { ppu.clearFrameComplete(); }
    const u32* getFramebuffer() const { return ppu.getFramebuffer(); }

    // Debug: memory access tracking
    void enableAccessLog(bool enable) { log_accesses = enable; }
    void clearAccessLog() { access_log.clear(); }
    const std::vector<MemAccess>& getAccessLog() const { return access_log; }

private:
    // Internal RAM (2KB, mirrored)
    u8 ram[2048];

    // Input device
    Input* input;

    // Cartridge
    Cartridge* cartridge;

    // System cycles
    u64 system_cycles;

    // Debug: access logging
    bool log_accesses;
    std::vector<MemAccess> access_log;
    
    void logAccess(MemAccess::Type type, u16 addr, u8 value);
    std::string getRegionName(u16 addr) const;
};

#endif // BUS_H
