#ifndef MAPPER_009_H
#define MAPPER_009_H

#include "mapper.h"

/**
 * Mapper 009 - MMC2 (Nintendo PxROM)
 * 
 * Used exclusively by Mike Tyson's Punch-Out!! and Punch-Out!!
 * 
 * Features:
 *   - PRG ROM: 128KB, one switchable 8KB bank + three fixed 8KB banks
 *   - CHR ROM: 128KB with unique latch-based switching
 *   - 8KB PRG RAM at $6000-$7FFF
 *   - Fixed vertical mirroring (can be changed via register)
 * 
 * The unique feature of MMC2 is its CHR latch mechanism:
 *   - Two 4KB CHR banks, each with two selectable pages
 *   - Latches are triggered by specific tile fetches ($FD or $FE tiles)
 *   - This allows sprite-0-hit based CHR switching for animations
 * 
 * PRG Memory Map:
 *   $6000-$7FFF: 8KB PRG RAM
 *   $8000-$9FFF: 8KB switchable PRG ROM bank
 *   $A000-$BFFF: Fixed to second-to-last 8KB bank
 *   $C000-$DFFF: Fixed to third-to-last 8KB bank  
 *   $E000-$FFFF: Fixed to last 8KB bank
 * 
 * CHR Memory Map:
 *   $0000-$0FFF: 4KB switchable (latch 0 selects between two banks)
 *   $1000-$1FFF: 4KB switchable (latch 1 selects between two banks)
 * 
 * Registers:
 *   $A000-$AFFF: PRG bank select (bits 0-3)
 *   $B000-$BFFF: CHR bank 0 select for $FD latch
 *   $C000-$CFFF: CHR bank 0 select for $FE latch
 *   $D000-$DFFF: CHR bank 1 select for $FD latch
 *   $E000-$EFFF: CHR bank 1 select for $FE latch
 *   $F000-$FFFF: Mirroring (bit 0: 0=vertical, 1=horizontal)
 */
class Mapper009 : public Mapper {
public:
    Mapper009();
    ~Mapper009() override = default;

    void init(std::vector<uint8_t>& prg, std::vector<uint8_t>& chr,
              std::vector<uint8_t>& prgRam, Mirroring initialMirroring) override;

    uint8_t readPrg(uint16_t addr) override;
    void writePrg(uint16_t addr, uint8_t data) override;

    uint8_t readChr(uint16_t addr) override;
    void writeChr(uint16_t addr, uint8_t data) override;

    const char* getName() const override { return "MMC2"; }

private:
    void updateChrBanks();

    // PRG bank register
    uint8_t prgBankSelect;

    // CHR bank registers (two banks for each latch state)
    uint8_t chrBank0FD;   // CHR $0000-$0FFF when latch 0 = $FD
    uint8_t chrBank0FE;   // CHR $0000-$0FFF when latch 0 = $FE
    uint8_t chrBank1FD;   // CHR $1000-$1FFF when latch 1 = $FD
    uint8_t chrBank1FE;   // CHR $1000-$1FFF when latch 1 = $FE

    // Latches (triggered by specific tile patterns)
    bool latch0;  // false = $FD, true = $FE
    bool latch1;  // false = $FD, true = $FE

    // Computed bank offsets
    uint32_t prgBankOffset;     // For the switchable 8KB bank at $8000
    uint32_t chrBankOffset[2];  // Two 4KB CHR banks
};

#endif // MAPPER_009_H
