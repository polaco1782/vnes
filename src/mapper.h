#ifndef MAPPER_H
#define MAPPER_H

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <iostream>

// Forward declaration
class Cartridge;

// Mirroring modes
enum class Mirroring {
    HORIZONTAL,
    VERTICAL,
    FOUR_SCREEN,
    SINGLE_LOWER,
    SINGLE_UPPER
};

// Constants
static const uint32_t PRG_ROM_UNIT = 16384;  // 16KB
static const uint32_t CHR_ROM_UNIT = 8192;   // 8KB
static const uint32_t PRG_BANK_8K = 8192;    // 8KB
static const uint32_t PRG_BANK_4K = 4096;    // 4KB
static const uint32_t CHR_BANK_4K = 4096;    // 4KB
static const uint32_t CHR_BANK_1K = 1024;    // 1KB
static const uint32_t TRAINER_SIZE = 512;

/**
 * Base Mapper class - interface for all NES mappers
 * 
 * Mappers handle bank switching for PRG and CHR memory,
 * as well as controlling nametable mirroring.
 */
class Mapper {
public:
    Mapper(uint8_t mapperNumber);
    virtual ~Mapper() = default;

    // Initialize the mapper with ROM data
    virtual void init(std::vector<uint8_t>& prg, std::vector<uint8_t>& chr, 
                      std::vector<uint8_t>& prgRam, Mirroring initialMirroring);

    // CPU interface for PRG space ($6000-$FFFF)
    virtual uint8_t readPrg(uint16_t addr) = 0;
    virtual void writePrg(uint16_t addr, uint8_t data) = 0;

    // PPU interface for CHR space ($0000-$1FFF)
    virtual uint8_t readChr(uint16_t addr) = 0;
    virtual void writeChr(uint16_t addr, uint8_t data) = 0;

    // Mirroring control - some mappers can change mirroring dynamically
    Mirroring getMirroring() const { return mirroring; }

    // Mapper info
    uint8_t getMapperNumber() const { return mapperNum; }
    virtual const char* getName() const = 0;

    // Optional: scanline counter for mappers like MMC3
    virtual void scanline() {}

    // Optional: PPU address notification for mappers like MMC2/MMC4
    virtual void notifyPpuAddr(uint16_t addr) { (void)addr; }

protected:
    uint8_t mapperNum;
    Mirroring mirroring;
    
    // Pointers to cartridge memory (owned by Cartridge)
    std::vector<uint8_t>* prgRom;
    std::vector<uint8_t>* chrRom;
    std::vector<uint8_t>* prgRam;
};

/**
 * Mapper Factory - creates the appropriate mapper based on mapper number
 */
class MapperFactory {
public:
    static std::unique_ptr<Mapper> create(uint8_t mapperNumber);
};

#endif // MAPPER_H
