#include "cartridge.h"
#include <fstream>
#include <iostream>

// Constants
static const uint32_t PRG_ROM_UNIT = 16384;  // 16KB
static const uint32_t CHR_ROM_UNIT = 8192;   // 8KB
static const uint32_t TRAINER_SIZE = 512;

Cartridge::Cartridge()
    : loaded(false)
    , mapper(0)
    , mirroring(Mirroring::HORIZONTAL)
    , battery(false)
    , mmc1_shift(0x10)
    , mmc1_shift_count(0)
    , mmc1_ctrl(0x0C)  // Default: 16KB PRG mode, fix last bank
    , mmc1_chr_bank0(0)
    , mmc1_chr_bank1(0)
    , mmc1_prg_bank(0)
{
    prg_bank_offset[0] = 0;
    prg_bank_offset[1] = 0;
    chr_bank_offset[0] = 0;
    chr_bank_offset[1] = 0;
}

Cartridge::~Cartridge()
{
}

bool Cartridge::load(const std::string& filepath)
{
    // Reset state
    loaded = false;
    prg_rom.clear();
    chr_rom.clear();
    prg_ram.clear();
    
    // Reset MMC1 state
    mmc1_shift = 0x10;
    mmc1_shift_count = 0;
    mmc1_ctrl = 0x0C;
    mmc1_chr_bank0 = 0;
    mmc1_chr_bank1 = 0;
    mmc1_prg_bank = 0;

    // Open file
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file: " << filepath << std::endl;
        return false;
    }

    // Read header
    INESHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!file) {
        std::cerr << "Error: Cannot read header" << std::endl;
        return false;
    }

    // Parse and validate header
    if (!parseHeader(header)) {
        return false;
    }

    // Skip trainer if present
    bool hasTrainer = (header.flags6 & 0x04) != 0;
    if (hasTrainer) {
        file.seekg(TRAINER_SIZE, std::ios::cur);
    }

    // Read PRG ROM
    uint32_t prg_size = header.prg_rom_size * PRG_ROM_UNIT;
    prg_rom.resize(prg_size);
    file.read(reinterpret_cast<char*>(prg_rom.data()), prg_size);
    if (!file) {
        std::cerr << "Error: Cannot read PRG ROM" << std::endl;
        return false;
    }

    // Read CHR ROM (or allocate CHR RAM if size is 0)
    uint32_t chr_size = header.chr_rom_size * CHR_ROM_UNIT;
    if (chr_size > 0) {
        chr_rom.resize(chr_size);
        file.read(reinterpret_cast<char*>(chr_rom.data()), chr_size);
        if (!file) {
            std::cerr << "Error: Cannot read CHR ROM" << std::endl;
            return false;
        }
    } else {
        // CHR RAM - allocate 8KB
        chr_rom.resize(CHR_ROM_UNIT, 0);
    }
    
    // Allocate 8KB PRG RAM at $6000-$7FFF
    prg_ram.resize(8192, 0);
    
    // Initialize mapper-specific bank offsets
    if (mapper == 1) {
        // MMC1 defaults: first 16KB at $8000, last 16KB at $C000
        prg_bank_offset[0] = 0;
        prg_bank_offset[1] = prg_rom.size() - PRG_ROM_UNIT;
        chr_bank_offset[0] = 0;
        chr_bank_offset[1] = 0x1000;
    }

    loaded = true;

    // Print ROM info
    std::cout << "ROM loaded: " << filepath << std::endl;
    std::cout << "  PRG ROM: " << (prg_size / 1024) << " KB" << std::endl;
    std::cout << "  CHR ROM: " << (chr_size / 1024) << " KB";
    if (chr_size == 0) std::cout << " (using CHR RAM)";
    std::cout << std::endl;
    std::cout << "  Mapper: " << (int)mapper << std::endl;
    std::cout << "  Mirroring: ";
    switch (mirroring) {
        case Mirroring::HORIZONTAL:   std::cout << "Horizontal"; break;
        case Mirroring::VERTICAL:     std::cout << "Vertical"; break;
        case Mirroring::FOUR_SCREEN:  std::cout << "Four-screen"; break;
        case Mirroring::SINGLE_LOWER: std::cout << "Single (lower)"; break;
        case Mirroring::SINGLE_UPPER: std::cout << "Single (upper)"; break;
    }
    std::cout << std::endl;
    std::cout << "  Battery: " << (battery ? "Yes" : "No") << std::endl;

    return true;
}

bool Cartridge::parseHeader(const INESHeader& header)
{
    // Check magic number "NES\x1A"
    if (header.magic[0] != 'N' ||
        header.magic[1] != 'E' ||
        header.magic[2] != 'S' ||
        header.magic[3] != 0x1A) {
        std::cerr << "Error: Invalid iNES header" << std::endl;
        return false;
    }

    // Extract mapper number (lower 4 bits from flags6, upper 4 bits from flags7)
    mapper = (header.flags6 >> 4) | (header.flags7 & 0xF0);

    // Extract mirroring mode
    if (header.flags6 & 0x08) {
        mirroring = Mirroring::FOUR_SCREEN;
    } else if (header.flags6 & 0x01) {
        mirroring = Mirroring::VERTICAL;
    } else {
        mirroring = Mirroring::HORIZONTAL;
    }

    // Battery-backed RAM
    battery = (header.flags6 & 0x02) != 0;

    // Check for NES 2.0 format (we only support iNES 1.0 for simplicity)
    if ((header.flags7 & 0x0C) == 0x08) {
        std::cerr << "Warning: NES 2.0 format detected, treating as iNES 1.0" << std::endl;
    }

    return true;
}

uint8_t Cartridge::readPrg(uint16_t addr) const
{
    // PRG RAM at $6000-$7FFF
    if (addr >= 0x6000 && addr < 0x8000) {
        return prg_ram[addr - 0x6000];
    }
    
    // PRG ROM at $8000-$FFFF
    if (addr >= 0x8000) {
        if (prg_rom.empty()) return 0;
        
        if (mapper == 1) {
            // MMC1: use computed bank offsets
            if (addr < 0xC000) {
                return prg_rom[prg_bank_offset[0] + (addr - 0x8000)];
            } else {
                return prg_rom[prg_bank_offset[1] + (addr - 0xC000)];
            }
        } else {
            // Mapper 0: simple mirroring
            uint32_t mapped_addr = (addr - 0x8000) % prg_rom.size();
            return prg_rom[mapped_addr];
        }
    }
    
    return 0;
}

void Cartridge::writePrg(uint16_t addr, uint8_t data)
{
    // PRG RAM at $6000-$7FFF
    if (addr >= 0x6000 && addr < 0x8000) {
        prg_ram[addr - 0x6000] = data;
        return;
    }
    
    // Mapper register writes at $8000-$FFFF
    if (addr >= 0x8000) {
        if (mapper == 1) {
            mmc1Write(addr, data);
        }
        // Mapper 0 has no writable registers
    }
}

uint8_t Cartridge::readChr(uint16_t addr) const
{
    if (chr_rom.empty()) return 0;

    if (mapper == 1) {
        // MMC1: use computed bank offsets (two 4KB banks)
        if (addr < 0x1000) {
            return chr_rom[(chr_bank_offset[0] + addr) % chr_rom.size()];
        } else {
            return chr_rom[(chr_bank_offset[1] + (addr - 0x1000)) % chr_rom.size()];
        }
    } else {
        // Mapper 0: direct access
        uint32_t mapped_addr = addr % chr_rom.size();
        return chr_rom[mapped_addr];
    }
}

void Cartridge::writeChr(uint16_t addr, uint8_t data)
{
    // Only writable if using CHR RAM
    if (!chr_rom.empty()) {
        if (mapper == 1) {
            // MMC1: use computed bank offsets
            uint32_t mapped_addr;
            if (addr < 0x1000) {
                mapped_addr = (chr_bank_offset[0] + addr) % chr_rom.size();
            } else {
                mapped_addr = (chr_bank_offset[1] + (addr - 0x1000)) % chr_rom.size();
            }
            chr_rom[mapped_addr] = data;
        } else {
            // Mapper 0: direct access
            uint32_t mapped_addr = addr % chr_rom.size();
            chr_rom[mapped_addr] = data;
        }
    }
}

// MMC1 (Mapper 1) Implementation
void Cartridge::mmc1Write(uint16_t addr, uint8_t data)
{
    // Bit 7 set = reset shift register
    if (data & 0x80) {
        mmc1_shift = 0x10;
        mmc1_shift_count = 0;
        mmc1_ctrl |= 0x0C;  // Set PRG mode to 3 (fix last bank)
        mmc1UpdateBanks();
        return;
    }
    
    // Shift in bit 0
    mmc1_shift = (mmc1_shift >> 1) | ((data & 0x01) << 4);
    mmc1_shift_count++;
    
    // After 5 writes, transfer to internal register
    if (mmc1_shift_count == 5) {
        uint8_t value = mmc1_shift;
        
        // Select register based on address
        if (addr < 0xA000) {
            // $8000-$9FFF: Control register
            mmc1_ctrl = value;
            
            // Update mirroring based on bits 0-1
            switch (value & 0x03) {
                case 0: mirroring = Mirroring::SINGLE_LOWER; break;
                case 1: mirroring = Mirroring::SINGLE_UPPER; break;
                case 2: mirroring = Mirroring::VERTICAL; break;
                case 3: mirroring = Mirroring::HORIZONTAL; break;
            }
        }
        else if (addr < 0xC000) {
            // $A000-$BFFF: CHR bank 0
            mmc1_chr_bank0 = value;
        }
        else if (addr < 0xE000) {
            // $C000-$DFFF: CHR bank 1
            mmc1_chr_bank1 = value;
        }
        else {
            // $E000-$FFFF: PRG bank
            mmc1_prg_bank = value & 0x0F;
        }
        
        mmc1UpdateBanks();
        
        // Reset shift register
        mmc1_shift = 0x10;
        mmc1_shift_count = 0;
    }
}

void Cartridge::mmc1UpdateBanks()
{
    uint32_t prg_bank_count = prg_rom.size() / PRG_ROM_UNIT;
    uint32_t chr_bank_count = chr_rom.size() / 0x1000;  // 4KB banks
    
    // PRG ROM bank mode (bits 2-3 of control register)
    uint8_t prg_mode = (mmc1_ctrl >> 2) & 0x03;
    
    switch (prg_mode) {
        case 0:
        case 1:
            // 32KB mode: switch both banks together, ignore low bit
            {
                uint32_t bank = (mmc1_prg_bank & 0x0E) % prg_bank_count;
                prg_bank_offset[0] = bank * PRG_ROM_UNIT;
                prg_bank_offset[1] = (bank + 1) * PRG_ROM_UNIT;
            }
            break;
        case 2:
            // Fix first bank at $8000, switch $C000
            prg_bank_offset[0] = 0;
            prg_bank_offset[1] = (mmc1_prg_bank % prg_bank_count) * PRG_ROM_UNIT;
            break;
        case 3:
            // Fix last bank at $C000, switch $8000
            prg_bank_offset[0] = (mmc1_prg_bank % prg_bank_count) * PRG_ROM_UNIT;
            prg_bank_offset[1] = (prg_bank_count - 1) * PRG_ROM_UNIT;
            break;
    }
    
    // CHR ROM bank mode (bit 4 of control register)
    if (chr_bank_count == 0) return;  // No CHR to bank
    
    if (mmc1_ctrl & 0x10) {
        // 4KB mode: switch two separate 4KB banks
        chr_bank_offset[0] = (mmc1_chr_bank0 % chr_bank_count) * 0x1000;
        chr_bank_offset[1] = (mmc1_chr_bank1 % chr_bank_count) * 0x1000;
    } else {
        // 8KB mode: switch 8KB at a time, ignore low bit of bank 0
        uint32_t bank = (mmc1_chr_bank0 & 0x1E) % chr_bank_count;
        chr_bank_offset[0] = bank * 0x1000;
        chr_bank_offset[1] = (bank + 1) * 0x1000;
    }
}
