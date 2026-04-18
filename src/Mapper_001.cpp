#include "Mapper_001.h"

Mapper_001::Mapper_001(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) {
    vRAMStatic.resize(32 * 1024);
}

Mapper_001::~Mapper_001() {}

bool Mapper_001::cpuMapRead(uint16_t addr, uint32_t &mapped_addr) {
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        mapped_addr = 0xFFFFFFFF; // PRG RAM
        return true;
    }

    if (addr >= 0x8000) {
        uint8_t nPRGMode = (nControlRegister >> 2) & 0x03;
        
        if (nPRGMode == 0 || nPRGMode == 1) {
            mapped_addr = (nPRGBankSelect32 * 0x8000) + (addr & 0x7FFF);
        } else if (nPRGMode == 2) {
            if (addr >= 0x8000 && addr <= 0xBFFF) {
                mapped_addr = (addr & 0x3FFF); // Fix First Bank
            } else {
                mapped_addr = (nPRGBankSelect16Hi * 0x4000) + (addr & 0x3FFF);
            }
        } else if (nPRGMode == 3) {
            if (addr >= 0x8000 && addr <= 0xBFFF) {
                mapped_addr = (nPRGBankSelect16Lo * 0x4000) + (addr & 0x3FFF);
            } else {
                mapped_addr = ((nPRGBanks - 1) * 0x4000) + (addr & 0x3FFF); // Fix Last Bank
            }
        }
        return true;
    }
    return false;
}

bool Mapper_001::cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data) {
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        mapped_addr = 0xFFFFFFFF;
        return true;
    }

    if (addr >= 0x8000) {
        if (data & 0x80) {
            // Reset Configuration
            nLoadRegister = 0x00;
            nLoadRegisterCount = 0;
            nControlRegister = nControlRegister | 0x0C; 
        } else {
            nLoadRegister >>= 1;
            nLoadRegister |= (data & 0x01) << 4;
            nLoadRegisterCount++;

            if (nLoadRegisterCount == 5) {
                uint8_t nTargetRegister = (addr >> 13) & 0x03;
                
                if (nTargetRegister == 0) { // Control
                    nControlRegister = nLoadRegister & 0x1F;
                } else if (nTargetRegister == 1) { // CHR Bank 0
                    if (nControlRegister & 0x10) {
                        nCHRBankSelect4Lo = nLoadRegister & 0x1F;
                    } else {
                        nCHRBankSelect8 = nLoadRegister & 0x1E;
                    }
                } else if (nTargetRegister == 2) { // CHR Bank 1
                    if (nControlRegister & 0x10) {
                        nCHRBankSelect4Hi = nLoadRegister & 0x1F;
                    }
                } else if (nTargetRegister == 3) { // PRG Bank
                    uint8_t nPRGMode = (nControlRegister >> 2) & 0x03;
                    if (nPRGMode == 0 || nPRGMode == 1) {
                        nPRGBankSelect32 = (nLoadRegister & 0x0E) >> 1;
                    } else if (nPRGMode == 2) {
                        nPRGBankSelect16Hi = nLoadRegister & 0x0F;
                    } else if (nPRGMode == 3) {
                        nPRGBankSelect16Lo = nLoadRegister & 0x0F;
                    }
                }

                nLoadRegister = 0x00;
                nLoadRegisterCount = 0;
            }
        }
        return false; // MUST RETURN FALSE SO CARTRIDGE DOES NOT WRITE DIRECTLY TO PRG ROM.
    }
    return false;
}

bool Mapper_001::ppuMapRead(uint16_t addr, uint32_t &mapped_addr) {
    if (addr < 0x2000) {
        if (nCHRBanks == 0) {
            mapped_addr = addr;
            return true;
        } else {
            if (nControlRegister & 0x10) {
                // 4K CHR Bank Mode
                if (addr >= 0x0000 && addr <= 0x0FFF) {
                    mapped_addr = (nCHRBankSelect4Lo * 0x1000) + (addr & 0x0FFF);
                } else if (addr >= 0x1000 && addr <= 0x1FFF) {
                    mapped_addr = (nCHRBankSelect4Hi * 0x1000) + (addr & 0x0FFF);
                }
            } else {
                // 8K CHR Bank Mode
                mapped_addr = (nCHRBankSelect8 * 0x1000) + (addr & 0x1FFF);
            }
            return true;
        }
    }
    return false;
}

bool Mapper_001::ppuMapWrite(uint16_t addr, uint32_t &mapped_addr) {
    if (addr < 0x2000) {
        if (nCHRBanks == 0) {
            // Treat as RAM
            mapped_addr = addr;
            return true;
        }
        return false;
    }
    return false;
}
