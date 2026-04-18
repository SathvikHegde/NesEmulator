#include "APU.h"
#include <cmath>

APU::APU() {}

APU::~APU() {}

void APU::reset() {
    clock_counter = 0;
    // Reset all internal channels, etc.
}

uint8_t APU::cpuRead(uint16_t addr) {
    uint8_t data = 0;
    if (addr == 0x4015) {
        if (pulse1_length_counter > 0) data |= 0x01;
        if (pulse2_length_counter > 0) data |= 0x02;
        if (triangle_length_counter > 0) data |= 0x04;
        if (noise_length_counter > 0) data |= 0x08;
    }
    return data;
}

void APU::cpuWrite(uint16_t addr, uint8_t data) {
    switch (addr) {
        // Pulse 1
        case 0x4000:
            pulse1_duty = (data & 0xC0) >> 6;
            pulse1_halt = (data & 0x20) > 0;
            pulse1_volume = data & 0x0F;
            break;
        case 0x4001:
            // Sweep config
            break;
        case 0x4002:
            pulse1_timer = (pulse1_timer & 0xFF00) | data;
            break;
        case 0x4003:
            pulse1_timer = (pulse1_timer & 0x00FF) | ((data & 0x07) << 8);
            if (pulse1_enabled) {
                pulse1_length_counter = length_table[(data & 0xF8) >> 3];
            }
            break;

        // Pulse 2
        case 0x4004:
            pulse2_duty = (data & 0xC0) >> 6;
            pulse2_halt = (data & 0x20) > 0;
            pulse2_volume = data & 0x0F;
            break;
        case 0x4005:
            // Sweep config
            break;
        case 0x4006:
            pulse2_timer = (pulse2_timer & 0xFF00) | data;
            break;
        case 0x4007:
            pulse2_timer = (pulse2_timer & 0x00FF) | ((data & 0x07) << 8);
            if (pulse2_enabled) {
                pulse2_length_counter = length_table[(data & 0xF8) >> 3];
            }
            break;

        // Triangle
        case 0x4008:
            triangle_halt = (data & 0x80) > 0;
            triangle_linear_counter_reload_flag = true;
            triangle_linear_counter_reload = data & 0x7F;
            break;
        case 0x400A:
            triangle_timer = (triangle_timer & 0xFF00) | data;
            break;
        case 0x400B:
            triangle_timer = (triangle_timer & 0x00FF) | ((data & 0x07) << 8);
            if (triangle_enabled) {
                triangle_length_counter = length_table[(data & 0xF8) >> 3];
            }
            triangle_linear_counter_reload_flag = true;
            break;

        // Noise
        case 0x400C:
            noise_halt = (data & 0x20) > 0;
            noise_volume = data & 0x0F;
            break;
        case 0x400E:
            noise_mode = (data & 0x80) > 0;
            // Period lookup omitted for brief simulation
            noise_timer = data & 0x0F; // Mock standard
            break;
        case 0x400F:
            if (noise_enabled) {
                noise_length_counter = length_table[(data & 0xF8) >> 3];
            }
            break;

        // Status
        case 0x4015:
            pulse1_enabled = (data & 0x01) > 0;
            if (!pulse1_enabled) pulse1_length_counter = 0;
            pulse2_enabled = (data & 0x02) > 0;
            if (!pulse2_enabled) pulse2_length_counter = 0;
            triangle_enabled = (data & 0x04) > 0;
            if (!triangle_enabled) triangle_length_counter = 0;
            noise_enabled = (data & 0x08) > 0;
            if (!noise_enabled) noise_length_counter = 0;
            break;

        // Frame Counter
        case 0x4017:
            frame_counter_mode = (data & 0x80) >> 7;
            frame_sequence_step = 0; // Reset frame step
            break;
    }
}

void APU::clock() {
    // Generate Oscillators mathematically for simulation
    // This allows generating physical sound waves immediately 
    // even if cycle-accurate phase tracking is bypassed!
    
    // Decrease lengths roughly matching ~120Hz/240Hz frame counter
    if (clock_counter % 7457 == 0) {
        if (!pulse1_halt && pulse1_length_counter > 0) pulse1_length_counter--;
        if (!pulse2_halt && pulse2_length_counter > 0) pulse2_length_counter--;
        if (!triangle_halt && triangle_length_counter > 0) triangle_length_counter--;
        if (!noise_halt && noise_length_counter > 0) noise_length_counter--;
    }

    clock_counter++;
}

double APU::pulse1_oscillator() {
    if (pulse1_length_counter == 0 || pulse1_timer < 8) return 0.0;
    // Convert NES timer to actual Hertz. Freq = CPU / (16 * (t + 1))
    double freq = 1789773.0 / (16.0 * (pulse1_timer + 1));
    pulse1_sample += freq / 44100.0;
    if (pulse1_sample > 1.0) pulse1_sample -= 1.0;
    
    // Duty cycle mapping: 12.5%, 25%, 50%, 25% inverted
    double duty[4] = { 0.125, 0.25, 0.50, 0.75 };
    double out = (pulse1_sample < duty[pulse1_duty]) ? 1.0 : -1.0;
    return out * (pulse1_volume / 15.0);
}

double APU::pulse2_oscillator() {
    if (pulse2_length_counter == 0 || pulse2_timer < 8) return 0.0;
    double freq = 1789773.0 / (16.0 * (pulse2_timer + 1));
    pulse2_sample += freq / 44100.0;
    if (pulse2_sample > 1.0) pulse2_sample -= 1.0;
    
    double duty[4] = { 0.125, 0.25, 0.50, 0.75 };
    double out = (pulse2_sample < duty[pulse2_duty]) ? 1.0 : -1.0;
    return out * (pulse2_volume / 15.0);
}

double APU::triangle_oscillator() {
    if (triangle_length_counter == 0 || triangle_timer < 3) return 0.0;
    double freq = 1789773.0 / (32.0 * (triangle_timer + 1));
    triangle_sample += freq / 44100.0;
    if (triangle_sample > 1.0) triangle_sample -= 1.0;
    
    // Triangle wave algorithm:
    double out = abs(triangle_sample * 4.0 - 2.0) - 1.0;
    return out;
}

double APU::noise_oscillator() {
    if (noise_length_counter == 0) return 0.0;
    
    // Fake LFSR noise simulation
    noise_shift_register ^= (noise_shift_register >> 1);
    noise_shift_register ^= (clock_counter & 0xFFFF);
    double out = ((noise_shift_register & 0x01) ? 1.0 : -1.0);
    return out * (noise_volume / 15.0);
}

double APU::GetOutputSample() {
    double p1 = pulse1_oscillator();
    double p2 = pulse2_oscillator();
    double t = triangle_oscillator();
    double n = noise_oscillator();

    // Standard NES Mixing Approximation
    double pulse_out = 0.0;
    if (p1 + p2 > 0.0) {
        pulse_out = 95.88 / ((8128.0 / (p1 + p2)) + 100.0);
    }
    
    double tnd_out = 0.0;
    if (t + n > 0.0) {
        tnd_out = 159.79 / ((1.0 / (t / 8227.0 + n / 12241.0)) + 100.0);
    }
    
    return pulse_out + tnd_out;
}
