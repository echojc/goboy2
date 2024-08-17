#ifndef __Z80_H__
#define __Z80_H__

#include "types.h"

#define FLAG_Z (1<<7)
#define FLAG_N (1<<6)
#define FLAG_H (1<<5)
#define FLAG_C (1<<4)

typedef struct {
  u8 b, c, d, e, h, l, a, f;
  u16 sp, pc;
  u8 *rom;         // 0x0000-0x3fff
  u8 *xrom;        // 0x4000-0x7fff
  u8 vram[0x2000]; // 0x8000-0x9fff
  u8 *xram;        // 0xa000-0xbfff
  u8 ram[0x2000];  // 0xc000-0xdfff mirror 0xe000-0xfdff
  u8 hram[0x200];  // 0xfe00-0xffff
  u32 cycles, cycles_prev; // ok to overflow; clock rate is power of 2

  // cpu states
  u8 halted, stopped, ints_enabled;
} cpu;

void cpu_init(cpu *z);
void cpu_step(cpu *z);
u8 cpu_read(cpu *z, u16 addr);
void cpu_write(cpu *z, u16 addr, u8 byte);

#define REG_DIV  (0xff04)
#define REG_TIMA (0xff05)
#define REG_TMA  (0xff06)
#define REG_TAC  (0xff07)
#define REG_IF   (0xff0f)
#define REG_IE   (0xffff)

#define TAC_ENABLE       (1<<2)
#define TAC_CLOCK_SELECT (0x03)

#define INT_BUTTON       (1<<4)
#define INT_SERIAL       (1<<3)
#define INT_TIMER        (1<<2)
#define INT_LCDC_STAT    (1<<1)
#define INT_VBLANK       (1<<0)

#endif
