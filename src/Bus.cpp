#include "Bus.h"
#include <fstream>
#include <iostream>

Bus::Bus() {
    ram.fill(0x00);
    cpu.ConnectBus(this);
    // DMC needs to read from CPU address space for sample playback
    apu.dmcRead = [this](uint16_t addr) -> uint8_t {
        return this->read(addr, false);
    };
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
        // Oh god, a DMA transfer. Hide your children, hide your wives.
        // It literally stops the entire CPU just to blast 256 bytes into the PPU.
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
    apu.reset();
    nSystemClockCounter = 0;
    audio_sample_counter = 0;
    audio_write_pos.store(0);
    audio_read_pos.store(0);
    
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
            // The DMA hijack. 
            // Why the hell are we waiting for an EVEN clock cycle just to start loading?
            // Because the spec told me to. I am dead inside.
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

        // APU clocks every CPU cycle (internal divider handles CPU/2 for timers)
        apu.clock();
    }

    if (ppu.nmi) {
        ppu.nmi = false;
        cpu.nmi();
    }

    // Audio sample generation (integer counter — zero drift)
    audio_sample_counter += 44100;
    if (audio_sample_counter >= 5369318) {
        audio_sample_counter -= 5369318;
        double sample = apu.GetOutputSample();
        int wp = audio_write_pos.load(std::memory_order_relaxed);
        int next = (wp + 1) % AUDIO_BUFFER_SIZE;
        if (next != audio_read_pos.load(std::memory_order_acquire)) {
            audio_buffer[wp] = sample;
            audio_write_pos.store(next, std::memory_order_release);
        }
    }

    nSystemClockCounter++;
}
