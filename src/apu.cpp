#include "apu.h"

// Length counter lookup table
static const u8 length_table[32] = {
    10, 254, 20,  2, 40,  4, 80,  6, 160,  8, 60, 10, 14, 12, 26, 14,
    12,  16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
};

// Noise period lookup table (NTSC)
static const u16 noise_period_table[16] = {
    4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
};

APU::APU()
    : frame_counter_mode(0), irq_inhibit(false), irq_flag(false)
    , frame_counter(0), cycles(0)
{
    // Initialize pulse channels
    for (int i = 0; i < 2; i++) {
        pulse[i].duty = 0;
        pulse[i].volume = 0;
        pulse[i].constant_volume = false;
        pulse[i].length_halt = false;
        pulse[i].timer = 0;
        pulse[i].timer_period = 0;
        pulse[i].length_counter = 0;
        pulse[i].enabled = false;
    }

    // Initialize triangle
    triangle.control = false;
    triangle.linear_counter = 0;
    triangle.linear_reload = 0;
    triangle.timer = 0;
    triangle.timer_period = 0;
    triangle.length_counter = 0;
    triangle.enabled = false;

    // Initialize noise
    noise.volume = 0;
    noise.constant_volume = false;
    noise.length_halt = false;
    noise.mode = false;
    noise.timer = 0;
    noise.timer_period = 0;
    noise.length_counter = 0;
    noise.shift_register = 1;
    noise.enabled = false;

    // Initialize DMC
    dmc.irq_enable = false;
    dmc.loop = false;
    dmc.rate = 0;
    dmc.output = 0;
    dmc.sample_addr = 0;
    dmc.sample_length = 0;
    dmc.enabled = false;
}

void APU::reset()
{
    writeRegister(0x4015, 0);
    writeRegister(0x4017, 0);
    cycles = 0;
    frame_counter = 0;
}

void APU::step()
{
    cycles++;
    
    // Frame counter (simplified - runs at ~240Hz for quarter frames)
    // In 4-step mode: steps at 3728.5, 7456.5, 11185.5, 14914.5 CPU cycles
    // For simplicity, we'll just increment and check modulo
    frame_counter++;
}

u8 APU::readRegister(u16 addr)
{
    u8 data = 0;

    if (addr == 0x4015) {
        // Status register
        if (pulse[0].length_counter > 0) data |= 0x01;
        if (pulse[1].length_counter > 0) data |= 0x02;
        if (triangle.length_counter > 0) data |= 0x04;
        if (noise.length_counter > 0)    data |= 0x08;
        if (dmc.enabled)                 data |= 0x10;
        if (irq_flag)                    data |= 0x40;
        
        irq_flag = false;  // Clear frame IRQ flag on read
    }

    return data;
}

void APU::writeRegister(u16 addr, u8 data)
{
    switch (addr) {
        // Pulse 1
        case 0x4000:
            pulse[0].duty = (data >> 6) & 0x03;
            pulse[0].length_halt = data & 0x20;
            pulse[0].constant_volume = data & 0x10;
            pulse[0].volume = data & 0x0F;
            break;
        case 0x4001:
            // Sweep (not fully implemented)
            break;
        case 0x4002:
            pulse[0].timer_period = (pulse[0].timer_period & 0x700) | data;
            break;
        case 0x4003:
            pulse[0].timer_period = (pulse[0].timer_period & 0xFF) | ((data & 0x07) << 8);
            if (pulse[0].enabled)
                pulse[0].length_counter = length_table[data >> 3];
            break;

        // Pulse 2
        case 0x4004:
            pulse[1].duty = (data >> 6) & 0x03;
            pulse[1].length_halt = data & 0x20;
            pulse[1].constant_volume = data & 0x10;
            pulse[1].volume = data & 0x0F;
            break;
        case 0x4005:
            // Sweep (not fully implemented)
            break;
        case 0x4006:
            pulse[1].timer_period = (pulse[1].timer_period & 0x700) | data;
            break;
        case 0x4007:
            pulse[1].timer_period = (pulse[1].timer_period & 0xFF) | ((data & 0x07) << 8);
            if (pulse[1].enabled)
                pulse[1].length_counter = length_table[data >> 3];
            break;

        // Triangle
        case 0x4008:
            triangle.control = data & 0x80;
            triangle.linear_reload = data & 0x7F;
            break;
        case 0x400A:
            triangle.timer_period = (triangle.timer_period & 0x700) | data;
            break;
        case 0x400B:
            triangle.timer_period = (triangle.timer_period & 0xFF) | ((data & 0x07) << 8);
            if (triangle.enabled)
                triangle.length_counter = length_table[data >> 3];
            break;

        // Noise
        case 0x400C:
            noise.length_halt = data & 0x20;
            noise.constant_volume = data & 0x10;
            noise.volume = data & 0x0F;
            break;
        case 0x400E:
            noise.mode = data & 0x80;
            noise.timer_period = noise_period_table[data & 0x0F];
            break;
        case 0x400F:
            if (noise.enabled)
                noise.length_counter = length_table[data >> 3];
            break;

        // DMC
        case 0x4010:
            dmc.irq_enable = data & 0x80;
            dmc.loop = data & 0x40;
            dmc.rate = data & 0x0F;
            break;
        case 0x4011:
            dmc.output = data & 0x7F;
            break;
        case 0x4012:
            dmc.sample_addr = 0xC000 + (data << 6);
            break;
        case 0x4013:
            dmc.sample_length = (data << 4) + 1;
            break;

        // Status
        case 0x4015:
            pulse[0].enabled = data & 0x01;
            pulse[1].enabled = data & 0x02;
            triangle.enabled = data & 0x04;
            noise.enabled = data & 0x08;
            dmc.enabled = data & 0x10;

            if (!pulse[0].enabled) pulse[0].length_counter = 0;
            if (!pulse[1].enabled) pulse[1].length_counter = 0;
            if (!triangle.enabled) triangle.length_counter = 0;
            if (!noise.enabled) noise.length_counter = 0;
            break;

        // Frame counter
        case 0x4017:
            frame_counter_mode = (data >> 7) & 0x01;
            irq_inhibit = data & 0x40;
            if (irq_inhibit) irq_flag = false;
            frame_counter = 0;
            break;
    }
}

float APU::getOutput() const
{
    // Mix all channels with proper volume levels
    // NES APU mixing formula (simplified)
    
    float pulse_out = 0.0f;
    float tnd_out = 0.0f;
    
    // Pulse channels (0-15 each when enabled)
    u8 pulse1_sample = (pulse[0].enabled && pulse[0].length_counter > 0) ? pulse[0].volume : 0;
    u8 pulse2_sample = (pulse[1].enabled && pulse[1].length_counter > 0) ? pulse[1].volume : 0;
    
    if (pulse1_sample || pulse2_sample) {
        pulse_out = 95.88f / ((8128.0f / (pulse1_sample + pulse2_sample)) + 100.0f);
    }
    
    // Triangle (0-15), Noise (0-15), DMC (0-127)
    u8 triangle_sample = (triangle.enabled && triangle.length_counter > 0) ? 15 : 0;
    u8 noise_sample = (noise.enabled && noise.length_counter > 0) ? noise.volume : 0;
    u8 dmc_sample = dmc.enabled ? dmc.output : 0;
    
    if (triangle_sample || noise_sample || dmc_sample) {
        tnd_out = 159.79f / ((1.0f / ((triangle_sample / 8227.0f) + (noise_sample / 12241.0f) + (dmc_sample / 22638.0f))) + 100.0f);
    }
    
    // Combine and normalize to [-1.0, 1.0]
    return pulse_out + tnd_out;
}
