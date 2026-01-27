#ifndef MAPPER_002_H
#define MAPPER_002_H

#include "mapper.h"

/**
 * Mapper 002 - UxROM
 * 
 * Used by games like Mega Man, Castlevania, Duck Tales, and many others.
 * 
 * Features:
 *   - PRG ROM: Up to 256KB, switchable in 16KB banks
 *   - CHR ROM: 8KB fixed (can be CHR RAM if size=0)
 *   - PRG RAM: 8KB at $6000-$7FFF (optional)
 *   - Fixed mirroring (horizontal or vertical from ROM header)
 * 
 * PRG Memory Map:
 *   $6000-$7FFF: 8KB PRG RAM (optional)
 *   $8000-$BFFF: 16KB switchable PRG ROM bank
 *   $C000-$FFFF: 16KB fixed to last bank of PRG ROM
 * 
 * CHR Memory Map:
 *   $0000-$1FFF: 8KB fixed CHR ROM/RAM
 * 
 * Registers:
 *   $8000-$FFFF: Bank select (writes to any address in this range)
 *                Bits 0-3: Select 16KB PRG bank at $8000-$BFFF
 */
class Mapper002 : public Mapper {
public:
    Mapper002();
    ~Mapper002() override = default;

    void init(std::vector<uint8_t>& prg, std::vector<uint8_t>& chr,
              std::vector<uint8_t>& prgRam, Mirroring initialMirroring) override;

    uint8_t readPrg(uint16_t addr) override;
    void writePrg(uint16_t addr, uint8_t data) override;

    uint8_t readChr(uint16_t addr) override;
    void writeChr(uint16_t addr, uint8_t data) override;

    const char* getName() const override { return "UxROM"; }

private:
    // PRG bank register
    uint8_t prgBankSelect;

    // Computed bank offsets
    uint32_t prgBankOffset;  // Switchable 16KB bank at $8000
};

#endif // MAPPER_002_H
