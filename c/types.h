#ifndef __TYPES_H__
#define __TYPES_H__

#define u8 unsigned char
#define u16 unsigned short
#define u32 unsigned int
#define u64 unsigned long
#define s8 signed char
#define s16 signed short
#define s32 signed int
#define s64 signed long

#define INLINE __attribute__((always_inline)) inline static

// check for arithmetic >>
#define HAS_SRA(T) ((T)(-1)>>1==(T)(-1))

#endif
