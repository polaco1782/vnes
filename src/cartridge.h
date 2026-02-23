#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
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

struct GGCode {
    std::string code;
    u16 addr;
    u8 value;
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
    void Cartridge::scanline();
    void Cartridge::clearIRQ();
    bool Cartridge::hasIRQ();

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

    std::unordered_map<u32, GGCode> gg_codes; // GameGenie codes (address -> GGCode)
    
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
