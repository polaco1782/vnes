#ifndef CPU_H
#define CPU_H

#include "types.h"
#include <string>

// Forward declaration
class Bus;

// 6502 Status flags
enum Flag {
    FLAG_C = 0x01,  // Carry
    FLAG_Z = 0x02,  // Zero
    FLAG_I = 0x04,  // Interrupt Disable
    FLAG_D = 0x08,  // Decimal (unused on NES)
    FLAG_B = 0x10,  // Break
    FLAG_U = 0x20,  // Unused (always 1)
    FLAG_V = 0x40,  // Overflow
    FLAG_N = 0x80   // Negative
};

class CPU {
public:
    CPU();

    void connect(Bus* bus);
    void reset();
    void irq();
    void nmi();
    void step();

    // For debugging
    u16 getPC() const { return pc; }
    u8 getSP() const { return sp; }
    u8 getA() const { return a; }
    u8 getX() const { return x; }
    u8 getY() const { return y; }
    u8 getStatus() const { return status; }
    u64 getCycles() const { return cycles; }

    // For debugger manipulation
    void setPC(u16 val) { pc = val; }
    void setSP(u8 val) { sp = val; }
    void setA(u8 val) { a = val; }
    void setX(u8 val) { x = val; }
    void setY(u8 val) { y = val; }
    void setStatus(u8 val) { status = val; }

private:
    // Memory access
    u8 read(u16 addr);
    void write(u16 addr, u8 data);

    // Stack operations
    void push(u8 data);
    u8 pull();
    void push16(u16 data);
    u16 pull16();

    // Flag operations
    void setFlag(Flag f, bool v);
    bool getFlag(Flag f) const;
    void updateZN(u8 value);

    // Addressing modes - return address
    u16 addr_imm();
    u16 addr_zp();
    u16 addr_zpx();
    u16 addr_zpy();
    u16 addr_abs();
    u16 addr_abx();
    u16 addr_aby();
    u16 addr_ind();
    u16 addr_izx();
    u16 addr_izy();

    // Instructions
    void op_adc(u16 addr);
    void op_and(u16 addr);
    void op_asl_a();
    void op_asl(u16 addr);
    void op_bcc();
    void op_bcs();
    void op_beq();
    void op_bit(u16 addr);
    void op_bmi();
    void op_bne();
    void op_bpl();
    void op_brk();
    void op_bvc();
    void op_bvs();
    void op_clc();
    void op_cld();
    void op_cli();
    void op_clv();
    void op_cmp(u16 addr);
    void op_cpx(u16 addr);
    void op_cpy(u16 addr);
    void op_dec(u16 addr);
    void op_dex();
    void op_dey();
    void op_eor(u16 addr);
    void op_inc(u16 addr);
    void op_inx();
    void op_iny();
    void op_jmp(u16 addr);
    void op_jsr(u16 addr);
    void op_lda(u16 addr);
    void op_ldx(u16 addr);
    void op_ldy(u16 addr);
    void op_lsr_a();
    void op_lsr(u16 addr);
    void op_nop();
    void op_ora(u16 addr);
    void op_pha();
    void op_php();
    void op_pla();
    void op_plp();
    void op_rol_a();
    void op_rol(u16 addr);
    void op_ror_a();
    void op_ror(u16 addr);
    void op_rti();
    void op_rts();
    void op_sbc(u16 addr);
    void op_sec();
    void op_sed();
    void op_sei();
    void op_sta(u16 addr);
    void op_stx(u16 addr);
    void op_sty(u16 addr);
    void op_tax();
    void op_tay();
    void op_tsx();
    void op_txa();
    void op_txs();
    void op_tya();

    // Branch helper
    void branch(bool condition);

    // Registers
    u16 pc;     // Program counter
    u8 sp;      // Stack pointer
    u8 a;       // Accumulator
    u8 x;       // X register
    u8 y;       // Y register
    u8 status;  // Status register

    // Cycle counter
    u64 cycles;

    // Bus connection
    Bus* bus;
};

#endif // CPU_H
