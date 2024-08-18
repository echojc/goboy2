#include "z80.h"

void cpu_init(cpu *z) {
  z->sp = 0xfffe;
  z->pc = 0x0100;
  z->cycles = 0;
  z->cycles_prev = 0;
  z->halted = 0;
  z->stopped = 0;
  z->ints_enabled = 0;
  z->cart_reg1 = 0;
  z->cart_reg2 = 0;
  z->cart_reg3 = 0;
  z->xrom_bank = 1;
  z->xram_bank = 0;
  z->xram_enabled = 0;
  z->rom = (void *)0;
  z->xrom = (void *)0;
  z->xram = (void *)0;
}

u8 cpu_read(cpu *z, u16 addr) {
  if (addr >= 0xfe00) {
    return z->hram[addr-0xfe00];
  }
  switch (addr>>13) {
    case 0: return z->rom ? z->rom[addr&0x1fff] : 0;
    case 1: return z->rom ? z->rom[addr&0x3fff] : 0;
    case 2: return z->xrom ? z->xrom[addr&0x1fff] : 0;
    case 3: return z->xrom ? z->xrom[addr&0x3fff] : 0;
    case 4: return z->vram[addr&0x1fff];
    case 5: return (z->xram && z->xram_enabled) ? z->xram[addr&0x1fff] : 0xff;
    case 6:
    case 7: return z->ram[addr&0x1fff];
  }
  return 0;
}

// TODO: this only partially emulates MBC1
static void cpu_bank_select(cpu *z) {
  u8 shift = (z->rom ? z->rom[0x0148] : 0);
  u8 rom_mask = (u8)(2 << shift) - 1;
  u8 rom_bank = (u8)((z->cart_reg2&0x03)<<5)|(z->cart_reg1&0x1f);
  z->xrom_bank = ((rom_bank & 0x1f) ? rom_bank : (rom_bank + 1)) & rom_mask;

  if (z->cart_reg3 & 0x01) {
    u8 ram_mask;
    switch (z->rom ? z->rom[0x0149] : 0) {
      default:
      case 0x00:
      case 0x01:
      case 0x02: ram_mask = 0x00; break;
      case 0x03: ram_mask = 0x03; break;
      case 0x04: ram_mask = 0x0f; break;
      case 0x05: ram_mask = 0x07; break;
    }
    u8 ram_bank = z->cart_reg2 & 0x03;
    z->xram_bank = ram_bank & ram_mask;
  } else {
    z->xram_bank = 0;
  }
}

void cpu_write(cpu *z, u16 addr, u8 byte) {
  if (addr >= 0xfe00) {
    z->hram[addr-0xfe00] = byte;
    return;
  }
  switch (addr>>13) {
    case 0: z->xram_enabled = ((byte&0x0f) == XRAM_ENABLE); break;
    case 1: z->cart_reg1 = byte; cpu_bank_select(z); break;
    case 2: z->cart_reg2 = byte; cpu_bank_select(z); break;
    case 3: z->cart_reg3 = byte; cpu_bank_select(z); break;
    case 4: z->vram[addr&0x1fff] = byte; break;
    case 5: if (z->xram && z->xram_enabled) z->xram[addr&0x1fff] = byte; break;
    case 6:
    case 7: z->ram[addr&0x1fff] = byte; break;
  }
}

#define NN(l,h) ((u16)((l)+(((u16)(h))<<8)))
#define HL(z) NN((z)->l,(z)->h)
#define BC(z) NN((z)->c,(z)->b)
#define DE(z) NN((z)->e,(z)->d)

INLINE u16 set_hc_flags(cpu *z, u16 a, u16 b, u16 res) {
  a = (a ^ b ^ res) & 0x0110;
  z->f = (u8)((a | (a>>5)) << 1);
  return res;
}
INLINE u16 set_nhc_flags(cpu *z, u16 a, u16 b, u16 res) {
  a = (a ^ b ^ res) & 0x0110;
  z->f = (u8)((a | (a>>5)) << 1)|FLAG_N;
  return res;
}

INLINE void ld_b_n(cpu *z) { z->b = cpu_read(z, z->pc++); z->cycles += 2; }
INLINE void ld_c_n(cpu *z) { z->c = cpu_read(z, z->pc++); z->cycles += 2; }
INLINE void ld_d_n(cpu *z) { z->d = cpu_read(z, z->pc++); z->cycles += 2; }
INLINE void ld_e_n(cpu *z) { z->e = cpu_read(z, z->pc++); z->cycles += 2; }
INLINE void ld_h_n(cpu *z) { z->h = cpu_read(z, z->pc++); z->cycles += 2; }
INLINE void ld_l_n(cpu *z) { z->l = cpu_read(z, z->pc++); z->cycles += 2; }
INLINE void ld_hl_n(cpu *z) { cpu_write(z, HL(z), cpu_read(z, z->pc++)); z->cycles += 3; }
INLINE void ld_a_n(cpu *z) { z->a = cpu_read(z, z->pc++); z->cycles += 2; }
INLINE void ld_b_b(cpu *z) { z->b = z->b; z->cycles += 1; }
INLINE void ld_b_c(cpu *z) { z->b = z->c; z->cycles += 1; }
INLINE void ld_b_d(cpu *z) { z->b = z->d; z->cycles += 1; }
INLINE void ld_b_e(cpu *z) { z->b = z->e; z->cycles += 1; }
INLINE void ld_b_h(cpu *z) { z->b = z->h; z->cycles += 1; }
INLINE void ld_b_l(cpu *z) { z->b = z->l; z->cycles += 1; }
INLINE void ld_b_hl(cpu *z) { z->b = cpu_read(z, HL(z)); z->cycles += 2; }
INLINE void ld_b_a(cpu *z) { z->b = z->a; z->cycles += 1; }
INLINE void ld_c_b(cpu *z) { z->c = z->b; z->cycles += 1; }
INLINE void ld_c_c(cpu *z) { z->c = z->c; z->cycles += 1; }
INLINE void ld_c_d(cpu *z) { z->c = z->d; z->cycles += 1; }
INLINE void ld_c_e(cpu *z) { z->c = z->e; z->cycles += 1; }
INLINE void ld_c_h(cpu *z) { z->c = z->h; z->cycles += 1; }
INLINE void ld_c_l(cpu *z) { z->c = z->l; z->cycles += 1; }
INLINE void ld_c_hl(cpu *z) { z->c = cpu_read(z, HL(z)); z->cycles += 2; }
INLINE void ld_c_a(cpu *z) { z->c = z->a; z->cycles += 1; }
INLINE void ld_d_b(cpu *z) { z->d = z->b; z->cycles += 1; }
INLINE void ld_d_c(cpu *z) { z->d = z->c; z->cycles += 1; }
INLINE void ld_d_d(cpu *z) { z->d = z->d; z->cycles += 1; }
INLINE void ld_d_e(cpu *z) { z->d = z->e; z->cycles += 1; }
INLINE void ld_d_h(cpu *z) { z->d = z->h; z->cycles += 1; }
INLINE void ld_d_l(cpu *z) { z->d = z->l; z->cycles += 1; }
INLINE void ld_d_hl(cpu *z) { z->d = cpu_read(z, HL(z)); z->cycles += 2; }
INLINE void ld_d_a(cpu *z) { z->d = z->a; z->cycles += 1; }
INLINE void ld_e_b(cpu *z) { z->e = z->b; z->cycles += 1; }
INLINE void ld_e_c(cpu *z) { z->e = z->c; z->cycles += 1; }
INLINE void ld_e_d(cpu *z) { z->e = z->d; z->cycles += 1; }
INLINE void ld_e_e(cpu *z) { z->e = z->e; z->cycles += 1; }
INLINE void ld_e_h(cpu *z) { z->e = z->h; z->cycles += 1; }
INLINE void ld_e_l(cpu *z) { z->e = z->l; z->cycles += 1; }
INLINE void ld_e_hl(cpu *z) { z->e = cpu_read(z, HL(z)); z->cycles += 2; }
INLINE void ld_e_a(cpu *z) { z->e = z->a; z->cycles += 1;}
INLINE void ld_h_b(cpu *z) { z->h = z->b; z->cycles += 1; }
INLINE void ld_h_c(cpu *z) { z->h = z->c; z->cycles += 1; }
INLINE void ld_h_d(cpu *z) { z->h = z->d; z->cycles += 1; }
INLINE void ld_h_e(cpu *z) { z->h = z->e; z->cycles += 1; }
INLINE void ld_h_h(cpu *z) { z->h = z->h; z->cycles += 1; }
INLINE void ld_h_l(cpu *z) { z->h = z->l; z->cycles += 1; }
INLINE void ld_h_hl(cpu *z) { z->h = cpu_read(z, HL(z)); z->cycles += 2; }
INLINE void ld_h_a(cpu *z) { z->h = z->a; z->cycles += 1; }
INLINE void ld_l_b(cpu *z) { z->l = z->b; z->cycles += 1; }
INLINE void ld_l_c(cpu *z) { z->l = z->c; z->cycles += 1; }
INLINE void ld_l_d(cpu *z) { z->l = z->d; z->cycles += 1; }
INLINE void ld_l_e(cpu *z) { z->l = z->e; z->cycles += 1; }
INLINE void ld_l_h(cpu *z) { z->l = z->h; z->cycles += 1; }
INLINE void ld_l_l(cpu *z) { z->l = z->l; z->cycles += 1; }
INLINE void ld_l_hl(cpu *z) { z->l = cpu_read(z, HL(z)); z->cycles += 2; }
INLINE void ld_l_a(cpu *z) { z->l = z->a; z->cycles += 1;}
INLINE void ld_hl_b(cpu *z) { cpu_write(z, HL(z), z->b); z->cycles += 2; }
INLINE void ld_hl_c(cpu *z) { cpu_write(z, HL(z), z->c); z->cycles += 2; }
INLINE void ld_hl_d(cpu *z) { cpu_write(z, HL(z), z->d); z->cycles += 2; }
INLINE void ld_hl_e(cpu *z) { cpu_write(z, HL(z), z->e); z->cycles += 2; }
INLINE void ld_hl_h(cpu *z) { cpu_write(z, HL(z), z->h); z->cycles += 2; }
INLINE void ld_hl_l(cpu *z) { cpu_write(z, HL(z), z->l); z->cycles += 2; }
INLINE void ld_hl_a(cpu *z) { cpu_write(z, HL(z), z->a); z->cycles += 2; }
INLINE void ld_a_b(cpu *z) { z->a = z->b; z->cycles += 1; }
INLINE void ld_a_c(cpu *z) { z->a = z->c; z->cycles += 1; }
INLINE void ld_a_d(cpu *z) { z->a = z->d; z->cycles += 1; }
INLINE void ld_a_e(cpu *z) { z->a = z->e; z->cycles += 1; }
INLINE void ld_a_h(cpu *z) { z->a = z->h; z->cycles += 1; }
INLINE void ld_a_l(cpu *z) { z->a = z->l; z->cycles += 1; }
INLINE void ld_a_hl(cpu *z) { z->a = cpu_read(z, HL(z)); z->cycles += 2; }
INLINE void ld_a_a(cpu *z) { z->a = z->a; z->cycles += 1; }
INLINE void ld_bc_a(cpu *z) { cpu_write(z, BC(z), z->a); z->cycles += 2; }
INLINE void ld_a_bc(cpu *z) { z->a = cpu_read(z, BC(z)); z->cycles += 2; }
INLINE void ld_de_a(cpu *z) { cpu_write(z, DE(z), z->a); z->cycles += 2; }
INLINE void ld_a_de(cpu *z) { z->a = cpu_read(z, DE(z)); z->cycles += 2; }
INLINE void ld_a_nn(cpu *z) { u8 l = cpu_read(z, z->pc++); u8 h = cpu_read(z, z->pc++); z->a = cpu_read(z, NN(l, h)); z->cycles += 4; }
INLINE void ld_nn_a(cpu *z) { u8 l = cpu_read(z, z->pc++); u8 h = cpu_read(z, z->pc++); cpu_write(z, NN(l, h), z->a); z->cycles += 4; }
INLINE void ldh_c_a(cpu *z) { cpu_write(z, NN(z->c, 0xff), z->a); z->cycles += 2; }
INLINE void ldh_a_c(cpu *z) { z->a = cpu_read(z, NN(z->c, 0xff)); z->cycles += 2; }
INLINE void ldh_n_a(cpu *z) { cpu_write(z, NN(cpu_read(z, z->pc++), 0xff), z->a); z->cycles += 3; }
INLINE void ldh_a_n(cpu *z) { z->a = cpu_read(z, NN(cpu_read(z, z->pc++), 0xff)); z->cycles += 3; }
INLINE void ldi_hl_a(cpu *z) { cpu_write(z, HL(z), z->a); if (!(++z->l)) z->h++; z->cycles += 2; }
INLINE void ldi_a_hl(cpu *z) { z->a = cpu_read(z, HL(z)); if (!(++z->l)) z->h++; z->cycles += 2; }
INLINE void ldd_hl_a(cpu *z) { cpu_write(z, HL(z), z->a); if (!(z->l--)) z->h--; z->cycles += 2; }
INLINE void ldd_a_hl(cpu *z) { z->a = cpu_read(z, HL(z)); if (!(z->l--)) z->h--; z->cycles += 2; }
INLINE void ld_bc_nn(cpu *z) { z->c = cpu_read(z, z->pc++); z->b = cpu_read(z, z->pc++); z->cycles += 3; }
INLINE void ld_de_nn(cpu *z) { z->e = cpu_read(z, z->pc++); z->d = cpu_read(z, z->pc++); z->cycles += 3; }
INLINE void ld_hl_nn(cpu *z) { z->l = cpu_read(z, z->pc++); z->h = cpu_read(z, z->pc++); z->cycles += 3; }
INLINE void ld_sp_nn(cpu *z) { u8 l = cpu_read(z, z->pc++); u8 h = cpu_read(z, z->pc++); z->sp = NN(l, h); z->cycles += 3; }
INLINE void ld_sp_hl(cpu *z) { z->sp = NN(z->l, z->h); z->cycles += 2; }
INLINE void ldhl_sp_n(cpu *z) {
  s8 n = (s8)cpu_read(z, z->pc++);
  u16 t = set_hc_flags(z, z->sp, (u16)n, (u16)(z->sp+n));
  z->l = (u8)t;
  z->h = (u8)(t >> 8);
  z->cycles += 3;
}
INLINE void ld_nn_sp(cpu *z) {
  u8 l = cpu_read(z, z->pc++);
  u8 h = cpu_read(z, z->pc++);
  u16 p = NN(l, h);
  cpu_write(z, p, (u8)z->sp);
  cpu_write(z, p+1, (u8)(z->sp>>8));
  z->cycles += 5;
}
INLINE void push_bc(cpu *z) { cpu_write(z, --z->sp, z->b); cpu_write(z, --z->sp, z->c); z->cycles += 4; }
INLINE void push_de(cpu *z) { cpu_write(z, --z->sp, z->d); cpu_write(z, --z->sp, z->e); z->cycles += 4; }
INLINE void push_hl(cpu *z) { cpu_write(z, --z->sp, z->h); cpu_write(z, --z->sp, z->l); z->cycles += 4; }
INLINE void push_af(cpu *z) { cpu_write(z, --z->sp, z->a); cpu_write(z, --z->sp, z->f); z->cycles += 4; }
INLINE void pop_bc(cpu *z) { z->c = cpu_read(z, z->sp++); z->b = cpu_read(z, z->sp++); z->cycles += 3; }
INLINE void pop_de(cpu *z) { z->e = cpu_read(z, z->sp++); z->d = cpu_read(z, z->sp++); z->cycles += 3; }
INLINE void pop_hl(cpu *z) { z->l = cpu_read(z, z->sp++); z->h = cpu_read(z, z->sp++); z->cycles += 3; }
INLINE void pop_af(cpu *z) { z->f = cpu_read(z, z->sp++)&0xf0; z->a = cpu_read(z, z->sp++); z->cycles += 3; }

INLINE void add_a(cpu *z, u8 x) { if ((z->a = (u8)set_hc_flags(z, z->a, x, z->a+x)) == 0) z->f |= FLAG_Z; z->cycles += 1; }
INLINE void add_a_b(cpu *z) { add_a(z, z->b); }
INLINE void add_a_c(cpu *z) { add_a(z, z->c); }
INLINE void add_a_d(cpu *z) { add_a(z, z->d); }
INLINE void add_a_e(cpu *z) { add_a(z, z->e); }
INLINE void add_a_h(cpu *z) { add_a(z, z->h); }
INLINE void add_a_l(cpu *z) { add_a(z, z->l); }
INLINE void add_a_hl(cpu *z) { add_a(z, cpu_read(z, HL(z))); z->cycles += 1; }
INLINE void add_a_a(cpu *z) { add_a(z, z->a); }
INLINE void add_a_n(cpu *z) { add_a(z, cpu_read(z, z->pc++)); z->cycles += 1; }
INLINE void adc_a(cpu *z, u8 x) { if ((z->a = (u8)set_hc_flags(z, z->a, x, z->a+x+((z->f&FLAG_C)>>4))) == 0) z->f |= FLAG_Z; z->cycles += 1; }
INLINE void adc_a_b(cpu *z) { adc_a(z, z->b); }
INLINE void adc_a_c(cpu *z) { adc_a(z, z->c); }
INLINE void adc_a_d(cpu *z) { adc_a(z, z->d); }
INLINE void adc_a_e(cpu *z) { adc_a(z, z->e); }
INLINE void adc_a_h(cpu *z) { adc_a(z, z->h); }
INLINE void adc_a_l(cpu *z) { adc_a(z, z->l); }
INLINE void adc_a_hl(cpu *z) { adc_a(z, cpu_read(z, HL(z))); z->cycles += 1; }
INLINE void adc_a_a(cpu *z) { adc_a(z, z->a); }
INLINE void adc_a_n(cpu *z) { adc_a(z, cpu_read(z, z->pc++)); z->cycles += 1; }
INLINE void sub_a(cpu *z, u8 x) { if ((z->a = (u8)set_nhc_flags(z, z->a, x, z->a-x)) == 0) z->f |= FLAG_Z; z->cycles += 1; }
INLINE void sub_a_b(cpu *z) { sub_a(z, z->b); }
INLINE void sub_a_c(cpu *z) { sub_a(z, z->c); }
INLINE void sub_a_d(cpu *z) { sub_a(z, z->d); }
INLINE void sub_a_e(cpu *z) { sub_a(z, z->e); }
INLINE void sub_a_h(cpu *z) { sub_a(z, z->h); }
INLINE void sub_a_l(cpu *z) { sub_a(z, z->l); }
INLINE void sub_a_hl(cpu *z) { sub_a(z, cpu_read(z, HL(z))); z->cycles += 1; }
INLINE void sub_a_a(cpu *z) { sub_a(z, z->a); }
INLINE void sub_a_n(cpu *z) { sub_a(z, cpu_read(z, z->pc++)); z->cycles += 1; }
INLINE void sbc_a(cpu *z, u8 x) { if ((z->a = (u8)set_nhc_flags(z, z->a, x, z->a-x-((z->f&FLAG_C)>>4))) == 0) z->f |= FLAG_Z; z->cycles += 1; }
INLINE void sbc_a_b(cpu *z) { sbc_a(z, z->b); }
INLINE void sbc_a_c(cpu *z) { sbc_a(z, z->c); }
INLINE void sbc_a_d(cpu *z) { sbc_a(z, z->d); }
INLINE void sbc_a_e(cpu *z) { sbc_a(z, z->e); }
INLINE void sbc_a_h(cpu *z) { sbc_a(z, z->h); }
INLINE void sbc_a_l(cpu *z) { sbc_a(z, z->l); }
INLINE void sbc_a_hl(cpu *z) { sbc_a(z, cpu_read(z, HL(z))); z->cycles += 1; }
INLINE void sbc_a_a(cpu *z) { sbc_a(z, z->a); }
INLINE void sbc_a_n(cpu *z) { sbc_a(z, cpu_read(z, z->pc++)); z->cycles += 1; }
INLINE void and_a_b(cpu *z) { z->f = (z->a &= z->b) ? FLAG_H : (FLAG_Z | FLAG_H); z->cycles += 1; }
INLINE void and_a_c(cpu *z) { z->f = (z->a &= z->c) ? FLAG_H : (FLAG_Z | FLAG_H); z->cycles += 1; }
INLINE void and_a_d(cpu *z) { z->f = (z->a &= z->d) ? FLAG_H : (FLAG_Z | FLAG_H); z->cycles += 1; }
INLINE void and_a_e(cpu *z) { z->f = (z->a &= z->e) ? FLAG_H : (FLAG_Z | FLAG_H); z->cycles += 1; }
INLINE void and_a_h(cpu *z) { z->f = (z->a &= z->h) ? FLAG_H : (FLAG_Z | FLAG_H); z->cycles += 1; }
INLINE void and_a_l(cpu *z) { z->f = (z->a &= z->l) ? FLAG_H : (FLAG_Z | FLAG_H); z->cycles += 1; }
INLINE void and_a_hl(cpu *z) { z->f = (z->a &= cpu_read(z, HL(z))) ? FLAG_H : (FLAG_Z | FLAG_H); z->cycles += 2; }
INLINE void and_a_a(cpu *z) { z->f = (z->a &= z->a) ? FLAG_H : (FLAG_Z | FLAG_H); z->cycles += 1; }
INLINE void and_a_n(cpu *z) { z->f = (z->a &= cpu_read(z, z->pc++)) ? FLAG_H : (FLAG_Z | FLAG_H); z->cycles += 2; }
INLINE void xor_a_b(cpu *z) { z->f = (z->a ^= z->b) ? 0 : FLAG_Z; z->cycles += 1; }
INLINE void xor_a_c(cpu *z) { z->f = (z->a ^= z->c) ? 0 : FLAG_Z; z->cycles += 1; }
INLINE void xor_a_d(cpu *z) { z->f = (z->a ^= z->d) ? 0 : FLAG_Z; z->cycles += 1; }
INLINE void xor_a_e(cpu *z) { z->f = (z->a ^= z->e) ? 0 : FLAG_Z; z->cycles += 1; }
INLINE void xor_a_h(cpu *z) { z->f = (z->a ^= z->h) ? 0 : FLAG_Z; z->cycles += 1; }
INLINE void xor_a_l(cpu *z) { z->f = (z->a ^= z->l) ? 0 : FLAG_Z; z->cycles += 1; }
INLINE void xor_a_hl(cpu *z) { z->f = (z->a ^= cpu_read(z, HL(z))) ? 0 : FLAG_Z; z->cycles += 2; }
INLINE void xor_a_a(cpu *z) { z->f = (z->a ^= z->a) ? 0 : FLAG_Z; z->cycles += 1; }
INLINE void xor_a_n(cpu *z) { z->f = (z->a ^= cpu_read(z, z->pc++)) ? 0 : FLAG_Z; z->cycles += 2; }
INLINE void or_a_b(cpu *z) { z->f = (z->a |= z->b) ? 0 : FLAG_Z; z->cycles += 1; }
INLINE void or_a_c(cpu *z) { z->f = (z->a |= z->c) ? 0 : FLAG_Z; z->cycles += 1; }
INLINE void or_a_d(cpu *z) { z->f = (z->a |= z->d) ? 0 : FLAG_Z; z->cycles += 1; }
INLINE void or_a_e(cpu *z) { z->f = (z->a |= z->e) ? 0 : FLAG_Z; z->cycles += 1; }
INLINE void or_a_h(cpu *z) { z->f = (z->a |= z->h) ? 0 : FLAG_Z; z->cycles += 1; }
INLINE void or_a_l(cpu *z) { z->f = (z->a |= z->l) ? 0 : FLAG_Z; z->cycles += 1; }
INLINE void or_a_hl(cpu *z) { z->f = (z->a |= cpu_read(z, HL(z))) ? 0 : FLAG_Z; z->cycles += 2; }
INLINE void or_a_a(cpu *z) { z->f = (z->a |= z->a) ? 0 : FLAG_Z; z->cycles += 1; }
INLINE void or_a_n(cpu *z) { z->f = (z->a |= cpu_read(z, z->pc++)) ? 0 : FLAG_Z; z->cycles += 2; }
INLINE void cp_a(cpu *z, u8 x) { if (set_nhc_flags(z, z->a, x, z->a-x) == 0) z->f |= FLAG_Z; z->cycles += 1; }
INLINE void cp_a_b(cpu *z) { cp_a(z, z->b); }
INLINE void cp_a_c(cpu *z) { cp_a(z, z->c); }
INLINE void cp_a_d(cpu *z) { cp_a(z, z->d); }
INLINE void cp_a_e(cpu *z) { cp_a(z, z->e); }
INLINE void cp_a_h(cpu *z) { cp_a(z, z->h); }
INLINE void cp_a_l(cpu *z) { cp_a(z, z->l); }
INLINE void cp_a_hl(cpu *z) { cp_a(z, cpu_read(z, HL(z))); z->cycles += 1; }
INLINE void cp_a_a(cpu *z) { cp_a(z, z->a); }
INLINE void cp_a_n(cpu *z) { cp_a(z, cpu_read(z, z->pc++)); z->cycles += 1; }
INLINE void _inc(cpu *z, u8 *r) { (*r)++; z->f = (z->f&FLAG_C) | ((*r)?0:FLAG_Z) | (((*r)&0x0f)?0:FLAG_H); }
INLINE void inc_b(cpu *z) { _inc(z, &z->b); z->cycles += 1; }
INLINE void inc_c(cpu *z) { _inc(z, &z->c); z->cycles += 1; }
INLINE void inc_d(cpu *z) { _inc(z, &z->d); z->cycles += 1; }
INLINE void inc_e(cpu *z) { _inc(z, &z->e); z->cycles += 1; }
INLINE void inc_h(cpu *z) { _inc(z, &z->h); z->cycles += 1; }
INLINE void inc_l(cpu *z) { _inc(z, &z->l); z->cycles += 1; }
INLINE void inc_hl(cpu *z) {
  u8 t = cpu_read(z, HL(z))+1; cpu_write(z, HL(z), t);
  z->f = (z->f&FLAG_C) | (t?0:FLAG_Z) | ((t&0x0f)?0:FLAG_H);
  z->cycles += 3;
}
INLINE void inc_a(cpu *z) { _inc(z, &z->a); z->cycles += 1; }
INLINE void _dec(cpu *z, u8 *r) { (*r)--; z->f = (z->f&FLAG_C) | FLAG_N | ((*r)?0:FLAG_Z) | ((((*r)&0x0f)==0x0f)?FLAG_H:0); }
INLINE void dec_b(cpu *z) { _dec(z, &z->b); z->cycles += 1; }
INLINE void dec_c(cpu *z) { _dec(z, &z->c); z->cycles += 1; }
INLINE void dec_d(cpu *z) { _dec(z, &z->d); z->cycles += 1; }
INLINE void dec_e(cpu *z) { _dec(z, &z->e); z->cycles += 1; }
INLINE void dec_h(cpu *z) { _dec(z, &z->h); z->cycles += 1; }
INLINE void dec_l(cpu *z) { _dec(z, &z->l); z->cycles += 1; }
INLINE void dec_hl(cpu *z) {
  u8 t = cpu_read(z, HL(z))-1; cpu_write(z, HL(z), t);
  z->f = (z->f&FLAG_C) | FLAG_N | (t?0:FLAG_Z) | (((t&0x0f)==0x0f)?FLAG_H:0);
  z->cycles += 3;
}
INLINE void dec_a(cpu *z) { _dec(z, &z->a); z->cycles += 1; }

INLINE void add_hl(cpu *z, u32 x) {
  u32 hl = ((u32)(z->h)<<8)+z->l;
  u32 res = hl + x;
  x = (hl ^ x ^ res) & 0x011000;
  z->l = (u8)res;
  z->h = (u8)(res>>8);
  z->f = (z->f&FLAG_Z) | ((x&0x1000)?FLAG_H:0) | ((x&0x10000)?FLAG_C:0);
  z->cycles += 2;
}
INLINE void add_hl_bc(cpu *z) { add_hl(z, ((u32)(z->b)<<8)+z->c); }
INLINE void add_hl_de(cpu *z) { add_hl(z, ((u32)(z->d)<<8)+z->e); }
INLINE void add_hl_hl(cpu *z) { add_hl(z, ((u32)(z->h)<<8)+z->l); }
INLINE void add_hl_sp(cpu *z) { add_hl(z, z->sp); }
INLINE void add_sp_n(cpu *z) {
  s8 n = (s8)cpu_read(z, z->pc++);
  z->sp = set_hc_flags(z, z->sp, (u16)n, (u16)(z->sp+n));
  z->cycles += 4;
}
INLINE void inc_bc(cpu *z) { if (++z->c == 0) z->b++; z->cycles += 2; }
INLINE void inc_de(cpu *z) { if (++z->e == 0) z->d++; z->cycles += 2; }
INLINE void inc_rhl(cpu *z) { if (++z->l == 0) z->h++; z->cycles += 2; }
INLINE void inc_sp(cpu *z) { z->sp++; z->cycles += 2; }
INLINE void dec_bc(cpu *z) { if (z->c-- == 0) z->b--; z->cycles += 2; }
INLINE void dec_de(cpu *z) { if (z->e-- == 0) z->d--; z->cycles += 2; }
INLINE void dec_rhl(cpu *z) { if (z->l-- == 0) z->h--; z->cycles += 2; }
INLINE void dec_sp(cpu *z) { z->sp--; z->cycles += 2; }
// https://blog.ollien.com/posts/gb-daa/
INLINE void daa(cpu *z) {
  u8 f_new = z->f & FLAG_N;
  if (z->f & FLAG_N) {
    if (z->f & FLAG_C) { z->a -= 0x60; f_new |= FLAG_C; }
    if (z->f & FLAG_H) z->a -= 0x06;
  } else {
    if ((z->f & FLAG_C) || (z->a > 0x99)) { z->a += 0x60; f_new |= FLAG_C; }
    if ((z->f & FLAG_H) || ((z->a & 0x0f) > 0x09)) z->a += 0x06;
  }
  z->f = f_new | (z->a?0:FLAG_Z);
  z->cycles += 1;
}
INLINE void cpl(cpu *z) { z->a = ~z->a; z->f = (z->f & (FLAG_Z|FLAG_C)) | FLAG_N | FLAG_H; z->cycles += 1;}
INLINE void ccf(cpu *z) { z->f = (z->f^FLAG_C)&(FLAG_Z|FLAG_C); z->cycles += 1; }
INLINE void scf(cpu *z) { z->f = (z->f&FLAG_Z)|FLAG_C; z->cycles += 1; }
INLINE void nop(cpu *z) { z->cycles += 1; }
INLINE void halt(cpu *z) { z->halted = 1; z->cycles += 1; }
INLINE void stop(cpu *z) { z->stopped = 1; z->cycles += 1; }
INLINE void di(cpu *z) { z->ints_enabled = 0; z->cycles += 1; }
INLINE void ei(cpu *z) { z->ints_enabled = 1; z->cycles += 1; }
INLINE void rlca(cpu *z) { z->f = (z->a>>3)&FLAG_C; z->a = (u8)(z->a<<1)|(z->a>>7); z->cycles += 1; }
INLINE void rla(cpu *z) { u8 t = (z->f&FLAG_C)>>4; z->f = (z->a>>3)&FLAG_C; z->a = (u8)(z->a<<1)|t; z->cycles += 1; }
INLINE void rrca(cpu *z) { z->f = (z->a<<4)&FLAG_C; z->a = (u8)(z->a<<7)|(z->a>>1); z->cycles += 1; }
INLINE void rra(cpu *z) { u8 t = (u8)((z->f&FLAG_C)<<3); z->f = (z->a<<4)&FLAG_C; z->a = t|(z->a>>1); z->cycles += 1; }
INLINE void _jp(cpu *z) { u8 l = cpu_read(z, z->pc++); u8 h = cpu_read(z, z->pc++); z->pc = NN(l, h); }
INLINE void jp(cpu *z) { _jp(z); z->cycles += 3; }
INLINE void jp_nz(cpu *z) { if (!(z->f & FLAG_Z)) { _jp(z); } else { z->pc += 2; } z->cycles += 3; }
INLINE void jp_z(cpu *z) { if (z->f & FLAG_Z) { _jp(z); } else { z->pc += 2; } z->cycles += 3; }
INLINE void jp_nc(cpu *z) { if (!(z->f & FLAG_C)) { _jp(z); } else { z->pc += 2; } z->cycles += 3; }
INLINE void jp_c(cpu *z) { if (z->f & FLAG_C) { _jp(z); } else { z->pc += 2; } z->cycles += 3; }
INLINE void jp_hl(cpu *z) { z->pc = HL(z); z->cycles += 1; }
INLINE void _jr(cpu *z) { z->pc += (s8)cpu_read(z, z->pc++); }
INLINE void jr(cpu *z) { _jr(z); z->cycles += 2; }
INLINE void jr_nz(cpu *z) { if (!(z->f & FLAG_Z)) { _jr(z); } else { z->pc += 1; } z->cycles += 2; }
INLINE void jr_z(cpu *z) { if (z->f & FLAG_Z) { _jr(z); } else { z->pc += 1; } z->cycles += 2; }
INLINE void jr_nc(cpu *z) { if (!(z->f & FLAG_C)) { _jr(z); } else { z->pc += 1; } z->cycles += 2; }
INLINE void jr_c(cpu *z) { if (z->f & FLAG_C) { _jr(z); } else { z->pc += 1; } z->cycles += 2; }
INLINE void _call(cpu *z) {
  u8 l = cpu_read(z, z->pc++);
  u8 h = cpu_read(z, z->pc++);
  cpu_write(z, --z->sp, (u8)(z->pc>>8));
  cpu_write(z, --z->sp, (u8)z->pc);
  z->pc = NN(l, h);
}
INLINE void call(cpu *z) { _call(z); z->cycles += 3; }
INLINE void call_nz(cpu *z) { if (!(z->f & FLAG_Z)) { _call(z); } else { z->pc += 2; } z->cycles += 3; }
INLINE void call_z(cpu *z) { if (z->f & FLAG_Z) { _call(z); } else { z->pc += 2; } z->cycles += 3; }
INLINE void call_nc(cpu *z) { if (!(z->f & FLAG_C)) { _call(z); } else { z->pc += 2; } z->cycles += 3; }
INLINE void call_c(cpu *z) { if (z->f & FLAG_C) { _call(z); } else { z->pc += 2; } z->cycles += 3; }
INLINE void rst(cpu *z, u16 n) { cpu_write(z, --z->sp, (u8)(z->pc>>8)); cpu_write(z, --z->sp, (u8)z->pc); z->pc = n; z->cycles += 8; }
INLINE void _ret(cpu *z) { u8 l = cpu_read(z, z->sp++); u8 h = cpu_read(z, z->sp++); z->pc = NN(l, h); }
INLINE void ret(cpu *z) { _ret(z); z->cycles += 2; }
INLINE void ret_nz(cpu *z) { if (!(z->f & FLAG_Z)) { _ret(z); } z->cycles += 2; }
INLINE void ret_z(cpu *z) { if (z->f & FLAG_Z) { _ret(z); } z->cycles += 2; }
INLINE void ret_nc(cpu *z) { if (!(z->f & FLAG_C)) { _ret(z); } z->cycles += 2; }
INLINE void ret_c(cpu *z) { if (z->f & FLAG_C) { _ret(z); } z->cycles += 2; }
INLINE void reti(cpu *z) { _ret(z); z->ints_enabled = 1; z->cycles += 2; }

INLINE void _rlc(cpu *z, u8 *r) { z->f = (*r>>3)&FLAG_C; *r = (u8)(*r<<1)|(*r>>7); if (!*r) z->f |= FLAG_Z; }
INLINE void rlc_b(cpu *z) { _rlc(z, &z->b); z->cycles += 2; }
INLINE void rlc_c(cpu *z) { _rlc(z, &z->c); z->cycles += 2; }
INLINE void rlc_d(cpu *z) { _rlc(z, &z->d); z->cycles += 2; }
INLINE void rlc_e(cpu *z) { _rlc(z, &z->e); z->cycles += 2; }
INLINE void rlc_h(cpu *z) { _rlc(z, &z->h); z->cycles += 2; }
INLINE void rlc_l(cpu *z) { _rlc(z, &z->l); z->cycles += 2; }
INLINE void rlc_hl(cpu *z) {
  u8 t = cpu_read(z, HL(z));
  z->f = (t>>3)&FLAG_C;
  u8 r = (u8)(t<<1)|(t>>7);
  cpu_write(z, HL(z), r);
  if (!r) z->f |= FLAG_Z;
  z->cycles += 4;
}
INLINE void rlc_a(cpu *z) { _rlc(z, &z->a); z->cycles += 2; }
INLINE void _rl(cpu *z, u8 *r) { u8 c = (z->f&FLAG_C)>>4; z->f = (*r>>3)&FLAG_C; *r = (u8)(*r<<1)|c; if (!*r) z->f |= FLAG_Z; }
INLINE void rl_b(cpu *z) { _rl(z, &z->b); z->cycles += 2; }
INLINE void rl_c(cpu *z) { _rl(z, &z->c); z->cycles += 2; }
INLINE void rl_d(cpu *z) { _rl(z, &z->d); z->cycles += 2; }
INLINE void rl_e(cpu *z) { _rl(z, &z->e); z->cycles += 2; }
INLINE void rl_h(cpu *z) { _rl(z, &z->h); z->cycles += 2; }
INLINE void rl_l(cpu *z) { _rl(z, &z->l); z->cycles += 2; }
INLINE void rl_hl(cpu *z) {
  u8 c = (z->f&FLAG_C)>>4;
  u8 t = cpu_read(z, HL(z));
  z->f = (t>>3)&FLAG_C;
  u8 r = (u8)(t<<1)|c;
  cpu_write(z, HL(z), r);
  if (!r) z->f |= FLAG_Z;
  z->cycles += 4;
}
INLINE void rl_a(cpu *z) { _rl(z, &z->a); z->cycles += 2; }
INLINE void _rrc(cpu *z, u8 *r) { z->f = (*r<<4)&FLAG_C; *r = (u8)(*r<<7)|(*r>>1); if (!*r) z->f |= FLAG_Z; }
INLINE void rrc_b(cpu *z) { _rrc(z, &z->b); z->cycles += 2; }
INLINE void rrc_c(cpu *z) { _rrc(z, &z->c); z->cycles += 2; }
INLINE void rrc_d(cpu *z) { _rrc(z, &z->d); z->cycles += 2; }
INLINE void rrc_e(cpu *z) { _rrc(z, &z->e); z->cycles += 2; }
INLINE void rrc_h(cpu *z) { _rrc(z, &z->h); z->cycles += 2; }
INLINE void rrc_l(cpu *z) { _rrc(z, &z->l); z->cycles += 2; }
INLINE void rrc_hl(cpu *z) {
  u8 t = cpu_read(z, HL(z));
  z->f = (t<<4)&FLAG_C;
  u8 r = (u8)(t<<7)|(t>>1);
  cpu_write(z, HL(z), r);
  if (!r) z->f |= FLAG_Z;
  z->cycles += 4;
}
INLINE void rrc_a(cpu *z) { _rrc(z, &z->a); z->cycles += 2; }
INLINE void _rr(cpu *z, u8 *r) { u8 c = (u8)((z->f&FLAG_C)<<3); z->f = (*r<<4)&FLAG_C; *r = c|(*r>>1); if (!*r) z->f |= FLAG_Z; }
INLINE void rr_b(cpu *z) { _rr(z, &z->b); z->cycles += 2; }
INLINE void rr_c(cpu *z) { _rr(z, &z->c); z->cycles += 2; }
INLINE void rr_d(cpu *z) { _rr(z, &z->d); z->cycles += 2; }
INLINE void rr_e(cpu *z) { _rr(z, &z->e); z->cycles += 2; }
INLINE void rr_h(cpu *z) { _rr(z, &z->h); z->cycles += 2; }
INLINE void rr_l(cpu *z) { _rr(z, &z->l); z->cycles += 2; }
INLINE void rr_hl(cpu *z) {
  u8 c = (u8)((z->f&FLAG_C)<<3);
  u8 t = cpu_read(z, HL(z));
  z->f = (t<<4)&FLAG_C;
  u8 r = c|(t>>1);
  cpu_write(z, HL(z), r);
  if (!r) z->f |= FLAG_Z;
  z->cycles += 4;
}
INLINE void rr_a(cpu *z) { _rr(z, &z->a); z->cycles += 2; }
INLINE void _sla(cpu *z, u8 *r) { z->f = (*r>>3)&FLAG_C; z->f |= (*r <<= 1)?0:FLAG_Z; }
INLINE void sla_b(cpu *z) { _sla(z, &z->b); z->cycles += 2; }
INLINE void sla_c(cpu *z) { _sla(z, &z->c); z->cycles += 2; }
INLINE void sla_d(cpu *z) { _sla(z, &z->d); z->cycles += 2; }
INLINE void sla_e(cpu *z) { _sla(z, &z->e); z->cycles += 2; }
INLINE void sla_h(cpu *z) { _sla(z, &z->h); z->cycles += 2; }
INLINE void sla_l(cpu *z) { _sla(z, &z->l); z->cycles += 2; }
INLINE void sla_hl(cpu *z) {
  u8 t = cpu_read(z, HL(z));
  z->f = (t>>3)&FLAG_C;
  t <<= 1;
  cpu_write(z, HL(z), t);
  z->f |= (t?0:FLAG_Z);
  z->cycles += 4;
}
INLINE void sla_a(cpu *z) { _sla(z, &z->a); z->cycles += 2; }
INLINE void _sra(cpu *z, u8 *r) { z->f = (*r<<4)&FLAG_C; z->f |= (*r = (u8)(((s8)*r)>>1))?0:FLAG_Z; }
INLINE void sra_b(cpu *z) { _sra(z, &z->b); z->cycles += 2; }
INLINE void sra_c(cpu *z) { _sra(z, &z->c); z->cycles += 2; }
INLINE void sra_d(cpu *z) { _sra(z, &z->d); z->cycles += 2; }
INLINE void sra_e(cpu *z) { _sra(z, &z->e); z->cycles += 2; }
INLINE void sra_h(cpu *z) { _sra(z, &z->h); z->cycles += 2; }
INLINE void sra_l(cpu *z) { _sra(z, &z->l); z->cycles += 2; }
INLINE void sra_hl(cpu *z) {
  u8 t = cpu_read(z, HL(z));
  z->f = (t<<4)&FLAG_C;
  t = (u8)(((s8)t)>>1);
  cpu_write(z, HL(z), t);
  z->f |= (t?0:FLAG_Z);
  z->cycles += 4;
}
INLINE void sra_a(cpu *z) { _sra(z, &z->a); z->cycles += 2; }
INLINE void swap_b(cpu *z) { z->f = (z->b = (u8)((z->b>>4)|(z->b<<4)))?0:FLAG_Z; z->cycles += 2; }
INLINE void swap_c(cpu *z) { z->f = (z->c = (u8)((z->c>>4)|(z->c<<4)))?0:FLAG_Z; z->cycles += 2; }
INLINE void swap_d(cpu *z) { z->f = (z->d = (u8)((z->d>>4)|(z->d<<4)))?0:FLAG_Z; z->cycles += 2; }
INLINE void swap_e(cpu *z) { z->f = (z->e = (u8)((z->e>>4)|(z->e<<4)))?0:FLAG_Z; z->cycles += 2; }
INLINE void swap_h(cpu *z) { z->f = (z->h = (u8)((z->h>>4)|(z->h<<4)))?0:FLAG_Z; z->cycles += 2; }
INLINE void swap_l(cpu *z) { z->f = (z->l = (u8)((z->l>>4)|(z->l<<4)))?0:FLAG_Z; z->cycles += 2; }
INLINE void swap_hl(cpu *z) {
  u8 t = cpu_read(z, HL(z));
  t = (u8)((t>>4)|(t<<4));
  cpu_write(z, HL(z), t);
  z->f = (t?0:FLAG_Z);
  z->cycles += 4;
}
INLINE void swap_a(cpu *z) { z->f = (z->a = (u8)((z->a>>4)|(z->a<<4)))?0:FLAG_Z; z->cycles += 2; }
INLINE void _srl(cpu *z, u8 *r) { z->f = (*r<<4)&FLAG_C; z->f |= (*r >>= 1)?0:FLAG_Z; }
INLINE void srl_b(cpu *z) { _srl(z, &z->b); z->cycles += 2; }
INLINE void srl_c(cpu *z) { _srl(z, &z->c); z->cycles += 2; }
INLINE void srl_d(cpu *z) { _srl(z, &z->d); z->cycles += 2; }
INLINE void srl_e(cpu *z) { _srl(z, &z->e); z->cycles += 2; }
INLINE void srl_h(cpu *z) { _srl(z, &z->h); z->cycles += 2; }
INLINE void srl_l(cpu *z) { _srl(z, &z->l); z->cycles += 2; }
INLINE void srl_hl(cpu *z) {
  u8 t = cpu_read(z, HL(z));
  z->f = (t<<4)&FLAG_C;
  t >>= 1;
  cpu_write(z, HL(z), t);
  z->f |= (t?0:FLAG_Z);
  z->cycles += 4;
}
INLINE void srl_a(cpu *z) { _srl(z, &z->a); z->cycles += 2; }
INLINE void _bit(cpu *z, u8 r, u8 b) { z->f = (u8)(~((r<<(7-b))|~FLAG_Z)) | FLAG_H | (z->f&FLAG_C); }
INLINE void bit_b(cpu *z, u8 b) { _bit(z, z->b, b); z->cycles += 2; }
INLINE void bit_c(cpu *z, u8 b) { _bit(z, z->c, b); z->cycles += 2; }
INLINE void bit_d(cpu *z, u8 b) { _bit(z, z->d, b); z->cycles += 2; }
INLINE void bit_e(cpu *z, u8 b) { _bit(z, z->e, b); z->cycles += 2; }
INLINE void bit_h(cpu *z, u8 b) { _bit(z, z->h, b); z->cycles += 2; }
INLINE void bit_l(cpu *z, u8 b) { _bit(z, z->l, b); z->cycles += 2; }
INLINE void bit_hl(cpu *z, u8 b) { _bit(z, cpu_read(z, HL(z)), b); z->cycles += 4; }
INLINE void bit_a(cpu *z, u8 b) { _bit(z, z->a, b); z->cycles += 2; }
INLINE void _res(u8 *r, u8 b) { *r &= ~(1<<b); }
INLINE void res_b(cpu *z, u8 b) { _res(&z->b, b); z->cycles += 2; }
INLINE void res_c(cpu *z, u8 b) { _res(&z->c, b); z->cycles += 2; }
INLINE void res_d(cpu *z, u8 b) { _res(&z->d, b); z->cycles += 2; }
INLINE void res_e(cpu *z, u8 b) { _res(&z->e, b); z->cycles += 2; }
INLINE void res_h(cpu *z, u8 b) { _res(&z->h, b); z->cycles += 2; }
INLINE void res_l(cpu *z, u8 b) { _res(&z->l, b); z->cycles += 2; }
INLINE void res_hl(cpu *z, u8 b) { cpu_write(z, HL(z), cpu_read(z, HL(z)) & ~(1<<b)); z->cycles += 4; }
INLINE void res_a(cpu *z, u8 b) { _res(&z->a, b); z->cycles += 2; }
INLINE void _set(u8 *r, u8 b) { *r |= (1<<b); }
INLINE void set_b(cpu *z, u8 b) { _set(&z->b, b); z->cycles += 2; }
INLINE void set_c(cpu *z, u8 b) { _set(&z->c, b); z->cycles += 2; }
INLINE void set_d(cpu *z, u8 b) { _set(&z->d, b); z->cycles += 2; }
INLINE void set_e(cpu *z, u8 b) { _set(&z->e, b); z->cycles += 2; }
INLINE void set_h(cpu *z, u8 b) { _set(&z->h, b); z->cycles += 2; }
INLINE void set_l(cpu *z, u8 b) { _set(&z->l, b); z->cycles += 2; }
INLINE void set_hl(cpu *z, u8 b) { cpu_write(z, HL(z), cpu_read(z, HL(z)) | (u8)(1<<b)); z->cycles += 4; }
INLINE void set_a(cpu *z, u8 b) { _set(&z->a, b); z->cycles += 2; }

INLINE void cb(cpu *z) {
  u8 ins = cpu_read(z, z->pc++);

  switch (ins) {
    case 0x00: rlc_b(z); break;
    case 0x01: rlc_c(z); break;
    case 0x02: rlc_d(z); break;
    case 0x03: rlc_e(z); break;
    case 0x04: rlc_h(z); break;
    case 0x05: rlc_l(z); break;
    case 0x06: rlc_hl(z); break;
    case 0x07: rlc_a(z); break;
    case 0x08: rrc_b(z); break;
    case 0x09: rrc_c(z); break;
    case 0x0a: rrc_d(z); break;
    case 0x0b: rrc_e(z); break;
    case 0x0c: rrc_h(z); break;
    case 0x0d: rrc_l(z); break;
    case 0x0e: rrc_hl(z); break;
    case 0x0f: rrc_a(z); break;
    case 0x10: rl_b(z); break;
    case 0x11: rl_c(z); break;
    case 0x12: rl_d(z); break;
    case 0x13: rl_e(z); break;
    case 0x14: rl_h(z); break;
    case 0x15: rl_l(z); break;
    case 0x16: rl_hl(z); break;
    case 0x17: rl_a(z); break;
    case 0x18: rr_b(z); break;
    case 0x19: rr_c(z); break;
    case 0x1a: rr_d(z); break;
    case 0x1b: rr_e(z); break;
    case 0x1c: rr_h(z); break;
    case 0x1d: rr_l(z); break;
    case 0x1e: rr_hl(z); break;
    case 0x1f: rr_a(z); break;
    case 0x20: sla_b(z); break;
    case 0x21: sla_c(z); break;
    case 0x22: sla_d(z); break;
    case 0x23: sla_e(z); break;
    case 0x24: sla_h(z); break;
    case 0x25: sla_l(z); break;
    case 0x26: sla_hl(z); break;
    case 0x27: sla_a(z); break;
    case 0x28: sra_b(z); break;
    case 0x29: sra_c(z); break;
    case 0x2a: sra_d(z); break;
    case 0x2b: sra_e(z); break;
    case 0x2c: sra_h(z); break;
    case 0x2d: sra_l(z); break;
    case 0x2e: sra_hl(z); break;
    case 0x2f: sra_a(z); break;
    case 0x30: swap_b(z); break;
    case 0x31: swap_c(z); break;
    case 0x32: swap_d(z); break;
    case 0x33: swap_e(z); break;
    case 0x34: swap_h(z); break;
    case 0x35: swap_l(z); break;
    case 0x36: swap_hl(z); break;
    case 0x37: swap_a(z); break;
    case 0x38: srl_b(z); break;
    case 0x39: srl_c(z); break;
    case 0x3a: srl_d(z); break;
    case 0x3b: srl_e(z); break;
    case 0x3c: srl_h(z); break;
    case 0x3d: srl_l(z); break;
    case 0x3e: srl_hl(z); break;
    case 0x3f: srl_a(z); break;
    case 0x40: bit_b(z, 0); break;
    case 0x41: bit_c(z, 0); break;
    case 0x42: bit_d(z, 0); break;
    case 0x43: bit_e(z, 0); break;
    case 0x44: bit_h(z, 0); break;
    case 0x45: bit_l(z, 0); break;
    case 0x46: bit_hl(z, 0); break;
    case 0x47: bit_a(z, 0); break;
    case 0x48: bit_b(z, 1); break;
    case 0x49: bit_c(z, 1); break;
    case 0x4a: bit_d(z, 1); break;
    case 0x4b: bit_e(z, 1); break;
    case 0x4c: bit_h(z, 1); break;
    case 0x4d: bit_l(z, 1); break;
    case 0x4e: bit_hl(z, 1); break;
    case 0x4f: bit_a(z, 1); break;
    case 0x50: bit_b(z, 2); break;
    case 0x51: bit_c(z, 2); break;
    case 0x52: bit_d(z, 2); break;
    case 0x53: bit_e(z, 2); break;
    case 0x54: bit_h(z, 2); break;
    case 0x55: bit_l(z, 2); break;
    case 0x56: bit_hl(z, 2); break;
    case 0x57: bit_a(z, 2); break;
    case 0x58: bit_b(z, 3); break;
    case 0x59: bit_c(z, 3); break;
    case 0x5a: bit_d(z, 3); break;
    case 0x5b: bit_e(z, 3); break;
    case 0x5c: bit_h(z, 3); break;
    case 0x5d: bit_l(z, 3); break;
    case 0x5e: bit_hl(z, 3); break;
    case 0x5f: bit_a(z, 3); break;
    case 0x60: bit_b(z, 4); break;
    case 0x61: bit_c(z, 4); break;
    case 0x62: bit_d(z, 4); break;
    case 0x63: bit_e(z, 4); break;
    case 0x64: bit_h(z, 4); break;
    case 0x65: bit_l(z, 4); break;
    case 0x66: bit_hl(z, 4); break;
    case 0x67: bit_a(z, 4); break;
    case 0x68: bit_b(z, 5); break;
    case 0x69: bit_c(z, 5); break;
    case 0x6a: bit_d(z, 5); break;
    case 0x6b: bit_e(z, 5); break;
    case 0x6c: bit_h(z, 5); break;
    case 0x6d: bit_l(z, 5); break;
    case 0x6e: bit_hl(z, 5); break;
    case 0x6f: bit_a(z, 5); break;
    case 0x70: bit_b(z, 6); break;
    case 0x71: bit_c(z, 6); break;
    case 0x72: bit_d(z, 6); break;
    case 0x73: bit_e(z, 6); break;
    case 0x74: bit_h(z, 6); break;
    case 0x75: bit_l(z, 6); break;
    case 0x76: bit_hl(z, 6); break;
    case 0x77: bit_a(z, 6); break;
    case 0x78: bit_b(z, 7); break;
    case 0x79: bit_c(z, 7); break;
    case 0x7a: bit_d(z, 7); break;
    case 0x7b: bit_e(z, 7); break;
    case 0x7c: bit_h(z, 7); break;
    case 0x7d: bit_l(z, 7); break;
    case 0x7e: bit_hl(z, 7); break;
    case 0x7f: bit_a(z, 7); break;
    case 0x80: res_b(z, 0); break;
    case 0x81: res_c(z, 0); break;
    case 0x82: res_d(z, 0); break;
    case 0x83: res_e(z, 0); break;
    case 0x84: res_h(z, 0); break;
    case 0x85: res_l(z, 0); break;
    case 0x86: res_hl(z, 0); break;
    case 0x87: res_a(z, 0); break;
    case 0x88: res_b(z, 1); break;
    case 0x89: res_c(z, 1); break;
    case 0x8a: res_d(z, 1); break;
    case 0x8b: res_e(z, 1); break;
    case 0x8c: res_h(z, 1); break;
    case 0x8d: res_l(z, 1); break;
    case 0x8e: res_hl(z, 1); break;
    case 0x8f: res_a(z, 1); break;
    case 0x90: res_b(z, 2); break;
    case 0x91: res_c(z, 2); break;
    case 0x92: res_d(z, 2); break;
    case 0x93: res_e(z, 2); break;
    case 0x94: res_h(z, 2); break;
    case 0x95: res_l(z, 2); break;
    case 0x96: res_hl(z, 2); break;
    case 0x97: res_a(z, 2); break;
    case 0x98: res_b(z, 3); break;
    case 0x99: res_c(z, 3); break;
    case 0x9a: res_d(z, 3); break;
    case 0x9b: res_e(z, 3); break;
    case 0x9c: res_h(z, 3); break;
    case 0x9d: res_l(z, 3); break;
    case 0x9e: res_hl(z, 3); break;
    case 0x9f: res_a(z, 3); break;
    case 0xa0: res_b(z, 4); break;
    case 0xa1: res_c(z, 4); break;
    case 0xa2: res_d(z, 4); break;
    case 0xa3: res_e(z, 4); break;
    case 0xa4: res_h(z, 4); break;
    case 0xa5: res_l(z, 4); break;
    case 0xa6: res_hl(z, 4); break;
    case 0xa7: res_a(z, 4); break;
    case 0xa8: res_b(z, 5); break;
    case 0xa9: res_c(z, 5); break;
    case 0xaa: res_d(z, 5); break;
    case 0xab: res_e(z, 5); break;
    case 0xac: res_h(z, 5); break;
    case 0xad: res_l(z, 5); break;
    case 0xae: res_hl(z, 5); break;
    case 0xaf: res_a(z, 5); break;
    case 0xb0: res_b(z, 6); break;
    case 0xb1: res_c(z, 6); break;
    case 0xb2: res_d(z, 6); break;
    case 0xb3: res_e(z, 6); break;
    case 0xb4: res_h(z, 6); break;
    case 0xb5: res_l(z, 6); break;
    case 0xb6: res_hl(z, 6); break;
    case 0xb7: res_a(z, 6); break;
    case 0xb8: res_b(z, 7); break;
    case 0xb9: res_c(z, 7); break;
    case 0xba: res_d(z, 7); break;
    case 0xbb: res_e(z, 7); break;
    case 0xbc: res_h(z, 7); break;
    case 0xbd: res_l(z, 7); break;
    case 0xbe: res_hl(z, 7); break;
    case 0xbf: res_a(z, 7); break;
    case 0xc0: set_b(z, 0); break;
    case 0xc1: set_c(z, 0); break;
    case 0xc2: set_d(z, 0); break;
    case 0xc3: set_e(z, 0); break;
    case 0xc4: set_h(z, 0); break;
    case 0xc5: set_l(z, 0); break;
    case 0xc6: set_hl(z, 0); break;
    case 0xc7: set_a(z, 0); break;
    case 0xc8: set_b(z, 1); break;
    case 0xc9: set_c(z, 1); break;
    case 0xca: set_d(z, 1); break;
    case 0xcb: set_e(z, 1); break;
    case 0xcc: set_h(z, 1); break;
    case 0xcd: set_l(z, 1); break;
    case 0xce: set_hl(z, 1); break;
    case 0xcf: set_a(z, 1); break;
    case 0xd0: set_b(z, 2); break;
    case 0xd1: set_c(z, 2); break;
    case 0xd2: set_d(z, 2); break;
    case 0xd3: set_e(z, 2); break;
    case 0xd4: set_h(z, 2); break;
    case 0xd5: set_l(z, 2); break;
    case 0xd6: set_hl(z, 2); break;
    case 0xd7: set_a(z, 2); break;
    case 0xd8: set_b(z, 3); break;
    case 0xd9: set_c(z, 3); break;
    case 0xda: set_d(z, 3); break;
    case 0xdb: set_e(z, 3); break;
    case 0xdc: set_h(z, 3); break;
    case 0xdd: set_l(z, 3); break;
    case 0xde: set_hl(z, 3); break;
    case 0xdf: set_a(z, 3); break;
    case 0xe0: set_b(z, 4); break;
    case 0xe1: set_c(z, 4); break;
    case 0xe2: set_d(z, 4); break;
    case 0xe3: set_e(z, 4); break;
    case 0xe4: set_h(z, 4); break;
    case 0xe5: set_l(z, 4); break;
    case 0xe6: set_hl(z, 4); break;
    case 0xe7: set_a(z, 4); break;
    case 0xe8: set_b(z, 5); break;
    case 0xe9: set_c(z, 5); break;
    case 0xea: set_d(z, 5); break;
    case 0xeb: set_e(z, 5); break;
    case 0xec: set_h(z, 5); break;
    case 0xed: set_l(z, 5); break;
    case 0xee: set_hl(z, 5); break;
    case 0xef: set_a(z, 5); break;
    case 0xf0: set_b(z, 6); break;
    case 0xf1: set_c(z, 6); break;
    case 0xf2: set_d(z, 6); break;
    case 0xf3: set_e(z, 6); break;
    case 0xf4: set_h(z, 6); break;
    case 0xf5: set_l(z, 6); break;
    case 0xf6: set_hl(z, 6); break;
    case 0xf7: set_a(z, 6); break;
    case 0xf8: set_b(z, 7); break;
    case 0xf9: set_c(z, 7); break;
    case 0xfa: set_d(z, 7); break;
    case 0xfb: set_e(z, 7); break;
    case 0xfc: set_h(z, 7); break;
    case 0xfd: set_l(z, 7); break;
    case 0xfe: set_hl(z, 7); break;
    case 0xff: set_a(z, 7); break;
  }
}

static void cpu_execute(cpu *z) {
  u8 ins = cpu_read(z, z->pc++);

  switch (ins) {
    case 0x00: nop(z); break;
    case 0x01: ld_bc_nn(z); break;
    case 0x02: ld_bc_a(z); break;
    case 0x03: inc_bc(z); break;
    case 0x04: inc_b(z); break;
    case 0x05: dec_b(z); break;
    case 0x06: ld_b_n(z); break;
    case 0x07: rlca(z); break;
    case 0x08: ld_nn_sp(z); break;
    case 0x09: add_hl_bc(z); break;
    case 0x0a: ld_a_bc(z); break;
    case 0x0b: dec_bc(z); break;
    case 0x0c: inc_c(z); break;
    case 0x0d: dec_c(z); break;
    case 0x0e: ld_c_n(z); break;
    case 0x0f: rrca(z); break;
    case 0x10: stop(z); break;
    case 0x11: ld_de_nn(z); break;
    case 0x12: ld_de_a(z); break;
    case 0x13: inc_de(z); break;
    case 0x14: inc_d(z); break;
    case 0x15: dec_d(z); break;
    case 0x16: ld_d_n(z); break;
    case 0x17: rla(z); break;
    case 0x18: jr(z); break;
    case 0x19: add_hl_de(z); break;
    case 0x1a: ld_a_de(z); break;
    case 0x1b: dec_de(z); break;
    case 0x1c: inc_e(z); break;
    case 0x1d: dec_e(z); break;
    case 0x1e: ld_e_n(z); break;
    case 0x1f: rra(z); break;
    case 0x20: jr_nz(z); break;
    case 0x21: ld_hl_nn(z); break;
    case 0x22: ldi_hl_a(z); break;
    case 0x23: inc_rhl(z); break;
    case 0x24: inc_h(z); break;
    case 0x25: dec_h(z); break;
    case 0x26: ld_h_n(z); break;
    case 0x27: daa(z); break;
    case 0x28: jr_z(z); break;
    case 0x29: add_hl_hl(z); break;
    case 0x2a: ldi_a_hl(z); break;
    case 0x2b: dec_rhl(z); break;
    case 0x2c: inc_l(z); break;
    case 0x2d: dec_l(z); break;
    case 0x2e: ld_l_n(z); break;
    case 0x2f: cpl(z); break;
    case 0x30: jr_nc(z); break;
    case 0x31: ld_sp_nn(z); break;
    case 0x32: ldd_hl_a(z); break;
    case 0x33: inc_sp(z); break;
    case 0x34: inc_hl(z); break;
    case 0x35: dec_hl(z); break;
    case 0x36: ld_hl_n(z); break;
    case 0x37: scf(z); break;
    case 0x38: jr_c(z); break;
    case 0x39: add_hl_sp(z); break;
    case 0x3a: ldd_a_hl(z); break;
    case 0x3b: dec_sp(z); break;
    case 0x3c: inc_a(z); break;
    case 0x3d: dec_a(z); break;
    case 0x3e: ld_a_n(z); break;
    case 0x3f: ccf(z); break;
    case 0x40: ld_b_b(z); break;
    case 0x41: ld_b_c(z); break;
    case 0x42: ld_b_d(z); break;
    case 0x43: ld_b_e(z); break;
    case 0x44: ld_b_h(z); break;
    case 0x45: ld_b_l(z); break;
    case 0x46: ld_b_hl(z); break;
    case 0x47: ld_b_a(z); break;
    case 0x48: ld_c_b(z); break;
    case 0x49: ld_c_c(z); break;
    case 0x4a: ld_c_d(z); break;
    case 0x4b: ld_c_e(z); break;
    case 0x4c: ld_c_h(z); break;
    case 0x4d: ld_c_l(z); break;
    case 0x4e: ld_c_hl(z); break;
    case 0x4f: ld_c_a(z); break;
    case 0x50: ld_d_b(z); break;
    case 0x51: ld_d_c(z); break;
    case 0x52: ld_d_d(z); break;
    case 0x53: ld_d_e(z); break;
    case 0x54: ld_d_h(z); break;
    case 0x55: ld_d_l(z); break;
    case 0x56: ld_d_hl(z); break;
    case 0x57: ld_d_a(z); break;
    case 0x58: ld_e_b(z); break;
    case 0x59: ld_e_c(z); break;
    case 0x5a: ld_e_d(z); break;
    case 0x5b: ld_e_e(z); break;
    case 0x5c: ld_e_h(z); break;
    case 0x5d: ld_e_l(z); break;
    case 0x5e: ld_e_hl(z); break;
    case 0x5f: ld_e_a(z); break;
    case 0x60: ld_h_b(z); break;
    case 0x61: ld_h_c(z); break;
    case 0x62: ld_h_d(z); break;
    case 0x63: ld_h_e(z); break;
    case 0x64: ld_h_h(z); break;
    case 0x65: ld_h_l(z); break;
    case 0x66: ld_h_hl(z); break;
    case 0x67: ld_h_a(z); break;
    case 0x68: ld_l_b(z); break;
    case 0x69: ld_l_c(z); break;
    case 0x6a: ld_l_d(z); break;
    case 0x6b: ld_l_e(z); break;
    case 0x6c: ld_l_h(z); break;
    case 0x6d: ld_l_l(z); break;
    case 0x6e: ld_l_hl(z); break;
    case 0x6f: ld_l_a(z); break;
    case 0x70: ld_hl_b(z); break;
    case 0x71: ld_hl_c(z); break;
    case 0x72: ld_hl_d(z); break;
    case 0x73: ld_hl_e(z); break;
    case 0x74: ld_hl_h(z); break;
    case 0x75: ld_hl_l(z); break;
    case 0x76: halt(z); break;
    case 0x77: ld_hl_a(z); break;
    case 0x78: ld_a_b(z); break;
    case 0x79: ld_a_c(z); break;
    case 0x7a: ld_a_d(z); break;
    case 0x7b: ld_a_e(z); break;
    case 0x7c: ld_a_h(z); break;
    case 0x7d: ld_a_l(z); break;
    case 0x7e: ld_a_hl(z); break;
    case 0x7f: ld_a_a(z); break;
    case 0x80: add_a_b(z); break;
    case 0x81: add_a_c(z); break;
    case 0x82: add_a_d(z); break;
    case 0x83: add_a_e(z); break;
    case 0x84: add_a_h(z); break;
    case 0x85: add_a_l(z); break;
    case 0x86: add_a_hl(z); break;
    case 0x87: add_a_a(z); break;
    case 0x88: adc_a_b(z); break;
    case 0x89: adc_a_c(z); break;
    case 0x8a: adc_a_d(z); break;
    case 0x8b: adc_a_e(z); break;
    case 0x8c: adc_a_h(z); break;
    case 0x8d: adc_a_l(z); break;
    case 0x8e: adc_a_hl(z); break;
    case 0x8f: adc_a_a(z); break;
    case 0x90: sub_a_b(z); break;
    case 0x91: sub_a_c(z); break;
    case 0x92: sub_a_d(z); break;
    case 0x93: sub_a_e(z); break;
    case 0x94: sub_a_h(z); break;
    case 0x95: sub_a_l(z); break;
    case 0x96: sub_a_hl(z); break;
    case 0x97: sub_a_a(z); break;
    case 0x98: sbc_a_b(z); break;
    case 0x99: sbc_a_c(z); break;
    case 0x9a: sbc_a_d(z); break;
    case 0x9b: sbc_a_e(z); break;
    case 0x9c: sbc_a_h(z); break;
    case 0x9d: sbc_a_l(z); break;
    case 0x9e: sbc_a_hl(z); break;
    case 0x9f: sbc_a_a(z); break;
    case 0xa0: and_a_b(z); break;
    case 0xa1: and_a_c(z); break;
    case 0xa2: and_a_d(z); break;
    case 0xa3: and_a_e(z); break;
    case 0xa4: and_a_h(z); break;
    case 0xa5: and_a_l(z); break;
    case 0xa6: and_a_hl(z); break;
    case 0xa7: and_a_a(z); break;
    case 0xa8: xor_a_b(z); break;
    case 0xa9: xor_a_c(z); break;
    case 0xaa: xor_a_d(z); break;
    case 0xab: xor_a_e(z); break;
    case 0xac: xor_a_h(z); break;
    case 0xad: xor_a_l(z); break;
    case 0xae: xor_a_hl(z); break;
    case 0xaf: xor_a_a(z); break;
    case 0xb0: or_a_b(z); break;
    case 0xb1: or_a_c(z); break;
    case 0xb2: or_a_d(z); break;
    case 0xb3: or_a_e(z); break;
    case 0xb4: or_a_h(z); break;
    case 0xb5: or_a_l(z); break;
    case 0xb6: or_a_hl(z); break;
    case 0xb7: or_a_a(z); break;
    case 0xb8: cp_a_b(z); break;
    case 0xb9: cp_a_c(z); break;
    case 0xba: cp_a_d(z); break;
    case 0xbb: cp_a_e(z); break;
    case 0xbc: cp_a_h(z); break;
    case 0xbd: cp_a_l(z); break;
    case 0xbe: cp_a_hl(z); break;
    case 0xbf: cp_a_a(z); break;
    case 0xc0: ret_nz(z); break;
    case 0xc1: pop_bc(z); break;
    case 0xc2: jp_nz(z); break;
    case 0xc3: jp(z); break;
    case 0xc4: call_nz(z); break;
    case 0xc5: push_bc(z); break;
    case 0xc6: add_a_n(z); break;
    case 0xc7: rst(z, 0x00); break;
    case 0xc8: ret_z(z); break;
    case 0xc9: ret(z); break;
    case 0xca: jp_z(z); break;
    case 0xcb: cb(z); break;
    case 0xcc: call_z(z); break;
    case 0xcd: call(z); break;
    case 0xce: adc_a_n(z); break;
    case 0xcf: rst(z, 0x08); break;
    case 0xd0: ret_nc(z); break;
    case 0xd1: pop_de(z); break;
    case 0xd2: jp_nc(z); break;
    case 0xd3: nop(z); break; /**/
    case 0xd4: call_nc(z); break;
    case 0xd5: push_de(z); break;
    case 0xd6: sub_a_n(z); break;
    case 0xd7: rst(z, 0x10); break;
    case 0xd8: ret_c(z); break;
    case 0xd9: reti(z); break;
    case 0xda: jp_c(z); break;
    case 0xdb: nop(z); break; /**/
    case 0xdc: call_c(z); break;
    case 0xdd: nop(z); break; /**/
    case 0xde: sbc_a_n(z); break;
    case 0xdf: rst(z, 0x18); break;
    case 0xe0: ldh_n_a(z); break;
    case 0xe1: pop_hl(z); break;
    case 0xe2: ldh_c_a(z); break;
    case 0xe3: nop(z); break; /**/
    case 0xe4: nop(z); break; /**/
    case 0xe5: push_hl(z); break;
    case 0xe6: and_a_n(z); break;
    case 0xe7: rst(z, 0x20); break;
    case 0xe8: add_sp_n(z); break;
    case 0xe9: jp_hl(z); break;
    case 0xea: ld_nn_a(z); break;
    case 0xeb: nop(z); break; /**/
    case 0xec: nop(z); break; /**/
    case 0xed: nop(z); break; /**/
    case 0xee: xor_a_n(z); break;
    case 0xef: rst(z, 0x28); break;
    case 0xf0: ldh_a_n(z); break;
    case 0xf1: pop_af(z); break;
    case 0xf2: ldh_a_c(z); break;
    case 0xf3: di(z); break;
    case 0xf4: nop(z); break; /**/
    case 0xf5: push_af(z); break;
    case 0xf6: or_a_n(z); break;
    case 0xf7: rst(z, 0x30); break;
    case 0xf8: ldhl_sp_n(z); break;
    case 0xf9: ld_sp_hl(z); break;
    case 0xfa: ld_a_nn(z); break;
    case 0xfb: ei(z); break;
    case 0xfc: nop(z); break; /**/
    case 0xfd: nop(z); break; /**/
    case 0xfe: cp_a_n(z); break;
    case 0xff: rst(z, 0x38); break;
  }
}

static void cpu_run_timers(cpu *z) {
  // CPU: 2^20 cycles

  // DIV: 2^14 (16384) Hz = every 2^6 cycles
  // TODO this only works if <64 cycles have elapsed since last call
  if (z->cycles_prev % 64 > z->cycles % 64) {
    cpu_write(z, REG_DIV, cpu_read(z, REG_DIV) + 1);
  }

  // TIMA: check TAC b2==1 (timer enable), then check TAC b1b0
  // 00: 2^12 (4096) Hz = every 2^8 cycles
  // 01: 2^18 (262144) Hz = every 2^2 cycles
  // 10: 2^16 (65536) Hz = every 2^4 cycles
  // 11: 2^14 (16384) Hz = every 2^6 cycles
  u8 tac = cpu_read(z, REG_TAC);
  if (tac & TAC_ENABLE) {
    u32 divisors[4] = {256, 4, 16, 64};
    u32 divisor = divisors[tac & TAC_CLOCK_SELECT];

    u32 counter = z->cycles_prev;
    if (counter % divisor != 0) {
      counter += divisor - (counter % divisor);
    }
    for (; counter < z->cycles; counter += divisor) {
      u8 tima = cpu_read(z, REG_TIMA) + 1;
      if (tima == 0) {
        cpu_write(z, REG_TIMA, cpu_read(z, REG_TMA));
        cpu_write(z, REG_IF, cpu_read(z, REG_IF)|INT_TIMER);
      } else {
        cpu_write(z, REG_TIMA, tima);
      }
    }
  }

  z->cycles_prev = z->cycles;
}

static u8 cpu_handle_ints(cpu *z) {
  u8 masked = cpu_read(z, REG_IF) & cpu_read(z, REG_IE);
  if (masked) {
    // always take cpu out of halted even if interrupts are disabled
    z->halted = 0;

    if (z->ints_enabled) {
      cpu_write(z, --z->sp, (u8)(z->pc>>8)); cpu_write(z, --z->sp, (u8)z->pc);
      if (masked & INT_VBLANK) {
        cpu_write(z, REG_IF, cpu_read(z, REG_IF)&~INT_VBLANK);
        z->pc = 0x0040;
      } else if (masked & INT_LCDC_STAT) {
        cpu_write(z, REG_IF, cpu_read(z, REG_IF)&~INT_LCDC_STAT);
        z->pc = 0x0048;
      } else if (masked & INT_TIMER) {
        cpu_write(z, REG_IF, cpu_read(z, REG_IF)&~INT_TIMER);
        z->pc = 0x0050;
      } else if (masked & INT_SERIAL) {
        cpu_write(z, REG_IF, cpu_read(z, REG_IF)&~INT_SERIAL);
        z->pc = 0x0058;
      } else if (masked & INT_BUTTON) {
        cpu_write(z, REG_IF, cpu_read(z, REG_IF)&~INT_BUTTON);
        z->pc = 0x0060;
      }

      z->ints_enabled = 0;
      // https://gbdev.gg8.se/wiki/articles/Interrupts#Interrupt_Service_Routine
      z->cycles += 5;
      return 1;
    }
  }
  return 0;
}

void cpu_step(cpu *z) {
  cpu_run_timers(z);
  if (cpu_handle_ints(z)) {
    return;
  }

  if (z->halted) {
    z->cycles++;
    return;
  }
  cpu_execute(z);
}
