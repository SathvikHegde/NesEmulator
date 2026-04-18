#pragma once
#include <cstdint>

class APU {
public:
    APU();
    ~APU();

    void cpuWrite(uint16_t addr, uint8_t data);
    uint8_t cpuRead(uint16_t addr);

    void clock();
    void reset();

    // The mixed internal audio amplitude ranging typically from -1.0 to 1.0. 
    double GetOutputSample();

private:
    uint32_t clock_counter = 0;

    // Registers and Internal States for Channels
    // ----------------------------------------
    // Pulse 1
    uint8_t pulse1_duty = 0;
    uint8_t pulse1_volume = 0;
    uint16_t pulse1_timer = 0;
    uint8_t pulse1_length_counter = 0;
    bool pulse1_halt = false;
    bool pulse1_enabled = false;
    uint16_t pulse1_sequence = 0; // Duty cycle sequence tracker
    double pulse1_sample = 0.0;

    // Pulse 2
    uint8_t pulse2_duty = 0;
    uint8_t pulse2_volume = 0;
    uint16_t pulse2_timer = 0;
    uint8_t pulse2_length_counter = 0;
    bool pulse2_halt = false;
    bool pulse2_enabled = false;
    uint16_t pulse2_sequence = 0;
    double pulse2_sample = 0.0;

    // Triangle
    uint16_t triangle_timer = 0;
    uint8_t triangle_length_counter = 0;
    uint8_t triangle_linear_counter = 0;
    uint8_t triangle_linear_counter_reload = 0;
    bool triangle_linear_counter_reload_flag = false;
    bool triangle_halt = false;
    bool triangle_enabled = false;
    uint8_t triangle_sequence = 0;
    double triangle_sample = 0.0;

    // Noise
    uint16_t noise_timer = 0;
    uint8_t noise_length_counter = 0;
    uint8_t noise_volume = 0;
    uint16_t noise_shift_register = 1;
    bool noise_halt = false;
    bool noise_enabled = false;
    bool noise_mode = false;
    double noise_sample = 0.0;
    
    // Status & APU global Frame Counter
    uint8_t frame_counter_mode = 0;
    uint16_t frame_sequence_step = 0; // Internal APU cycle steps counting roughly to 14915.

    // Hardware lookups
    const uint8_t length_table[32] = {
        10, 254, 20,  2, 40,  4, 80,  6, 160,  8, 60, 10, 14, 12, 26, 14,
        12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
    };

    // Helper functions for oscillators natively matching time loops
    double pulse1_oscillator();
    double pulse2_oscillator();
    double triangle_oscillator();
    double noise_oscillator();
};
