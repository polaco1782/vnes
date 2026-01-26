#include "cpu.h"
#include "bus.h"

CPU::CPU()
    : pc(0), sp(0), a(0), x(0), y(0), status(0), cycles(0), bus(nullptr)
{
}

void CPU::connect(Bus* b)
{
    bus = b;
}

void CPU::reset()
{
    // Read reset vector
    u16 lo = read(0xFFFC);
    u16 hi = read(0xFFFD);
    pc = (hi << 8) | lo;

    // Initialize registers
    a = 0;
    x = 0;
    y = 0;
    sp = 0xFD;
    status = FLAG_U | FLAG_I;

    cycles = 7;  // Reset takes 7 cycles
}

void CPU::irq()
{
    if (!getFlag(FLAG_I)) {
        push16(pc);
        setFlag(FLAG_B, false);
        setFlag(FLAG_U, true);
        setFlag(FLAG_I, true);
        push(status);

        u16 lo = read(0xFFFE);
        u16 hi = read(0xFFFF);
        pc = (hi << 8) | lo;

        cycles += 7;
    }
}

void CPU::nmi()
{
    push16(pc);
    setFlag(FLAG_B, false);
    setFlag(FLAG_U, true);
    setFlag(FLAG_I, true);
    push(status);

    u16 lo = read(0xFFFA);
    u16 hi = read(0xFFFB);
    pc = (hi << 8) | lo;

    cycles += 7;
}

// Memory access
u8 CPU::read(u16 addr)
{
    return bus->cpuRead(addr);
}

void CPU::write(u16 addr, u8 data)
{
    bus->cpuWrite(addr, data);
}

// Stack operations
void CPU::push(u8 data)
{
    write(0x0100 + sp, data);
    sp--;
}

u8 CPU::pull()
{
    sp++;
    return read(0x0100 + sp);
}

void CPU::push16(u16 data)
{
    push((data >> 8) & 0xFF);
    push(data & 0xFF);
}

u16 CPU::pull16()
{
    u16 lo = pull();
    u16 hi = pull();
    return (hi << 8) | lo;
}

// Flag operations
void CPU::setFlag(Flag f, bool v)
{
    if (v)
        status |= f;
    else
        status &= ~f;
}

bool CPU::getFlag(Flag f) const
{
    return (status & f) != 0;
}

void CPU::updateZN(u8 value)
{
    setFlag(FLAG_Z, value == 0);
    setFlag(FLAG_N, value & 0x80);
}

// Addressing modes
u16 CPU::addr_imm()
{
    return pc++;
}

u16 CPU::addr_zp()
{
    return read(pc++) & 0xFF;
}

u16 CPU::addr_zpx()
{
    cycles++;
    return (read(pc++) + x) & 0xFF;
}

u16 CPU::addr_zpy()
{
    cycles++;
    return (read(pc++) + y) & 0xFF;
}

u16 CPU::addr_abs()
{
    u16 lo = read(pc++);
    u16 hi = read(pc++);
    return (hi << 8) | lo;
}

u16 CPU::addr_abx()
{
    u16 lo = read(pc++);
    u16 hi = read(pc++);
    u16 addr = ((hi << 8) | lo) + x;
    if ((addr & 0xFF00) != (hi << 8))
        cycles++;  // Page boundary crossed
    return addr;
}

u16 CPU::addr_aby()
{
    u16 lo = read(pc++);
    u16 hi = read(pc++);
    u16 addr = ((hi << 8) | lo) + y;
    if ((addr & 0xFF00) != (hi << 8))
        cycles++;  // Page boundary crossed
    return addr;
}

u16 CPU::addr_ind()
{
    u16 ptr_lo = read(pc++);
    u16 ptr_hi = read(pc++);
    u16 ptr = (ptr_hi << 8) | ptr_lo;

    // 6502 bug: if low byte is 0xFF, wraps within page
    if (ptr_lo == 0xFF)
        return (read(ptr & 0xFF00) << 8) | read(ptr);
    else
        return (read(ptr + 1) << 8) | read(ptr);
}

u16 CPU::addr_izx()
{
    u8 ptr = read(pc++) + x;
    cycles++;
    u16 lo = read(ptr & 0xFF);
    u16 hi = read((ptr + 1) & 0xFF);
    return (hi << 8) | lo;
}

u16 CPU::addr_izy()
{
    u8 ptr = read(pc++);
    u16 lo = read(ptr & 0xFF);
    u16 hi = read((ptr + 1) & 0xFF);
    u16 addr = ((hi << 8) | lo) + y;
    if ((addr & 0xFF00) != (hi << 8))
        cycles++;  // Page boundary crossed
    return addr;
}

// Instructions
void CPU::op_adc(u16 addr)
{
    u8 m = read(addr);
    u16 sum = a + m + (getFlag(FLAG_C) ? 1 : 0);
    setFlag(FLAG_C, sum > 0xFF);
    setFlag(FLAG_V, (~(a ^ m) & (a ^ sum)) & 0x80);
    a = sum & 0xFF;
    updateZN(a);
}

void CPU::op_and(u16 addr)
{
    a &= read(addr);
    updateZN(a);
}

void CPU::op_asl_a()
{
    setFlag(FLAG_C, a & 0x80);
    a <<= 1;
    updateZN(a);
}

void CPU::op_asl(u16 addr)
{
    u8 m = read(addr);
    setFlag(FLAG_C, m & 0x80);
    m <<= 1;
    write(addr, m);
    updateZN(m);
    cycles++;
}

void CPU::branch(bool condition)
{
    s8 offset = (s8)read(pc++);
    if (condition) {
        cycles++;
        u16 new_pc = pc + offset;
        if ((new_pc & 0xFF00) != (pc & 0xFF00))
            cycles++;  // Page boundary crossed
        pc = new_pc;
    }
}

void CPU::op_bcc() { branch(!getFlag(FLAG_C)); }
void CPU::op_bcs() { branch(getFlag(FLAG_C)); }
void CPU::op_beq() { branch(getFlag(FLAG_Z)); }
void CPU::op_bmi() { branch(getFlag(FLAG_N)); }
void CPU::op_bne() { branch(!getFlag(FLAG_Z)); }
void CPU::op_bpl() { branch(!getFlag(FLAG_N)); }
void CPU::op_bvc() { branch(!getFlag(FLAG_V)); }
void CPU::op_bvs() { branch(getFlag(FLAG_V)); }

void CPU::op_bit(u16 addr)
{
    u8 m = read(addr);
    setFlag(FLAG_Z, (a & m) == 0);
    setFlag(FLAG_V, m & 0x40);
    setFlag(FLAG_N, m & 0x80);
}

void CPU::op_brk()
{
    pc++;
    push16(pc);
    setFlag(FLAG_B, true);
    setFlag(FLAG_U, true);
    push(status);
    setFlag(FLAG_I, true);

    u16 lo = read(0xFFFE);
    u16 hi = read(0xFFFF);
    pc = (hi << 8) | lo;
}

void CPU::op_clc() { setFlag(FLAG_C, false); }
void CPU::op_cld() { setFlag(FLAG_D, false); }
void CPU::op_cli() { setFlag(FLAG_I, false); }
void CPU::op_clv() { setFlag(FLAG_V, false); }

void CPU::op_cmp(u16 addr)
{
    u8 m = read(addr);
    setFlag(FLAG_C, a >= m);
    updateZN(a - m);
}

void CPU::op_cpx(u16 addr)
{
    u8 m = read(addr);
    setFlag(FLAG_C, x >= m);
    updateZN(x - m);
}

void CPU::op_cpy(u16 addr)
{
    u8 m = read(addr);
    setFlag(FLAG_C, y >= m);
    updateZN(y - m);
}

void CPU::op_dec(u16 addr)
{
    u8 m = read(addr) - 1;
    write(addr, m);
    updateZN(m);
    cycles++;
}

void CPU::op_dex()
{
    x--;
    updateZN(x);
}

void CPU::op_dey()
{
    y--;
    updateZN(y);
}

void CPU::op_eor(u16 addr)
{
    a ^= read(addr);
    updateZN(a);
}

void CPU::op_inc(u16 addr)
{
    u8 m = read(addr) + 1;
    write(addr, m);
    updateZN(m);
    cycles++;
}

void CPU::op_inx()
{
    x++;
    updateZN(x);
}

void CPU::op_iny()
{
    y++;
    updateZN(y);
}

void CPU::op_jmp(u16 addr)
{
    pc = addr;
}

void CPU::op_jsr(u16 addr)
{
    push16(pc - 1);
    pc = addr;
}

void CPU::op_lda(u16 addr)
{
    a = read(addr);
    updateZN(a);
}

void CPU::op_ldx(u16 addr)
{
    x = read(addr);
    updateZN(x);
}

void CPU::op_ldy(u16 addr)
{
    y = read(addr);
    updateZN(y);
}

void CPU::op_lsr_a()
{
    setFlag(FLAG_C, a & 0x01);
    a >>= 1;
    updateZN(a);
}

void CPU::op_lsr(u16 addr)
{
    u8 m = read(addr);
    setFlag(FLAG_C, m & 0x01);
    m >>= 1;
    write(addr, m);
    updateZN(m);
    cycles++;
}

void CPU::op_nop() { }

void CPU::op_ora(u16 addr)
{
    a |= read(addr);
    updateZN(a);
}

void CPU::op_pha()
{
    push(a);
}

void CPU::op_php()
{
    push(status | FLAG_B | FLAG_U);
}

void CPU::op_pla()
{
    a = pull();
    updateZN(a);
    cycles++;
}

void CPU::op_plp()
{
    status = pull();
    setFlag(FLAG_U, true);
    setFlag(FLAG_B, false);
    cycles++;
}

void CPU::op_rol_a()
{
    u8 carry = getFlag(FLAG_C) ? 1 : 0;
    setFlag(FLAG_C, a & 0x80);
    a = (a << 1) | carry;
    updateZN(a);
}

void CPU::op_rol(u16 addr)
{
    u8 m = read(addr);
    u8 carry = getFlag(FLAG_C) ? 1 : 0;
    setFlag(FLAG_C, m & 0x80);
    m = (m << 1) | carry;
    write(addr, m);
    updateZN(m);
    cycles++;
}

void CPU::op_ror_a()
{
    u8 carry = getFlag(FLAG_C) ? 0x80 : 0;
    setFlag(FLAG_C, a & 0x01);
    a = (a >> 1) | carry;
    updateZN(a);
}

void CPU::op_ror(u16 addr)
{
    u8 m = read(addr);
    u8 carry = getFlag(FLAG_C) ? 0x80 : 0;
    setFlag(FLAG_C, m & 0x01);
    m = (m >> 1) | carry;
    write(addr, m);
    updateZN(m);
    cycles++;
}

void CPU::op_rti()
{
    status = pull();
    setFlag(FLAG_U, true);
    setFlag(FLAG_B, false);
    pc = pull16();
}

void CPU::op_rts()
{
    pc = pull16() + 1;
}

void CPU::op_sbc(u16 addr)
{
    u8 m = read(addr) ^ 0xFF;  // Invert for subtraction
    u16 sum = a + m + (getFlag(FLAG_C) ? 1 : 0);
    setFlag(FLAG_C, sum > 0xFF);
    setFlag(FLAG_V, (~(a ^ m) & (a ^ sum)) & 0x80);
    a = sum & 0xFF;
    updateZN(a);
}

void CPU::op_sec() { setFlag(FLAG_C, true); }
void CPU::op_sed() { setFlag(FLAG_D, true); }
void CPU::op_sei() { setFlag(FLAG_I, true); }

void CPU::op_sta(u16 addr)
{
    write(addr, a);
}

void CPU::op_stx(u16 addr)
{
    write(addr, x);
}

void CPU::op_sty(u16 addr)
{
    write(addr, y);
}

void CPU::op_tax()
{
    x = a;
    updateZN(x);
}

void CPU::op_tay()
{
    y = a;
    updateZN(y);
}

void CPU::op_tsx()
{
    x = sp;
    updateZN(x);
}

void CPU::op_txa()
{
    a = x;
    updateZN(a);
}

void CPU::op_txs()
{
    sp = x;
}

void CPU::op_tya()
{
    a = y;
    updateZN(a);
}

// Main execution step - decode and execute one instruction
void CPU::step()
{
    u8 opcode = read(pc++);

    // Base cycles for each instruction (will be added to by addressing modes)
    switch (opcode) {
        // ADC
        case 0x69: cycles += 2; op_adc(addr_imm()); break;
        case 0x65: cycles += 3; op_adc(addr_zp()); break;
        case 0x75: cycles += 4; op_adc(addr_zpx()); break;
        case 0x6D: cycles += 4; op_adc(addr_abs()); break;
        case 0x7D: cycles += 4; op_adc(addr_abx()); break;
        case 0x79: cycles += 4; op_adc(addr_aby()); break;
        case 0x61: cycles += 6; op_adc(addr_izx()); break;
        case 0x71: cycles += 5; op_adc(addr_izy()); break;

        // AND
        case 0x29: cycles += 2; op_and(addr_imm()); break;
        case 0x25: cycles += 3; op_and(addr_zp()); break;
        case 0x35: cycles += 4; op_and(addr_zpx()); break;
        case 0x2D: cycles += 4; op_and(addr_abs()); break;
        case 0x3D: cycles += 4; op_and(addr_abx()); break;
        case 0x39: cycles += 4; op_and(addr_aby()); break;
        case 0x21: cycles += 6; op_and(addr_izx()); break;
        case 0x31: cycles += 5; op_and(addr_izy()); break;

        // ASL
        case 0x0A: cycles += 2; op_asl_a(); break;
        case 0x06: cycles += 5; op_asl(addr_zp()); break;
        case 0x16: cycles += 6; op_asl(addr_zpx()); break;
        case 0x0E: cycles += 6; op_asl(addr_abs()); break;
        case 0x1E: cycles += 7; op_asl(addr_abx()); break;

        // Branch
        case 0x90: cycles += 2; op_bcc(); break;
        case 0xB0: cycles += 2; op_bcs(); break;
        case 0xF0: cycles += 2; op_beq(); break;
        case 0x30: cycles += 2; op_bmi(); break;
        case 0xD0: cycles += 2; op_bne(); break;
        case 0x10: cycles += 2; op_bpl(); break;
        case 0x50: cycles += 2; op_bvc(); break;
        case 0x70: cycles += 2; op_bvs(); break;

        // BIT
        case 0x24: cycles += 3; op_bit(addr_zp()); break;
        case 0x2C: cycles += 4; op_bit(addr_abs()); break;

        // BRK
        case 0x00: cycles += 7; op_brk(); break;

        // Clear flags
        case 0x18: cycles += 2; op_clc(); break;
        case 0xD8: cycles += 2; op_cld(); break;
        case 0x58: cycles += 2; op_cli(); break;
        case 0xB8: cycles += 2; op_clv(); break;

        // CMP
        case 0xC9: cycles += 2; op_cmp(addr_imm()); break;
        case 0xC5: cycles += 3; op_cmp(addr_zp()); break;
        case 0xD5: cycles += 4; op_cmp(addr_zpx()); break;
        case 0xCD: cycles += 4; op_cmp(addr_abs()); break;
        case 0xDD: cycles += 4; op_cmp(addr_abx()); break;
        case 0xD9: cycles += 4; op_cmp(addr_aby()); break;
        case 0xC1: cycles += 6; op_cmp(addr_izx()); break;
        case 0xD1: cycles += 5; op_cmp(addr_izy()); break;

        // CPX
        case 0xE0: cycles += 2; op_cpx(addr_imm()); break;
        case 0xE4: cycles += 3; op_cpx(addr_zp()); break;
        case 0xEC: cycles += 4; op_cpx(addr_abs()); break;

        // CPY
        case 0xC0: cycles += 2; op_cpy(addr_imm()); break;
        case 0xC4: cycles += 3; op_cpy(addr_zp()); break;
        case 0xCC: cycles += 4; op_cpy(addr_abs()); break;

        // DEC
        case 0xC6: cycles += 5; op_dec(addr_zp()); break;
        case 0xD6: cycles += 6; op_dec(addr_zpx()); break;
        case 0xCE: cycles += 6; op_dec(addr_abs()); break;
        case 0xDE: cycles += 7; op_dec(addr_abx()); break;

        // DEX/DEY
        case 0xCA: cycles += 2; op_dex(); break;
        case 0x88: cycles += 2; op_dey(); break;

        // EOR
        case 0x49: cycles += 2; op_eor(addr_imm()); break;
        case 0x45: cycles += 3; op_eor(addr_zp()); break;
        case 0x55: cycles += 4; op_eor(addr_zpx()); break;
        case 0x4D: cycles += 4; op_eor(addr_abs()); break;
        case 0x5D: cycles += 4; op_eor(addr_abx()); break;
        case 0x59: cycles += 4; op_eor(addr_aby()); break;
        case 0x41: cycles += 6; op_eor(addr_izx()); break;
        case 0x51: cycles += 5; op_eor(addr_izy()); break;

        // INC
        case 0xE6: cycles += 5; op_inc(addr_zp()); break;
        case 0xF6: cycles += 6; op_inc(addr_zpx()); break;
        case 0xEE: cycles += 6; op_inc(addr_abs()); break;
        case 0xFE: cycles += 7; op_inc(addr_abx()); break;

        // INX/INY
        case 0xE8: cycles += 2; op_inx(); break;
        case 0xC8: cycles += 2; op_iny(); break;

        // JMP
        case 0x4C: cycles += 3; op_jmp(addr_abs()); break;
        case 0x6C: cycles += 5; op_jmp(addr_ind()); break;

        // JSR
        case 0x20: cycles += 6; op_jsr(addr_abs()); break;

        // LDA
        case 0xA9: cycles += 2; op_lda(addr_imm()); break;
        case 0xA5: cycles += 3; op_lda(addr_zp()); break;
        case 0xB5: cycles += 4; op_lda(addr_zpx()); break;
        case 0xAD: cycles += 4; op_lda(addr_abs()); break;
        case 0xBD: cycles += 4; op_lda(addr_abx()); break;
        case 0xB9: cycles += 4; op_lda(addr_aby()); break;
        case 0xA1: cycles += 6; op_lda(addr_izx()); break;
        case 0xB1: cycles += 5; op_lda(addr_izy()); break;

        // LDX
        case 0xA2: cycles += 2; op_ldx(addr_imm()); break;
        case 0xA6: cycles += 3; op_ldx(addr_zp()); break;
        case 0xB6: cycles += 4; op_ldx(addr_zpy()); break;
        case 0xAE: cycles += 4; op_ldx(addr_abs()); break;
        case 0xBE: cycles += 4; op_ldx(addr_aby()); break;

        // LDY
        case 0xA0: cycles += 2; op_ldy(addr_imm()); break;
        case 0xA4: cycles += 3; op_ldy(addr_zp()); break;
        case 0xB4: cycles += 4; op_ldy(addr_zpx()); break;
        case 0xAC: cycles += 4; op_ldy(addr_abs()); break;
        case 0xBC: cycles += 4; op_ldy(addr_abx()); break;

        // LSR
        case 0x4A: cycles += 2; op_lsr_a(); break;
        case 0x46: cycles += 5; op_lsr(addr_zp()); break;
        case 0x56: cycles += 6; op_lsr(addr_zpx()); break;
        case 0x4E: cycles += 6; op_lsr(addr_abs()); break;
        case 0x5E: cycles += 7; op_lsr(addr_abx()); break;

        // NOP
        case 0xEA: cycles += 2; op_nop(); break;

        // ORA
        case 0x09: cycles += 2; op_ora(addr_imm()); break;
        case 0x05: cycles += 3; op_ora(addr_zp()); break;
        case 0x15: cycles += 4; op_ora(addr_zpx()); break;
        case 0x0D: cycles += 4; op_ora(addr_abs()); break;
        case 0x1D: cycles += 4; op_ora(addr_abx()); break;
        case 0x19: cycles += 4; op_ora(addr_aby()); break;
        case 0x01: cycles += 6; op_ora(addr_izx()); break;
        case 0x11: cycles += 5; op_ora(addr_izy()); break;

        // Stack
        case 0x48: cycles += 3; op_pha(); break;
        case 0x08: cycles += 3; op_php(); break;
        case 0x68: cycles += 4; op_pla(); break;
        case 0x28: cycles += 4; op_plp(); break;

        // ROL
        case 0x2A: cycles += 2; op_rol_a(); break;
        case 0x26: cycles += 5; op_rol(addr_zp()); break;
        case 0x36: cycles += 6; op_rol(addr_zpx()); break;
        case 0x2E: cycles += 6; op_rol(addr_abs()); break;
        case 0x3E: cycles += 7; op_rol(addr_abx()); break;

        // ROR
        case 0x6A: cycles += 2; op_ror_a(); break;
        case 0x66: cycles += 5; op_ror(addr_zp()); break;
        case 0x76: cycles += 6; op_ror(addr_zpx()); break;
        case 0x6E: cycles += 6; op_ror(addr_abs()); break;
        case 0x7E: cycles += 7; op_ror(addr_abx()); break;

        // RTI/RTS
        case 0x40: cycles += 6; op_rti(); break;
        case 0x60: cycles += 6; op_rts(); break;

        // SBC
        case 0xE9: cycles += 2; op_sbc(addr_imm()); break;
        case 0xE5: cycles += 3; op_sbc(addr_zp()); break;
        case 0xF5: cycles += 4; op_sbc(addr_zpx()); break;
        case 0xED: cycles += 4; op_sbc(addr_abs()); break;
        case 0xFD: cycles += 4; op_sbc(addr_abx()); break;
        case 0xF9: cycles += 4; op_sbc(addr_aby()); break;
        case 0xE1: cycles += 6; op_sbc(addr_izx()); break;
        case 0xF1: cycles += 5; op_sbc(addr_izy()); break;

        // Set flags
        case 0x38: cycles += 2; op_sec(); break;
        case 0xF8: cycles += 2; op_sed(); break;
        case 0x78: cycles += 2; op_sei(); break;

        // STA
        case 0x85: cycles += 3; op_sta(addr_zp()); break;
        case 0x95: cycles += 4; op_sta(addr_zpx()); break;
        case 0x8D: cycles += 4; op_sta(addr_abs()); break;
        case 0x9D: cycles += 5; op_sta(addr_abx()); break;
        case 0x99: cycles += 5; op_sta(addr_aby()); break;
        case 0x81: cycles += 6; op_sta(addr_izx()); break;
        case 0x91: cycles += 6; op_sta(addr_izy()); break;

        // STX
        case 0x86: cycles += 3; op_stx(addr_zp()); break;
        case 0x96: cycles += 4; op_stx(addr_zpy()); break;
        case 0x8E: cycles += 4; op_stx(addr_abs()); break;

        // STY
        case 0x84: cycles += 3; op_sty(addr_zp()); break;
        case 0x94: cycles += 4; op_sty(addr_zpx()); break;
        case 0x8C: cycles += 4; op_sty(addr_abs()); break;

        // Transfer
        case 0xAA: cycles += 2; op_tax(); break;
        case 0xA8: cycles += 2; op_tay(); break;
        case 0xBA: cycles += 2; op_tsx(); break;
        case 0x8A: cycles += 2; op_txa(); break;
        case 0x9A: cycles += 2; op_txs(); break;
        case 0x98: cycles += 2; op_tya(); break;

        // Unofficial NOPs (common ones)
        case 0x1A: case 0x3A: case 0x5A: case 0x7A: case 0xDA: case 0xFA:
            cycles += 2; op_nop(); break;

        default:
            // Unknown opcode - treat as NOP
            cycles += 2;
            break;
    }
}
