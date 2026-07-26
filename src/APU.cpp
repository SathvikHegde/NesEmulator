#include "APU.h"

// ============================================================
// Hardware Lookup Tables
// ============================================================

const uint8_t APU::length_table[32] = {
    10, 254, 20,  2, 40,  4, 80,  6, 160,  8, 60, 10, 14, 12, 26, 14,
    12,  16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
};

const uint8_t APU::duty_table[4][8] = {
    {0, 0, 0, 0, 0, 0, 0, 1}, // 12.5%
    {0, 0, 0, 0, 0, 0, 1, 1}, // 25%
    {0, 0, 0, 0, 1, 1, 1, 1}, // 50%
    {1, 1, 1, 1, 1, 1, 0, 0}, // 75% (inverted 25%)
};

const uint8_t APU::triangle_table[32] = {
    15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0,
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15
};

// NTSC noise timer periods (in APU half-cycles)
const uint16_t APU::noise_table[16] = {
    4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
};

// NTSC DMC rate periods (in CPU cycles)
const uint16_t APU::dmc_rate_table[16] = {
    428, 380, 340, 320, 286, 254, 226, 214, 190, 160, 142, 128, 106, 84, 72, 54
};

// ============================================================
// Constructor / Destructor / Reset
// ============================================================

APU::APU() {}
APU::~APU() {}

void APU::reset() {
    clock_counter = 0;
    frame_mode = 0;
    frame_irq_inhibit = false;
    frame_irq_flag = false;
    frame_counter = 0;
    irq = false;

    pulse1 = {};
    pulse2 = {};
    triangle = {};
    noise = {};
    noise.lfsr = 1;
    dmc = {};
}

// ============================================================
// Envelope
// ============================================================

void APU::Envelope::clock() {
    if (start) {
        start = false;
        decay = 15;
        divider = volume;
    } else {
        if (divider == 0) {
            divider = volume;
            if (decay > 0) {
                decay--;
            } else if (loop_flag) {
                decay = 15;
            }
        } else {
            divider--;
        }
    }
}

uint8_t APU::Envelope::output() const {
    return constant_flag ? volume : decay;
}

// ============================================================
// Sweep
// ============================================================

uint16_t APU::Sweep::target(uint16_t timer_period, bool ch1) const {
    uint16_t delta = timer_period >> shift;
    if (negate) {
        // Pulse 1 uses one's complement (subtracts delta + 1)
        // Pulse 2 uses two's complement (subtracts delta)
        if (ch1)
            return timer_period - delta - 1;
        else
            return timer_period - delta;
    }
    return timer_period + delta;
}

bool APU::Sweep::muting(uint16_t timer_period, bool ch1) const {
    return timer_period < 8 || target(timer_period, ch1) > 0x7FF;
}

void APU::Sweep::clock(uint16_t& timer_period, bool ch1) {
    uint16_t tgt = target(timer_period, ch1);
    if (divider == 0 && enabled && shift > 0 &&
        timer_period >= 8 && tgt <= 0x7FF) {
        timer_period = tgt;
    }
    if (divider == 0 || reload) {
        divider = period;
        reload = false;
    } else {
        divider--;
    }
}

// ============================================================
// Pulse Channel
// ============================================================

void APU::Pulse::clock_timer() {
    if (timer == 0) {
        timer = timer_period;
        duty_pos = (duty_pos + 1) & 7;
    } else {
        timer--;
    }
}

void APU::Pulse::clock_length() {
    if (!halt && length > 0) {
        length--;
    }
}

uint8_t APU::Pulse::output(bool ch1) const {
    if (!enabled) return 0;
    if (length == 0) return 0;
    if (duty_table[duty][duty_pos] == 0) return 0;
    if (sweep.muting(timer_period, ch1)) return 0;
    return envelope.output();
}

// ============================================================
// Triangle Channel
// ============================================================

void APU::Triangle::clock_timer() {
    if (timer == 0) {
        timer = timer_period;
        // Only advance the sequencer if both counters are non-zero
        if (length > 0 && linear > 0) {
            seq_pos = (seq_pos + 1) & 31;
        }
    } else {
        timer--;
    }
}

void APU::Triangle::clock_length() {
    if (!halt && length > 0) {
        length--;
    }
}

void APU::Triangle::clock_linear() {
    if (linear_reload) {
        linear = linear_load;
    } else if (linear > 0) {
        linear--;
    }
    // If halt (control) flag is clear, clear the reload flag
    if (!halt) {
        linear_reload = false;
    }
}

uint8_t APU::Triangle::output() const {
    // Triangle always outputs its current sequencer position.
    // When silenced, the sequencer just freezes (no pop).
    return triangle_table[seq_pos];
}

// ============================================================
// Noise Channel
// ============================================================

void APU::Noise::clock_timer() {
    if (timer == 0) {
        timer = timer_period;
        // Real NES 15-bit LFSR: feedback = bit0 XOR bit1 (or bit6 in short mode)
        uint16_t feedback = ((lfsr >> 0) ^ (lfsr >> (mode ? 6 : 1))) & 1;
        lfsr = (lfsr >> 1) | (feedback << 14);
    } else {
        timer--;
    }
}

void APU::Noise::clock_length() {
    if (!halt && length > 0) {
        length--;
    }
}

uint8_t APU::Noise::output() const {
    if (!enabled) return 0;
    if (length == 0) return 0;
    if (lfsr & 1) return 0; // Bit 0 gates the output
    return envelope.output();
}

// ============================================================
// DMC Channel
// ============================================================

void APU::DMCChannel::clock_timer(std::function<uint8_t(uint16_t)>& reader) {
    // Attempt to fill the sample buffer if empty
    if (buffer_empty && bytes_remaining > 0 && reader) {
        sample_buffer = reader(current_addr);
        buffer_empty = false;
        // Address wraps from $FFFF to $8000
        if (current_addr == 0xFFFF)
            current_addr = 0x8000;
        else
            current_addr++;
        bytes_remaining--;
        if (bytes_remaining == 0) {
            if (loop_flag) {
                restart();
            } else if (irq_enable) {
                irq_flag = true;
            }
        }
    }

    // Output unit
    if (timer == 0) {
        timer = timer_period;

        if (!silence) {
            if (shift_reg & 1) {
                if (output_level <= 125) output_level += 2;
            } else {
                if (output_level >= 2) output_level -= 2;
            }
        }

        shift_reg >>= 1;
        if (bits_remaining > 0) bits_remaining--;

        if (bits_remaining == 0) {
            bits_remaining = 8;
            if (buffer_empty) {
                silence = true;
            } else {
                silence = false;
                shift_reg = sample_buffer;
                buffer_empty = true;
            }
        }
    } else {
        timer--;
    }
}

void APU::DMCChannel::restart() {
    current_addr = sample_addr;
    bytes_remaining = sample_len;
}

// ============================================================
// Frame Counter
// ============================================================

void APU::quarter_frame() {
    pulse1.envelope.clock();
    pulse2.envelope.clock();
    noise.envelope.clock();
    triangle.clock_linear();
}

void APU::half_frame() {
    pulse1.clock_length();
    pulse2.clock_length();
    triangle.clock_length();
    noise.clock_length();
    pulse1.sweep.clock(pulse1.timer_period, true);
    pulse2.sweep.clock(pulse2.timer_period, false);
}

// ============================================================
// Main Clock (called every CPU cycle)
// ============================================================

void APU::clock() {
    // Triangle and DMC timers clock every CPU cycle
    triangle.clock_timer();
    dmc.clock_timer(dmcRead);

    // Pulse, Noise timers and Frame counter clock at CPU/2 (every other cycle)
    if (clock_counter % 2 == 0) {
        pulse1.clock_timer();
        pulse2.clock_timer();
        noise.clock_timer();

        // Frame sequencer (counts in APU half-cycles)
        frame_counter++;

        if (frame_mode == 0) {
            // 4-step mode
            if (frame_counter == 3729) {
                quarter_frame();
            } else if (frame_counter == 7457) {
                quarter_frame();
                half_frame();
            } else if (frame_counter == 11186) {
                quarter_frame();
            } else if (frame_counter == 14915) {
                quarter_frame();
                half_frame();
                if (!frame_irq_inhibit) {
                    frame_irq_flag = true;
                }
                frame_counter = 0;
            }
        } else {
            // 5-step mode (no IRQ)
            if (frame_counter == 3729) {
                quarter_frame();
            } else if (frame_counter == 7457) {
                quarter_frame();
                half_frame();
            } else if (frame_counter == 11186) {
                quarter_frame();
            } else if (frame_counter == 18641) {
                quarter_frame();
                half_frame();
                frame_counter = 0;
            }
        }
    }

    // Aggregate IRQ output
    irq = frame_irq_flag || dmc.irq_flag;

    clock_counter++;
}

// ============================================================
// Register Writes ($4000-$4017)
// ============================================================

void APU::cpuWrite(uint16_t addr, uint8_t data) {
    switch (addr) {
        // ---- Pulse 1 ($4000-$4003) ----
        case 0x4000:
            pulse1.duty = (data >> 6) & 0x03;
            pulse1.halt = (data & 0x20) != 0;
            pulse1.envelope.loop_flag = (data & 0x20) != 0;
            pulse1.envelope.constant_flag = (data & 0x10) != 0;
            pulse1.envelope.volume = data & 0x0F;
            break;
        case 0x4001:
            pulse1.sweep.enabled = (data & 0x80) != 0;
            pulse1.sweep.period = (data >> 4) & 0x07;
            pulse1.sweep.negate = (data & 0x08) != 0;
            pulse1.sweep.shift = data & 0x07;
            pulse1.sweep.reload = true;
            break;
        case 0x4002:
            pulse1.timer_period = (pulse1.timer_period & 0xFF00) | data;
            break;
        case 0x4003:
            pulse1.timer_period = (pulse1.timer_period & 0x00FF) | ((uint16_t)(data & 0x07) << 8);
            if (pulse1.enabled)
                pulse1.length = length_table[(data >> 3) & 0x1F];
            pulse1.duty_pos = 0;
            pulse1.envelope.start = true;
            break;

        // ---- Pulse 2 ($4004-$4007) ----
        case 0x4004:
            pulse2.duty = (data >> 6) & 0x03;
            pulse2.halt = (data & 0x20) != 0;
            pulse2.envelope.loop_flag = (data & 0x20) != 0;
            pulse2.envelope.constant_flag = (data & 0x10) != 0;
            pulse2.envelope.volume = data & 0x0F;
            break;
        case 0x4005:
            pulse2.sweep.enabled = (data & 0x80) != 0;
            pulse2.sweep.period = (data >> 4) & 0x07;
            pulse2.sweep.negate = (data & 0x08) != 0;
            pulse2.sweep.shift = data & 0x07;
            pulse2.sweep.reload = true;
            break;
        case 0x4006:
            pulse2.timer_period = (pulse2.timer_period & 0xFF00) | data;
            break;
        case 0x4007:
            pulse2.timer_period = (pulse2.timer_period & 0x00FF) | ((uint16_t)(data & 0x07) << 8);
            if (pulse2.enabled)
                pulse2.length = length_table[(data >> 3) & 0x1F];
            pulse2.duty_pos = 0;
            pulse2.envelope.start = true;
            break;

        // ---- Triangle ($4008, $400A, $400B) ----
        case 0x4008:
            triangle.halt = (data & 0x80) != 0;
            triangle.linear_load = data & 0x7F;
            break;
        case 0x400A:
            triangle.timer_period = (triangle.timer_period & 0xFF00) | data;
            break;
        case 0x400B:
            triangle.timer_period = (triangle.timer_period & 0x00FF) | ((uint16_t)(data & 0x07) << 8);
            if (triangle.enabled)
                triangle.length = length_table[(data >> 3) & 0x1F];
            triangle.linear_reload = true;
            break;

        // ---- Noise ($400C, $400E, $400F) ----
        case 0x400C:
            noise.halt = (data & 0x20) != 0;
            noise.envelope.loop_flag = (data & 0x20) != 0;
            noise.envelope.constant_flag = (data & 0x10) != 0;
            noise.envelope.volume = data & 0x0F;
            break;
        case 0x400E:
            noise.mode = (data & 0x80) != 0;
            noise.timer_period = noise_table[data & 0x0F];
            break;
        case 0x400F:
            if (noise.enabled)
                noise.length = length_table[(data >> 3) & 0x1F];
            noise.envelope.start = true;
            break;

        // ---- DMC ($4010-$4013) ----
        case 0x4010:
            dmc.irq_enable = (data & 0x80) != 0;
            dmc.loop_flag = (data & 0x40) != 0;
            dmc.timer_period = dmc_rate_table[data & 0x0F];
            if (!dmc.irq_enable) dmc.irq_flag = false;
            break;
        case 0x4011:
            dmc.output_level = data & 0x7F;
            break;
        case 0x4012:
            dmc.sample_addr = 0xC000 + ((uint16_t)data * 64);
            break;
        case 0x4013:
            dmc.sample_len = ((uint16_t)data * 16) + 1;
            break;

        // ---- Status ($4015) ----
        case 0x4015:
            pulse1.enabled = (data & 0x01) != 0;
            if (!pulse1.enabled) pulse1.length = 0;

            pulse2.enabled = (data & 0x02) != 0;
            if (!pulse2.enabled) pulse2.length = 0;

            triangle.enabled = (data & 0x04) != 0;
            if (!triangle.enabled) triangle.length = 0;

            noise.enabled = (data & 0x08) != 0;
            if (!noise.enabled) noise.length = 0;

            dmc.irq_flag = false;
            if (data & 0x10) {
                if (dmc.bytes_remaining == 0)
                    dmc.restart();
            } else {
                dmc.bytes_remaining = 0;
            }
            break;

        // ---- Frame Counter ($4017) ----
        case 0x4017:
            frame_mode = (data >> 7) & 1;
            frame_irq_inhibit = (data & 0x40) != 0;
            if (frame_irq_inhibit) frame_irq_flag = false;
            frame_counter = 0;
            // 5-step mode immediately clocks quarter and half frame
            if (frame_mode == 1) {
                quarter_frame();
                half_frame();
            }
            break;
    }
}

// ============================================================
// Register Read ($4015)
// ============================================================

uint8_t APU::cpuRead(uint16_t addr) {
    uint8_t data = 0x00;
    if (addr == 0x4015) {
        if (pulse1.length > 0)       data |= 0x01;
        if (pulse2.length > 0)       data |= 0x02;
        if (triangle.length > 0)     data |= 0x04;
        if (noise.length > 0)        data |= 0x08;
        if (dmc.bytes_remaining > 0) data |= 0x10;
        if (frame_irq_flag)          data |= 0x40;
        if (dmc.irq_flag)            data |= 0x80;
        // Reading $4015 clears the frame IRQ flag
        frame_irq_flag = false;
    }
    return data;
}

// ============================================================
// Output Mixing (NES nonlinear mixing formula)
// ============================================================

double APU::GetOutputSample() {
    uint8_t p1 = pulse1.output(true);
    uint8_t p2 = pulse2.output(false);
    uint8_t t  = triangle.output();
    uint8_t n  = noise.output();
    uint8_t d  = dmc.output_level;

    // Pulse mixer (nonlinear)
    double pulse_out = 0.0;
    if (p1 + p2 > 0)
        pulse_out = 95.88 / (8128.0 / (double)(p1 + p2) + 100.0);

    // TND mixer (nonlinear)
    double tnd_out = 0.0;
    double tnd_sum = (double)t / 8227.0 + (double)n / 12241.0 + (double)d / 22638.0;
    if (tnd_sum > 0.0)
        tnd_out = 159.79 / (1.0 / tnd_sum + 100.0);

    return pulse_out + tnd_out;
}
