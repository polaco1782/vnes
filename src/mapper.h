#ifndef MAPPER_H
#define MAPPER_H

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <iostream>

#include "types.h"

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
    Mapper(u8 mapperNumber);
    virtual ~Mapper() = default;

    // Initialize the mapper with ROM data
    virtual void init(std::vector<u8>& prg, std::vector<u8>& chr, 
                      std::vector<u8>& prgRam, Mirroring initialMirroring);

    // CPU interface for PRG space ($6000-$FFFF)
    virtual u8 readPrg(u16 addr) = 0;
    virtual void writePrg(u16 addr, u8 data) = 0;

    // PPU interface for CHR space ($0000-$1FFF)
    virtual u8 readChr(u16 addr) = 0;
    virtual void writeChr(u16 addr, u8 data) = 0;

    // Mirroring control - some mappers can change mirroring dynamically
    Mirroring getMirroring() const { return mirroring; }

	// IRQ handling - for mappers that support scanline-based IRQs (e.g. MMC3)
	virtual bool irqPending() const { return false; }
	virtual void clearIrq() {}

    // Mapper info
    u8 getMapperNumber() const { return mapperNum; }
    virtual const char* getName() const = 0;

    // Optional: scanline counter for mappers like MMC3
    virtual void scanline() {}

    // Optional: PPU address notification for mappers like MMC2/MMC4
    virtual void notifyPpuAddr(u16 addr) { (void)addr; }

protected:
    u8 mapperNum;
    Mirroring mirroring;
    
    // Pointers to cartridge memory (owned by Cartridge)
    std::vector<u8>* prgRom;
    std::vector<u8>* chrRom;
    std::vector<u8>* prgRam;
};

/**
 * Mapper Factory - creates the appropriate mapper based on mapper number
 */
class MapperFactory {
public:
    static std::unique_ptr<Mapper> create(u8 mapperNumber);
};

#endif // MAPPER_H
