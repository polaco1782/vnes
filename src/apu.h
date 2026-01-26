#ifndef APU_H
#define APU_H

#include "types.h"
#include <cstdint>

class APU {
public:
    APU();

    void reset();
    void step();

    // CPU interface (registers $4000-$4017)
    u8 readRegister(u16 addr);
    void writeRegister(u16 addr, u8 data);

    // Frame counter IRQ
    bool isIRQ() const { return irq_flag; }
    void clearIRQ() { irq_flag = false; }

    // For debugger - channel status
    struct ChannelStatus {
        bool enabled;
        u8 volume;
        u16 period;
        u8 length;
    };
    
    ChannelStatus getPulse1Status() const {
        return { pulse[0].enabled, pulse[0].volume, pulse[0].timer_period, pulse[0].length_counter };
    }
    ChannelStatus getPulse2Status() const {
        return { pulse[1].enabled, pulse[1].volume, pulse[1].timer_period, pulse[1].length_counter };
    }
    ChannelStatus getTriangleStatus() const {
        return { triangle.enabled, 0, triangle.timer_period, triangle.length_counter };
    }
    ChannelStatus getNoiseStatus() const {
        return { noise.enabled, noise.volume, noise.timer_period, noise.length_counter };
    }
    ChannelStatus getDMCStatus() const {
        return { dmc.enabled, dmc.output, dmc.rate, 0 };
    }
    
    u8 getFrameCounterMode() const { return frame_counter_mode; }
    bool getIrqInhibit() const { return irq_inhibit; }
    
    // Audio output
    float getOutput() const;

private:
    // Pulse channels
    struct Pulse {
        u8 duty;
        u8 volume;
        bool constant_volume;
        bool length_halt;
        u16 timer;
        u16 timer_period;
        u8 length_counter;
        bool enabled;
    } pulse[2];

    // Triangle channel
    struct Triangle {
        bool control;
        u8 linear_counter;
        u8 linear_reload;
        u16 timer;
        u16 timer_period;
        u8 length_counter;
        bool enabled;
    } triangle;

    // Noise channel
    struct Noise {
        u8 volume;
        bool constant_volume;
        bool length_halt;
        bool mode;
        u16 timer;
        u16 timer_period;
        u8 length_counter;
        u16 shift_register;
        bool enabled;
    } noise;

    // DMC channel
    struct DMC {
        bool irq_enable;
        bool loop;
        u16 rate;
        u8 output;
        u16 sample_addr;
        u16 sample_length;
        bool enabled;
    } dmc;

    // Frame counter
    u8 frame_counter_mode;
    bool irq_inhibit;
    bool irq_flag;
    u32 frame_counter;

    u64 cycles;
};

#endif // APU_H
