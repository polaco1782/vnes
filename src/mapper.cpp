#include "mapper.h"
#include "mapper_000.h"
#include "mapper_001.h"
#include "mapper_002.h"
#include "mapper_004.h"
#include "mapper_009.h"

Mapper::Mapper(u8 mapperNumber)
    : mapperNum(mapperNumber)
    , mirroring(Mirroring::HORIZONTAL)
    , prgRom(nullptr)
    , chrRom(nullptr)
    , prgRam(nullptr)
{
}

void Mapper::init(std::vector<u8>& prg, std::vector<u8>& chr,
                  std::vector<u8>& ram, Mirroring initialMirroring)
{
    prgRom = &prg;
    chrRom = &chr;
    prgRam = &ram;
    mirroring = initialMirroring;
}

std::unique_ptr<Mapper> MapperFactory::create(u8 mapperNumber)
{
    switch (mapperNumber) {
        case 0:
            return std::make_unique<Mapper000>();
        case 1:
            return std::make_unique<Mapper001>();
        case 2:
            return std::make_unique<Mapper002>();
        case 4:
            return std::make_unique<Mapper004>();
        case 9:
            return std::make_unique<Mapper009>();
        default:
            std::cerr << "Warning: Unsupported mapper " << (int)mapperNumber 
                      << ", falling back to mapper 0" << std::endl;

            auto m = std::make_unique<Mapper000>();
            m->unsupported = true; // Mark as unsupported for special handling

            return m;
    }
}
