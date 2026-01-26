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
{
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
        case Mirroring::HORIZONTAL: std::cout << "Horizontal"; break;
        case Mirroring::VERTICAL:   std::cout << "Vertical"; break;
        case Mirroring::FOUR_SCREEN: std::cout << "Four-screen"; break;
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
    if (prg_rom.empty()) return 0;

    // Simple mapper 0 behavior: mirror if needed
    uint32_t mapped_addr = addr % prg_rom.size();
    return prg_rom[mapped_addr];
}

uint8_t Cartridge::readChr(uint16_t addr) const
{
    if (chr_rom.empty()) return 0;

    uint32_t mapped_addr = addr % chr_rom.size();
    return chr_rom[mapped_addr];
}

void Cartridge::writeChr(uint16_t addr, uint8_t data)
{
    // Only writable if using CHR RAM
    if (!chr_rom.empty()) {
        uint32_t mapped_addr = addr % chr_rom.size();
        chr_rom[mapped_addr] = data;
    }
}
