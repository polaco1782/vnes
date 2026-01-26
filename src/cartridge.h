#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <array>
#include "mapper.h"
#include "types.h"

// iNES Header (16 bytes)
struct INESHeader {
    u8 magic[4];      // "NES" + 0x1A
    u8 prg_rom_size;  // PRG ROM size in 16KB units
    u8 chr_rom_size;  // CHR ROM size in 8KB units
    u8 flags6;        // Mapper, mirroring, battery, trainer
    u8 flags7;        // Mapper, VS/Playchoice, NES 2.0
    u8 flags8;        // PRG-RAM size (rarely used)
    u8 flags9;        // TV system (rarely used)
    u8 flags10;       // TV system, PRG-RAM (unofficial)
    u8 padding[5];    // Unused padding
};

class Cartridge {
public:
    Cartridge();
    ~Cartridge();

    // Load ROM from file, returns true on success
    bool load(const std::string& filepath);

    // Getters
    bool isLoaded() const { return loaded; }
    u8 getMapperNumber() const { return mapperNumber; }
    Mirroring getMirroring() const;
    bool hasBattery() const { return battery; }
    const char* getMapperName() const;
    Mapper* getMapper() const { return mapper.get(); }

    // ROM data access (for debugging/inspection)
    const std::vector<u8>& getPrgRom() const { return prg_rom; }
    const std::vector<u8>& getChrRom() const { return chr_rom; }

    // CPU interface for PRG space ($6000-$FFFF)
    u8 readPrg(u16 addr) const;
    void writePrg(u16 addr, u8 data);
    
    // PPU interface for CHR space ($0000-$1FFF)
    u8 readChr(u16 addr) const;
    void writeChr(u16 addr, u8 data);

	// Mapper-specific timing (for mappers that need it, eg: MMC3)
    void scanline();
    void clearIRQ();
    bool hasIRQ();

    bool addGGCode(const std::string& code);
    void removeGGCode(const std::string& code);

    void signalFrameComplete();
    void flushSRAM();

private:
    bool parseHeader(const INESHeader& header);
    
    bool loaded;
    u8 mapperNumber;
    bool battery;

    std::vector<u8> prg_rom;  // Program ROM
    std::vector<u8> chr_rom;  // Character ROM (can be RAM if size=0)
    std::vector<u8> prg_ram;  // PRG RAM at $6000-$7FFF (8KB)

    // Active Game Genie entries. We keep a small dense array of at most
    // MAX_GG_CODES entries and a `gg_count` to avoid scanning when empty.
    static const size_t MAX_GG_CODES = 16;
    uint8_t gg_count;
    struct GGActiveEntry {
        std::string code; // human-readable code string (optional)
        u16 addr = 0;
        u8 value = 0;
        u8 compare = 0;
        bool has_compare = false;
        u32 hits = 0;
    };
    std::array<GGActiveEntry, MAX_GG_CODES> gg_active_entries;
    
    // The pluggable mapper
    std::unique_ptr<Mapper> mapper;
    
    // Initial mirroring from ROM header (before mapper can change it)
    Mirroring initialMirroring;

    // SRAM persistence (for battery-backed carts)
    bool prgRamDirty;
    u32 framesSinceLastSave;
    std::string savePath;
};

#endif // CARTRIDGE_H
