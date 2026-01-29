#ifndef MAPPER_001_H
#define MAPPER_001_H

#include "mapper.h"

/**
 * Mapper 001 - MMC1 (Nintendo SxROM)
 * 
 * One of the most common NES mappers, used by many games including
 * The Legend of Zelda, Metroid, and many others.
 * 
 * Features:
 *   - PRG ROM: Up to 512KB, switchable in 16KB or 32KB banks
 *   - CHR ROM/RAM: Up to 128KB, switchable in 4KB or 8KB banks
 *   - PRG RAM: 8KB at $6000-$7FFF with optional battery backup
 *   - Programmable mirroring
 * 
 * Registers are written serially, 5 bits at a time via a shift register.
 * Writing with bit 7 set resets the shift register.
 * 
 * Register Map:
 *   $8000-$9FFF: Control (mirroring, PRG/CHR mode)
 *   $A000-$BFFF: CHR bank 0
 *   $C000-$DFFF: CHR bank 1
 *   $E000-$FFFF: PRG bank
 */
class Mapper001 : public Mapper {
public:
    Mapper001();

    void init(std::vector<uint8_t>& prg, std::vector<uint8_t>& chr,
              std::vector<uint8_t>& prgRam, Mirroring initialMirroring) override;

    uint8_t readPrg(uint16_t addr) override;
    void writePrg(uint16_t addr, uint8_t data) override;

    uint8_t readChr(uint16_t addr) override;
    void writeChr(uint16_t addr, uint8_t data) override;

    const char* getName() const override { return "MMC1"; }

private:
    void writeRegister(uint16_t addr, uint8_t data);
    void updateBanks();

    // Shift register
    uint8_t shiftReg;
    uint8_t shiftCount;

    // Internal registers
    uint8_t ctrlReg;      // Control: mirroring, PRG/CHR modes
    uint8_t chrBank0;     // CHR bank 0 select
    uint8_t chrBank1;     // CHR bank 1 select
    uint8_t prgBank;      // PRG bank select

    // Computed bank offsets
    uint32_t prgBankOffset[2];  // Two 16KB PRG banks
    uint32_t chrBankOffset[2];  // Two 4KB CHR banks
};

#endif // MAPPER_001_H
