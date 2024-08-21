#include "z80.h"
#include "video.h"

// base_addr: 0x8000 or 0x9000
// tile_id: 0-255
// y: line
// return: big-endian data for sprite
u16 tile_data(cpu *z, u16 base_addr, u8 tile_id, u8 y) {
  if (base_addr == 0x9000 && tile_id >= 0x80) {
    base_addr = 0x8000;
  }
  base_addr -= 0x8000;
  base_addr += ((u16)(tile_id))*0x10;
  base_addr += y;
  base_addr += y;

  u8 b1 = z->vram[base_addr];
  u8 b2 = z->vram[base_addr+1];

  u16 out = 0;
  for (int i = 0; i < 8; i++) {
    out <<= 1;
    out |= (b2>>(7-i))&0x01;
    out <<=1;
    out |= (b1>>(7-i))&0x01;
  }
  return out;
}
