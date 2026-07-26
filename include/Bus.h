#pragma once
#include <cstdint>
#include <array>
#include <string>
#include <memory>
#include <atomic>
#include "PPU.h"
#include "CPU.h"
#include "APU.h"
#include "Cartridge.h"

class Bus {
public:
    Bus();
    ~Bus();

    PPU ppu;
    CPU cpu;
    APU apu;

    static const int AUDIO_BUFFER_SIZE = 8192;
    double audio_buffer[AUDIO_BUFFER_SIZE] = {0};
    std::atomic<int> audio_write_pos{0};
    std::atomic<int> audio_read_pos{0};
    uint32_t audio_sample_counter = 0;

    // Bus Read & Write
    void write(uint16_t addr, uint8_t data);
    uint8_t read(uint16_t addr, bool bReadOnly = false);

    // Load ROM
    void insertCartridge(const std::shared_ptr<Cartridge>& cartridge);

    // Hardware Controls
    void reset();
    void clock();

    // Controllers
    uint8_t controller[2] = {0, 0};

private:
    uint8_t controller_state[2];

    std::shared_ptr<Cartridge> cart;
    std::array<uint8_t, 64 * 1024> ram;

    // System Sync
    uint32_t nSystemClockCounter = 0;

    // DMA
    uint8_t dma_page = 0x00;
    uint8_t dma_addr = 0x00;
    uint8_t dma_data = 0x00;
    bool dma_dummy = true;
    bool dma_transfer = false;
};
