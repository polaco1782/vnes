#include "mapper.h"
#include "mapper_000.h"
#include "mapper_001.h"
#include "mapper_004.h"
#include "mapper_009.h"
#include <iostream>

Mapper::Mapper(uint8_t mapperNumber)
    : mapperNum(mapperNumber)
    , mirroring(Mirroring::HORIZONTAL)
    , prgRom(nullptr)
    , chrRom(nullptr)
    , prgRam(nullptr)
{
}

void Mapper::init(std::vector<uint8_t>& prg, std::vector<uint8_t>& chr,
                  std::vector<uint8_t>& ram, Mirroring initialMirroring)
{
    prgRom = &prg;
    chrRom = &chr;
    prgRam = &ram;
    mirroring = initialMirroring;
}

std::unique_ptr<Mapper> MapperFactory::create(uint8_t mapperNumber)
{
    switch (mapperNumber) {
        case 0:
            return std::unique_ptr<Mapper>(new Mapper000());
        case 1:
            return std::unique_ptr<Mapper>(new Mapper001());
        case 4:
            return std::unique_ptr<Mapper>(new Mapper004());
        case 9:
            return std::unique_ptr<Mapper>(new Mapper009());
        default:
            std::cerr << "Warning: Unsupported mapper " << (int)mapperNumber 
                      << ", falling back to mapper 0" << std::endl;
            return std::unique_ptr<Mapper>(new Mapper000());
    }
}
