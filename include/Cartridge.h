#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include "Mapper_000.h"

class Cartridge {
public:
    Cartridge(const std::string& sFileName);
    ~Cartridge();

    bool ImageValid();

    // Comms with Main Bus
    bool cpuRead(uint16_t addr, uint8_t &data);
    bool cpuWrite(uint16_t addr, uint8_t data);

    // Comms with PPU Bus
    bool ppuRead(uint16_t addr, uint8_t &data);
    bool ppuWrite(uint16_t addr, uint8_t data);

private:
    bool bImageValid = false;

    uint8_t nMapperID = 0;
    uint8_t nPRGBanks = 0;
    uint8_t nCHRBanks = 0;

    std::vector<uint8_t> vPRGMemory;
    std::vector<uint8_t> vCHRMemory;

    std::shared_ptr<Mapper> pMapper;
};
