#include "cartridge.h"
#include <fstream>
#include <iostream>

Cartridge::Cartridge()
    : loaded(false)
    , mapperNumber(0)
    , battery(false)
    , mapper(nullptr)
    , initialMirroring(Mirroring::HORIZONTAL)
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
    prg_ram.clear();
    mapper.reset();

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
    
    // Create and initialize the mapper
    mapper = MapperFactory::create(mapperNumber);
    mapper->init(prg_rom, chr_rom, prg_ram, initialMirroring);

    loaded = true;

    // Print ROM info
    std::cout << "ROM loaded: " << filepath << std::endl;
    std::cout << "  PRG ROM: " << (prg_size / 1024) << " KB" << std::endl;
    std::cout << "  CHR ROM: " << (chr_size / 1024) << " KB";
    if (chr_size == 0) std::cout << " (using CHR RAM)";
    std::cout << std::endl;
    std::cout << "  Mapper: " << (int)mapperNumber << " (" << mapper->getName() << ")" << std::endl;
    std::cout << "  Mirroring: ";
    switch (initialMirroring) {
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
    mapperNumber = (header.flags6 >> 4) | (header.flags7 & 0xF0);

    // Extract mirroring mode
    if (header.flags6 & 0x08) {
        initialMirroring = Mirroring::FOUR_SCREEN;
    } else if (header.flags6 & 0x01) {
        initialMirroring = Mirroring::VERTICAL;
    } else {
        initialMirroring = Mirroring::HORIZONTAL;
    }

    // Battery-backed RAM
    battery = (header.flags6 & 0x02) != 0;

    // Check for NES 2.0 format (we only support iNES 1.0 for simplicity)
    if ((header.flags7 & 0x0C) == 0x08) {
        std::cerr << "Warning: NES 2.0 format detected, treating as iNES 1.0" << std::endl;
    }

    return true;
}

Mirroring Cartridge::getMirroring() const
{
    if (mapper) {
        return mapper->getMirroring();
    }
    return initialMirroring;
}

const char* Cartridge::getMapperName() const
{
    if (mapper) {
        return mapper->getName();
    }
    return "Unknown";
}

uint8_t Cartridge::readPrg(uint16_t addr) const
{
    if (mapper) {
        return mapper->readPrg(addr);
    }
    return 0;
}

void Cartridge::writePrg(uint16_t addr, uint8_t data)
{
    if (mapper) {
        mapper->writePrg(addr, data);
    }
}

uint8_t Cartridge::readChr(uint16_t addr) const
{
    if (mapper) {
        return mapper->readChr(addr);
    }
    return 0;
}

void Cartridge::writeChr(uint16_t addr, uint8_t data)
{
    if (mapper) {
        mapper->writeChr(addr, data);
    }
}

void Cartridge::signalScanline()
{
    if (mapper) {
        mapper->scanline();
    }
}
