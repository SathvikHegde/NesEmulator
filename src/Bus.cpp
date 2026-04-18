#include "Bus.h"
#include <fstream>
#include <iostream>

Bus::Bus() {
    ram.fill(0x00);
    cpu.ConnectBus(this);
}

Bus::~Bus() {}

void Bus::write(uint16_t addr, uint8_t data) {
    if (cart->cpuWrite(addr, data)) {
        // cartridge handled it
    } else if (addr >= 0x0000 && addr <= 0x1FFF) {
        ram[addr & 0x07FF] = data; // Mirror 2KB RAM
    } else if (addr >= 0x2000 && addr <= 0x3FFF) {
        ppu.cpuWrite(addr & 0x0007, data); // Map to 8 PPU registers
    } else if (addr == 0x4014) {
        dma_page = data;
        dma_addr = 0x00;
        dma_transfer = true;
    } else if (addr >= 0x4000 && addr <= 0x4013) {
        apu.cpuWrite(addr, data);
    } else if (addr == 0x4015) {
        apu.cpuWrite(addr, data);
    } else if (addr == 0x4017) {
        apu.cpuWrite(addr, data);
    } else if (addr == 0x4016) {
        controller_state[0] = controller[0];
        controller_state[1] = controller[1];
    }
}

uint8_t Bus::read(uint16_t addr, bool bReadOnly) {
    uint8_t data = 0x00;
    if (cart->cpuRead(addr, data)) {
        // Cartridge intercepted standard mapped data
    } else if (addr >= 0x0000 && addr <= 0x1FFF) {
        data = ram[addr & 0x07FF];
    } else if (addr >= 0x2000 && addr <= 0x3FFF) {
        data = ppu.cpuRead(addr & 0x0007, bReadOnly);
    } else if (addr == 0x4015) {
        data = apu.cpuRead(addr);
    } else if (addr >= 0x4016 && addr <= 0x4017) {
        data = (controller_state[addr & 0x0001] & 0x80) > 0;
        controller_state[addr & 0x0001] <<= 1;
    }
    return data;
}

void Bus::insertCartridge(const std::shared_ptr<Cartridge>& cartridge) {
    this->cart = cartridge;
    ppu.ConnectCartridge(cartridge);
}

void Bus::reset() {
    cpu.reset();
    nSystemClockCounter = 0;
    
    dma_page = 0x00;
    dma_addr = 0x00;
    dma_data = 0x00;
    dma_dummy = true;
    dma_transfer = false;
}

void Bus::clock() {
    ppu.clock();

    if (nSystemClockCounter % 3 == 0) {
        if (dma_transfer) {
            if (dma_dummy) {
                if (nSystemClockCounter % 2 == 1) {
                    dma_dummy = false;
                }
            } else {
                if (nSystemClockCounter % 2 == 0) {
                    dma_data = read((dma_page << 8) | dma_addr);
                } else {
                    ppu.pOAM[dma_addr] = dma_data;
                    dma_addr++;
                    if (dma_addr == 0x00) {
                        dma_transfer = false;
                        dma_dummy = true;
                    }
                }
            }
        } else {
            cpu.clock();
        }
    }

    if (ppu.nmi) {
        ppu.nmi = false;
        cpu.nmi();
    }

    // Standard APU execution clock is tied functionally to CPU cycle / 2.
    if (nSystemClockCounter % 6 == 0) {
        apu.clock();
    }

    // Audio Synchronization
    bool bAudioSampleReady = false;
    audio_time += (1.0 / 5369318.0); // Exact generic master clock speed
    if (audio_time >= (1.0 / 44100.0)) {
        audio_time -= (1.0 / 44100.0);
        bAudioSampleReady = true;
    }

    if (bAudioSampleReady) {
        std::lock_guard<std::mutex> lock(audio_mutex);
        audio_samples.push(apu.GetOutputSample());
    }

    nSystemClockCounter++;
}
