#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include "mapper.h"

// iNES Header (16 bytes)
struct INESHeader {
    uint8_t magic[4];      // "NES" + 0x1A
    uint8_t prg_rom_size;  // PRG ROM size in 16KB units
    uint8_t chr_rom_size;  // CHR ROM size in 8KB units
    uint8_t flags6;        // Mapper, mirroring, battery, trainer
    uint8_t flags7;        // Mapper, VS/Playchoice, NES 2.0
    uint8_t flags8;        // PRG-RAM size (rarely used)
    uint8_t flags9;        // TV system (rarely used)
    uint8_t flags10;       // TV system, PRG-RAM (unofficial)
    uint8_t padding[5];    // Unused padding
};

class Cartridge {
public:
    Cartridge();
    ~Cartridge();

    // Load ROM from file, returns true on success
    bool load(const std::string& filepath);

    // Getters
    bool isLoaded() const { return loaded; }
    uint8_t getMapperNumber() const { return mapperNumber; }
    Mirroring getMirroring() const;
    bool hasBattery() const { return battery; }
    const char* getMapperName() const;

    // ROM data access (for debugging/inspection)
    const std::vector<uint8_t>& getPrgRom() const { return prg_rom; }
    const std::vector<uint8_t>& getChrRom() const { return chr_rom; }

    // CPU interface for PRG space ($6000-$FFFF)
    uint8_t readPrg(uint16_t addr) const;
    void writePrg(uint16_t addr, uint8_t data);
    
    // PPU interface for CHR space ($0000-$1FFF)
    uint8_t readChr(uint16_t addr) const;
    void writeChr(uint16_t addr, uint8_t data);

    void signalFrameComplete();
    void flushSRAM();

private:
    bool parseHeader(const INESHeader& header);
    
    bool loaded;
    uint8_t mapperNumber;
    bool battery;

    std::vector<uint8_t> prg_rom;  // Program ROM
    std::vector<uint8_t> chr_rom;  // Character ROM (can be RAM if size=0)
    std::vector<uint8_t> prg_ram;  // PRG RAM at $6000-$7FFF (8KB)
    
    // The pluggable mapper
    std::unique_ptr<Mapper> mapper;
    
    // Initial mirroring from ROM header (before mapper can change it)
    Mirroring initialMirroring;

    // SRAM persistence (for battery-backed carts)
    bool prgRamDirty;
    uint32_t framesSinceLastSave;
    std::string savePath;
};

#endif // CARTRIDGE_H
