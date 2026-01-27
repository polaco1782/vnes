#include "mapper_002.h"

Mapper002::Mapper002()
    : Mapper(2)
    , prgBankSelect(0)
    , prgBankOffset(0)
{
}

void Mapper002::init(std::vector<uint8_t>& prg, std::vector<uint8_t>& chr,
                     std::vector<uint8_t>& ram, Mirroring initialMirroring)
{
    Mapper::init(prg, chr, ram, initialMirroring);

    // Reset register
    prgBankSelect = 0;

    // Initialize bank offset
    prgBankOffset = 0;
}

uint8_t Mapper002::readPrg(uint16_t addr)
{
    // PRG RAM at $6000-$7FFF
    if (addr >= 0x6000 && addr < 0x8000) {
        if (prgRam && !prgRam->empty()) {
            return (*prgRam)[addr - 0x6000];
        }
        return 0;
    }

    // PRG ROM at $8000-$FFFF
    if (addr >= 0x8000) {
        if (!prgRom || prgRom->empty()) return 0;

        if (addr < 0xC000) {
            // $8000-$BFFF: Switchable bank
            return (*prgRom)[prgBankOffset + (addr - 0x8000)];
        } else {
            // $C000-$FFFF: Fixed to last 16KB bank
            uint32_t lastBankOffset = prgRom->size() - PRG_ROM_UNIT;
            return (*prgRom)[lastBankOffset + (addr - 0xC000)];
        }
    }

    return 0;
}

void Mapper002::writePrg(uint16_t addr, uint8_t data)
{
    // PRG RAM at $6000-$7FFF
    if (addr >= 0x6000 && addr < 0x8000) {
        if (prgRam && !prgRam->empty()) {
            (*prgRam)[addr - 0x6000] = data;
        }
        return;
    }

    // Bank select register at $8000-$FFFF
    if (addr >= 0x8000) {
        prgBankSelect = data & 0x0F;  // 4 bits for bank selection
        uint32_t bankCount = prgRom->size() / PRG_ROM_UNIT;
        prgBankOffset = (prgBankSelect % bankCount) * PRG_ROM_UNIT;
    }
}

uint8_t Mapper002::readChr(uint16_t addr)
{
    if (chrRom && !chrRom->empty() && addr < 0x2000) {
        return (*chrRom)[addr % chrRom->size()];
    }
    return 0;
}

void Mapper002::writeChr(uint16_t addr, uint8_t data)
{
    // CHR RAM is writable (when chr_rom_size == 0 in header)
    if (chrRom && !chrRom->empty() && addr < 0x2000) {
        (*chrRom)[addr % chrRom->size()] = data;
    }
}
