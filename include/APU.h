#pragma once
#include <cstdint>
#include <functional>

class APU {
public:
    APU();
    ~APU();

    void cpuWrite(uint16_t addr, uint8_t data);
    uint8_t cpuRead(uint16_t addr);
    void clock();
    void reset();
    double GetOutputSample();

    // DMC memory reader — set by Bus to read from CPU address space
    std::function<uint8_t(uint16_t)> dmcRead;

    // IRQ output (frame counter + DMC)
    bool irq = false;

private:
    uint32_t clock_counter = 0;

    // ---- Frame Counter ----
    uint8_t frame_mode = 0; // 0 = 4-step, 1 = 5-step
    bool frame_irq_inhibit = false;
    bool frame_irq_flag = false;
    uint16_t frame_counter = 0;
    void quarter_frame();
    void half_frame();

    // ---- Envelope (shared by Pulse and Noise) ----
    struct Envelope {
        bool start = false;
        bool loop_flag = false;
        bool constant_flag = false;
        uint8_t volume = 0;    // Also serves as divider period
        uint8_t divider = 0;
        uint8_t decay = 15;
        void clock();
        uint8_t output() const;
    };

    // ---- Sweep (Pulse only) ----
    struct Sweep {
        bool enabled = false;
        bool negate = false;
        bool reload = false;
        uint8_t period = 0;
        uint8_t divider = 0;
        uint8_t shift = 0;
        uint16_t target(uint16_t timer_period, bool ch1) const;
        bool muting(uint16_t timer_period, bool ch1) const;
        void clock(uint16_t& timer_period, bool ch1);
    };

    // ---- Pulse Channel ----
    struct Pulse {
        bool enabled = false;
        uint8_t duty = 0;
        uint8_t duty_pos = 0;
        uint16_t timer_period = 0;
        uint16_t timer = 0;
        uint8_t length = 0;
        bool halt = false;
        Envelope envelope;
        Sweep sweep;
        void clock_timer();
        void clock_length();
        uint8_t output(bool ch1) const;
    } pulse1, pulse2;

    // ---- Triangle Channel ----
    struct Triangle {
        bool enabled = false;
        uint16_t timer_period = 0;
        uint16_t timer = 0;
        uint8_t seq_pos = 0;
        uint8_t length = 0;
        bool halt = false; // Also controls linear counter reload behavior
        uint8_t linear_load = 0;
        uint8_t linear = 0;
        bool linear_reload = false;
        void clock_timer();
        void clock_length();
        void clock_linear();
        uint8_t output() const;
    } triangle;

    // ---- Noise Channel ----
    struct Noise {
        bool enabled = false;
        uint16_t timer_period = 0;
        uint16_t timer = 0;
        uint16_t lfsr = 1; // 15-bit linear feedback shift register
        bool mode = false;  // false = normal (bit 1), true = short (bit 6)
        uint8_t length = 0;
        bool halt = false;
        Envelope envelope;
        void clock_timer();
        void clock_length();
        uint8_t output() const;
    } noise;

    // ---- DMC Channel ----
    struct DMCChannel {
        bool enabled = false;
        bool irq_enable = false;
        bool irq_flag = false;
        bool loop_flag = false;
        uint16_t timer_period = 0;
        uint16_t timer = 0;
        uint8_t output_level = 0;
        uint16_t sample_addr = 0xC000;
        uint16_t sample_len = 1;
        uint16_t current_addr = 0xC000;
        uint16_t bytes_remaining = 0;
        uint8_t sample_buffer = 0;
        bool buffer_empty = true;
        uint8_t shift_reg = 0;
        uint8_t bits_remaining = 8;
        bool silence = true;
        void clock_timer(std::function<uint8_t(uint16_t)>& reader);
        void restart();
    } dmc;

    // ---- Hardware Lookup Tables ----
    static const uint8_t length_table[32];
    static const uint8_t duty_table[4][8];
    static const uint8_t triangle_table[32];
    static const uint16_t noise_table[16];
    static const uint16_t dmc_rate_table[16];
};
