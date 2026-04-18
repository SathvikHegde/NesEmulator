#include "Cartridge.h"
#include "Mapper_000.h"
#include "Mapper_001.h"
#include <fstream>
#include <iostream>

Cartridge::Cartridge(const std::string& sFileName) {
    // iNES Format Header
    struct sHeader {
        char name[4];
        uint8_t prg_rom_chunks;
        uint8_t chr_rom_chunks;
        uint8_t mapper1;
        uint8_t mapper2;
        uint8_t prg_ram_size;
        uint8_t tv_system1;
        uint8_t tv_system2;
        char unused[5];
    } header;

    bImageValid = false;
    std::ifstream ifs(sFileName, std::ios::binary);
    if (!ifs.is_open()) {
        std::cerr << "Cartridge: Cannot open file " << sFileName << std::endl;
        return;
    }

    ifs.read((char*)&header, sizeof(sHeader));
    if (header.name[0] != 'N' || header.name[1] != 'E' || header.name[2] != 'S' || header.name[3] != '\x1A') {
        std::cerr << "Cartridge: Invalid iNES file!" << std::endl;
        return;
    }

    if (header.mapper1 & 0x04) ifs.seekg(512, std::ios_base::cur); // Skip Trainer

    nMapperID = ((header.mapper2 >> 4) << 4) | (header.mapper1 >> 4);
    
    // File type 1 parsing
    nPRGBanks = header.prg_rom_chunks;
    vPRGMemory.resize(nPRGBanks * 16384);
    ifs.read((char*)vPRGMemory.data(), vPRGMemory.size());

    nCHRBanks = header.chr_rom_chunks;
    if (nCHRBanks == 0) {
        // Create CHR RAM
        vCHRMemory.resize(8192);
    } else {
        // Allocate for CHR ROM
        vCHRMemory.resize(nCHRBanks * 8192);
    }
    ifs.read((char*)vCHRMemory.data(), vCHRMemory.size());

    ifs.close();

    // Map the proper hardware
    switch (nMapperID) {
        case 0: pMapper = std::make_shared<Mapper_000>(nPRGBanks, nCHRBanks); break;
        case 1: pMapper = std::make_shared<Mapper_001>(nPRGBanks, nCHRBanks); break;
        default: std::cerr << "Cartridge: Mapper " << (int)nMapperID << " not supported!" << std::endl; return;
    }

    bImageValid = true;
}

Cartridge::~Cartridge() { }

bool Cartridge::ImageValid() { return bImageValid; }

bool Cartridge::cpuRead(uint16_t addr, uint8_t &data) {
    uint32_t mapped_addr = 0;
    if (pMapper->cpuMapRead(addr, mapped_addr)) {
        if (mapped_addr == 0xFFFFFFFF) {
            // Read from RAM
            data = 0x00; // Static RAM ignoring for this phase (Zelda/etc usually use it). 
            return true;
        }
        data = vPRGMemory[mapped_addr];
        return true;
    }
    return false;
}

bool Cartridge::cpuWrite(uint16_t addr, uint8_t data) {
    uint32_t mapped_addr = 0;
    if (pMapper->cpuMapWrite(addr, mapped_addr, data)) {
        if (mapped_addr == 0xFFFFFFFF) {
            // PRG RAM write
            return true;
        }
        vPRGMemory[mapped_addr] = data; // Usually PRG is ROM, but written for completeness or RAM mappers
        return true;
    }
    return false;
}

bool Cartridge::ppuRead(uint16_t addr, uint8_t &data) {
    uint32_t mapped_addr = 0;
    if (pMapper->ppuMapRead(addr, mapped_addr)) {
        data = vCHRMemory[mapped_addr];
        return true;
    }
    return false;
}

bool Cartridge::ppuWrite(uint16_t addr, uint8_t data) {
    uint32_t mapped_addr = 0;
    if (pMapper->ppuMapWrite(addr, mapped_addr)) {
        vCHRMemory[mapped_addr] = data;
        return true;
    }
    return false;
}
