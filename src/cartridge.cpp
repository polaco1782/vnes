#include "cartridge.h"
#include <fstream>
#include <iostream>

#include <charconv>
#include <algorithm>
#include "util.h"

using vnes::util::toHex;
using vnes::util::hexByte;
using vnes::util::hexWord;

Cartridge::Cartridge()
    : loaded(false)
    , mapperNumber(0)
    , battery(false)
    , mapper(nullptr)
    , initialMirroring(Mirroring::HORIZONTAL)
    , prgRamDirty(false)
    , framesSinceLastSave(0)
    , gg_count(0)
{
    gg_count = 0;
    for (size_t i = 0; i < gg_active_entries.size(); ++i) gg_active_entries[i] = GGActiveEntry();
}

bool Cartridge::addGGCode(const std::string& code)
{
    // Decode Game Genie codes. Accept codes in the Game Genie alphabet
    // (A P Z L G I T Y E O X U K S V N) with or without hyphens.
    // Map each letter to a 4-bit nibble, then pack n into address/value.

    static const std::string ggAlphabet = "APZLGITYEOXUKSVN";

    // Normalize: remove non-alpha characters and uppercase
    std::string s;
    for (char c : code) {
        if (std::isalpha(static_cast<unsigned char>(c))) s += (char)std::toupper(static_cast<unsigned char>(c));
    }

    if (s.empty())
		return false;

    // Only support 6- or 8-character Game Genie codes here. Other lengths are ignored.
    if (s.length() != 6 && s.length() != 8)
		return false;

    // Convert characters to nibbles
    std::vector<u8> n;
    n.reserve(s.length());
    for (char c : s) {
        size_t pos = ggAlphabet.find(c);
        if (pos == std::string::npos) {
            // Invalid character found -> not a valid Game Genie code
			return false;
        }
        n.push_back((u8)pos);
    }

    // Pack: first 4 n = 16-bit address (low nibble first), next 2 n = 8-bit value
    // For 8-char codes we ignore any trailing nibble(s) beyond the first 6 for now.
    u16 addr = 0;
    u8 value = 0;
	u8 compare = 0;

	addr = 0x8000 + ((n[3] & 7) << 12) | ((n[5] & 7) << 8) | ((n[4] & 8) << 8) |
		            ((n[2] & 7) << 4) | ((n[1] & 8) << 4) | (n[4] & 7) | (n[3] & 8);

	if (s.length() == 6) {
		value = ((n[1] & 7) << 4) | ((n[0] & 8) << 4) | (n[0] & 7) | (n[5] & 8);
	}
	else {
		value = ((n[1] & 7) << 4) | ((n[0] & 8) << 4) | (n[0] & 7) | (n[7] & 8);
		compare = ((n[7] & 7) << 4) | ((n[6] & 8) << 4) | (n[6] & 7) | (n[5] & 8);
	}

    std::cout << "Adding Game Genie code: " << code << " -> Write $" << toHex(value, 2) << " to $" << toHex(addr, 4) << std::endl;

    GGActiveEntry entry;
    entry.code = code;
    entry.addr = addr;
    entry.value = value;
    entry.compare = compare;
    entry.has_compare = (s.length() == 8);

    // try overwrite existing active entry by address
    for (size_t k = 0; k < gg_count; ++k) {
        if (gg_active_entries[k].addr == addr) {
            gg_active_entries[k] = entry;
            return true;
        }
    }

    // append new active entry if space
    if (gg_count < MAX_GG_CODES) {
        gg_active_entries[gg_count] = entry;
        ++gg_count;
        return true;
    }

    // no free slot
    std::cerr << "Error: Maximum number of Game Genie codes reached (" << MAX_GG_CODES << ")" << std::endl;
    return false;
}

void Cartridge::removeGGCode(const std::string& code)
{
    for (size_t k = 0; k < gg_count; ++k) {
        if (gg_active_entries[k].code == code) {
            size_t last = gg_count - 1;
            if (k != last) gg_active_entries[k] = gg_active_entries[last];
            gg_active_entries[last] = GGActiveEntry();
            --gg_count;
            return;
        }
    }
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

    // clear active entries
    for (u32 i = 0; i < MAX_GG_CODES; ++i) {
        gg_active_entries[i] = GGActiveEntry();
    }
    gg_count = 0;

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
	u32 prg_size = header.prg_rom_size * PRG_ROM_UNIT;
	prg_rom.resize(prg_size);
	file.read(reinterpret_cast<char*>(prg_rom.data()), prg_size);
	if (!file) {
		std::cerr << "Error: Cannot read PRG ROM" << std::endl;
		return false;
	}

	// Read CHR ROM (or allocate CHR RAM if size is 0)
	u32 chr_size = header.chr_rom_size * CHR_ROM_UNIT;
	if (chr_size > 0) {
		chr_rom.resize(chr_size);
		file.read(reinterpret_cast<char*>(chr_rom.data()), chr_size);
		if (!file) {
			std::cerr << "Error: Cannot read CHR ROM" << std::endl;
			return false;
		}
	}
	else {
		// CHR RAM - allocate 8KB
		chr_rom.resize(CHR_ROM_UNIT, 0);
	}

	// Allocate 8KB PRG RAM at $6000-$7FFF
	prg_ram.resize(8192, 0);

	// Create and initialize the mapper
	mapper = MapperFactory::create(mapperNumber);
	mapper->init(prg_rom, chr_rom, prg_ram, initialMirroring);

	// Set up SRAM save path and load for battery-backed carts
	if (battery) {
		// Generate save file name from ROM path (replace .nes with .sav)
		savePath = filepath;
		size_t extPos = savePath.rfind(".nes");
		if (extPos != std::string::npos) {
			savePath.replace(extPos, 4, ".sav");
		}
		else {
			savePath += ".sav";
		}

		// Load SRAM from disk if it exists
		std::ifstream saveFile(savePath, std::ios::binary);
		if (saveFile) {
			saveFile.read(reinterpret_cast<char*>(prg_ram.data()), prg_ram.size());
			saveFile.close();
			std::cout << "  Loaded SRAM from " << savePath << std::endl;
		}
	}

	loaded = true;

	// Print ROM info
	std::cout << "ROM loaded: " << filepath << std::endl;
	std::cout << "  PRG ROM: " << (prg_size / 1024) << " KB" << std::endl;
	std::cout << "  CHR ROM: " << (chr_size / 1024) << " KB";
	if (chr_size == 0) std::cout << " (using CHR RAM)";
	std::cout << std::endl;
	std::cout << "  Mapper: " << (int)mapperNumber << " (" << getMapperName() << ")" << std::endl;
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
	}
	else if (header.flags6 & 0x01) {
		initialMirroring = Mirroring::VERTICAL;
	}
	else {
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
	return mapper->getMirroring();
}

const char* Cartridge::getMapperName() const
{
	if (!mapper->unsupported) {
		return mapper->getName();
	}

	return "UNSUPPORTED";
}

u8 Cartridge::readPrg(u16 addr) const
{
    // Fast lookup for Game Genie patches in $8000-$FFFF
    if (addr >= 0x8000) {
        if (gg_count == 0) return mapper->readPrg(addr);
        for (size_t k = 0; k < gg_count; ++k) {
            auto &e = gg_active_entries[k];
            if (e.addr == addr) {
                return e.value;
            }
        }
    }

    return mapper->readPrg(addr);
}

void Cartridge::writePrg(u16 addr, u8 data)
{
	// Check if writing to PRG RAM for battery-backed save detection
	if (battery && addr >= 0x6000 && addr < 0x8000) {
		prgRamDirty = true;
		framesSinceLastSave = 0;
	}

	mapper->writePrg(addr, data);
}

u8 Cartridge::readChr(u16 addr) const
{
	return mapper->readChr(addr);
}

void Cartridge::writeChr(u16 addr, u8 data)
{
	mapper->writeChr(addr, data);
}

void Cartridge::signalFrameComplete()
{
	if (!battery || !prgRamDirty) {
		return;
	}

	framesSinceLastSave++;

	// Save every 60 frames (~1 second at 60 FPS)
	if (framesSinceLastSave >= 60) {
		flushSRAM();
		framesSinceLastSave = 0;
	}

	scanline();  // Some mappers need frame timing
}

// call access to mapper scanline
void Cartridge::scanline() {
	mapper->scanline();
}

void Cartridge::clearIRQ() {
	mapper->clearIrq();
}

bool Cartridge::hasIRQ()
{
	if (mapper->irqPending()) {
		return true;
	}

	return false;
}

void Cartridge::flushSRAM()
{
	if (!battery || !prgRamDirty || prg_ram.empty() || savePath.empty()) {
		return;
	}

	std::ofstream saveFile(savePath, std::ios::binary);
	if (saveFile) {
		saveFile.write(reinterpret_cast<const char*>(prg_ram.data()), prg_ram.size());
		saveFile.close();
		prgRamDirty = false;
		std::cout << "SRAM saved to " << savePath << std::endl;
	}
}
