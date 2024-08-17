package main

import (
	"fmt"
)

import "C"

func isCall(ins byte) bool {
	return ins == 0xc4 || ins == 0xcc || ins == 0xcd || ins == 0xd4 || ins == 0xdc
}

func decode(ins []byte) (string, int) {
	var count int = 1
	var str string

	switch ins[0] {
	case 0x00:
		str = "nop"
	case 0x01:
		count = 3
		str = fmt.Sprintf("ld bc, $%02x%02x", ins[2], ins[1])
	case 0x02:
		str = "ld (bc), a"
	case 0x03:
		str = "inc bc"
	case 0x04:
		str = "inc b"
	case 0x05:
		str = "dec b"
	case 0x06:
		count = 2
		str = fmt.Sprintf("ld b, $%02x", ins[1])
	case 0x07:
		str = "rlca"
	case 0x08:
		count = 3
		str = fmt.Sprintf("ld ($%02x%02x), sp", ins[2], ins[1])
	case 0x09:
		str = "add hl, bc"
	case 0x0a:
		str = "ld a, (bc)"
	case 0x0b:
		str = "dec bc"
	case 0x0c:
		str = "inc c"
	case 0x0d:
		str = "dec c"
	case 0x0e:
		count = 2
		str = fmt.Sprintf("ld c, $%02x", ins[1])
	case 0x0f:
		str = "rrca"
	case 0x10:
		str = "stop"
	case 0x11:
		count = 3
		str = fmt.Sprintf("ld de, $%02x%02x", ins[2], ins[1])
	case 0x12:
		str = "ld (de), a"
	case 0x13:
		str = "inc de"
	case 0x14:
		str = "inc d"
	case 0x15:
		str = "dec d"
	case 0x16:
		count = 2
		str = fmt.Sprintf("ld d, $%02x", ins[1])
	case 0x17:
		str = "rla"
	case 0x18:
		count = 2
		str = fmt.Sprintf("jr $%02x", ins[1])
	case 0x19:
		str = "add hl, de"
	case 0x1a:
		str = "ld a, (de)"
	case 0x1b:
		str = "dec de"
	case 0x1c:
		str = "inc e"
	case 0x1d:
		str = "dec e"
	case 0x1e:
		count = 2
		str = fmt.Sprintf("ld e, $%02x", ins[1])
	case 0x1f:
		str = "rra"
	case 0x20:
		count = 2
		str = fmt.Sprintf("jr nz, $%02x", ins[1])
	case 0x21:
		count = 3
		str = fmt.Sprintf("ld hl, $%02x%02x", ins[2], ins[1])
	case 0x22:
		str = "ldi (hl), a"
	case 0x23:
		str = "inc hl"
	case 0x24:
		str = "inc h"
	case 0x25:
		str = "dec h"
	case 0x26:
		count = 2
		str = fmt.Sprintf("ld h, $%02x", ins[1])
	case 0x27:
		str = "daa"
	case 0x28:
		count = 2
		str = fmt.Sprintf("jr z, $%02x", ins[1])
	case 0x29:
		str = "add hl, hl"
	case 0x2a:
		str = "ldi a, (hl)"
	case 0x2b:
		str = "dec hl"
	case 0x2c:
		str = "inc l"
	case 0x2d:
		str = "dec l"
	case 0x2e:
		count = 2
		str = fmt.Sprintf("ld l, $%02x", ins[1])
	case 0x2f:
		str = "cpl"
	case 0x30:
		count = 2
		str = fmt.Sprintf("jr nc, $%02x", ins[1])
	case 0x31:
		count = 3
		str = fmt.Sprintf("ld sp, $%02x%02x", ins[2], ins[1])
	case 0x32:
		str = "ldd (hl), a"
	case 0x33:
		str = "inc sp"
	case 0x34:
		str = "inc (hl)"
	case 0x35:
		str = "dec (hl)"
	case 0x36:
		count = 2
		str = fmt.Sprintf("ld (hl), $%02x", ins[1])
	case 0x37:
		str = "scf"
	case 0x38:
		count = 2
		str = fmt.Sprintf("jr c, $%02x", ins[1])
	case 0x39:
		str = "add hl, sp"
	case 0x3a:
		str = "ldd a, (hl)"
	case 0x3b:
		str = "dec sp"
	case 0x3c:
		str = "inc a"
	case 0x3d:
		str = "dec a"
	case 0x3e:
		count = 2
		str = fmt.Sprintf("ld a, $%02x", ins[1])
	case 0x3f:
		str = "ccf"
	case 0x40:
		str = "ld b, b"
	case 0x41:
		str = "ld b, c"
	case 0x42:
		str = "ld b, d"
	case 0x43:
		str = "ld b, e"
	case 0x44:
		str = "ld b, h"
	case 0x45:
		str = "ld b, l"
	case 0x46:
		str = "ld b, (hl)"
	case 0x47:
		str = "ld b, a"
	case 0x48:
		str = "ld c, b"
	case 0x49:
		str = "ld c, c"
	case 0x4a:
		str = "ld c, d"
	case 0x4b:
		str = "ld c, e"
	case 0x4c:
		str = "ld c, h"
	case 0x4d:
		str = "ld c, l"
	case 0x4e:
		str = "ld c, (hl)"
	case 0x4f:
		str = "ld c, a"
	case 0x50:
		str = "ld d, b"
	case 0x51:
		str = "ld d, c"
	case 0x52:
		str = "ld d, d"
	case 0x53:
		str = "ld d, e"
	case 0x54:
		str = "ld d, h"
	case 0x55:
		str = "ld d, l"
	case 0x56:
		str = "ld d, (hl)"
	case 0x57:
		str = "ld d, a"
	case 0x58:
		str = "ld e, b"
	case 0x59:
		str = "ld e, c"
	case 0x5a:
		str = "ld e, d"
	case 0x5b:
		str = "ld e, e"
	case 0x5c:
		str = "ld e, h"
	case 0x5d:
		str = "ld e, l"
	case 0x5e:
		str = "ld e, (hl)"
	case 0x5f:
		str = "ld e, a"
	case 0x60:
		str = "ld h, b"
	case 0x61:
		str = "ld h, c"
	case 0x62:
		str = "ld h, d"
	case 0x63:
		str = "ld h, e"
	case 0x64:
		str = "ld h, h"
	case 0x65:
		str = "ld h, l"
	case 0x66:
		str = "ld h, (hl)"
	case 0x67:
		str = "ld h, a"
	case 0x68:
		str = "ld l, b"
	case 0x69:
		str = "ld l, c"
	case 0x6a:
		str = "ld l, d"
	case 0x6b:
		str = "ld l, e"
	case 0x6c:
		str = "ld l, h"
	case 0x6d:
		str = "ld l, l"
	case 0x6e:
		str = "ld l, (hl)"
	case 0x6f:
		str = "ld l, a"
	case 0x70:
		str = "ld (hl), b"
	case 0x71:
		str = "ld (hl), c"
	case 0x72:
		str = "ld (hl), d"
	case 0x73:
		str = "ld (hl), e"
	case 0x74:
		str = "ld (hl), h"
	case 0x75:
		str = "ld (hl), l"
	case 0x76:
		str = "halt"
	case 0x77:
		str = "ld (hl), a"
	case 0x78:
		str = "ld a, b"
	case 0x79:
		str = "ld a, c"
	case 0x7a:
		str = "ld a, d"
	case 0x7b:
		str = "ld a, e"
	case 0x7c:
		str = "ld a, h"
	case 0x7d:
		str = "ld a, l"
	case 0x7e:
		str = "ld a, (hl)"
	case 0x7f:
		str = "ld a, a"
	case 0x80:
		str = "add b"
	case 0x81:
		str = "add c"
	case 0x82:
		str = "add d"
	case 0x83:
		str = "add e"
	case 0x84:
		str = "add h"
	case 0x85:
		str = "add l"
	case 0x86:
		str = "add (hl)"
	case 0x87:
		str = "add a"
	case 0x88:
		str = "adc b"
	case 0x89:
		str = "adc c"
	case 0x8a:
		str = "adc d"
	case 0x8b:
		str = "adc e"
	case 0x8c:
		str = "adc h"
	case 0x8d:
		str = "adc l"
	case 0x8e:
		str = "adc (hl)"
	case 0x8f:
		str = "adc a"
	case 0x90:
		str = "sub b"
	case 0x91:
		str = "sub c"
	case 0x92:
		str = "sub d"
	case 0x93:
		str = "sub e"
	case 0x94:
		str = "sub h"
	case 0x95:
		str = "sub l"
	case 0x96:
		str = "sub (hl)"
	case 0x97:
		str = "sub a"
	case 0x98:
		str = "sbc b"
	case 0x99:
		str = "sbc c"
	case 0x9a:
		str = "sbc d"
	case 0x9b:
		str = "sbc e"
	case 0x9c:
		str = "sbc h"
	case 0x9d:
		str = "sbc l"
	case 0x9e:
		str = "sbc (hl)"
	case 0x9f:
		str = "sbc a"
	case 0xa0:
		str = "and b"
	case 0xa1:
		str = "and c"
	case 0xa2:
		str = "and d"
	case 0xa3:
		str = "and e"
	case 0xa4:
		str = "and h"
	case 0xa5:
		str = "and l"
	case 0xa6:
		str = "and (hl)"
	case 0xa7:
		str = "and a"
	case 0xa8:
		str = "xor b"
	case 0xa9:
		str = "xor c"
	case 0xaa:
		str = "xor d"
	case 0xab:
		str = "xor e"
	case 0xac:
		str = "xor h"
	case 0xad:
		str = "xor l"
	case 0xae:
		str = "xor (hl)"
	case 0xaf:
		str = "xor a"
	case 0xb0:
		str = "or b"
	case 0xb1:
		str = "or c"
	case 0xb2:
		str = "or d"
	case 0xb3:
		str = "or e"
	case 0xb4:
		str = "or h"
	case 0xb5:
		str = "or l"
	case 0xb6:
		str = "or (hl)"
	case 0xb7:
		str = "or a"
	case 0xb8:
		str = "cp b"
	case 0xb9:
		str = "cp c"
	case 0xba:
		str = "cp d"
	case 0xbb:
		str = "cp e"
	case 0xbc:
		str = "cp h"
	case 0xbd:
		str = "cp l"
	case 0xbe:
		str = "cp (hl)"
	case 0xbf:
		str = "cp a"
	case 0xc0:
		str = "ret nz"
	case 0xc1:
		str = "pop bc"
	case 0xc2:
		count = 3
		str = fmt.Sprintf("jp nz, $%02x%02x", ins[2], ins[1])
	case 0xc3:
		count = 3
		str = fmt.Sprintf("jp $%02x%02x", ins[2], ins[1])
	case 0xc4:
		count = 3
		str = fmt.Sprintf("call nz, $%02x%02x", ins[2], ins[1])
	case 0xc5:
		str = "push bc"
	case 0xc6:
		count = 2
		str = fmt.Sprintf("add $%02x", ins[1])
	case 0xc7:
		str = "rst $00"
	case 0xc8:
		str = "ret z"
	case 0xc9:
		str = "ret"
	case 0xca:
		count = 3
		str = fmt.Sprintf("jp z, $%02x%02x", ins[2], ins[1])
	case 0xcb:
		count = 2
		str = fmt.Sprintf("cb %02x", ins[1])
	case 0xcc:
		count = 3
		str = fmt.Sprintf("call z, $%02x%02x", ins[2], ins[1])
	case 0xcd:
		count = 3
		str = fmt.Sprintf("call $%02x%02x", ins[2], ins[1])
	case 0xce:
		count = 2
		str = fmt.Sprintf("adc $%02x", ins[1])
	case 0xcf:
		str = "rst $08"
	case 0xd0:
		str = "ret nc"
	case 0xd1:
		str = "pop de"
	case 0xd2:
		count = 3
		str = fmt.Sprintf("jp nc, $%02x%02x", ins[2], ins[1])
	case 0xd3:
		str = "xx"
	case 0xd4:
		count = 3
		str = fmt.Sprintf("call nc, $%02x%02x", ins[2], ins[1])
	case 0xd5:
		str = "push de"
	case 0xd6:
		count = 2
		str = fmt.Sprintf("sub $%02x", ins[1])
	case 0xd7:
		str = "rst $10"
	case 0xd8:
		str = "ret c"
	case 0xd9:
		str = "reti"
	case 0xda:
		count = 3
		str = fmt.Sprintf("jp c, $%02x%02x", ins[2], ins[1])
	case 0xdb:
		str = "xx"
	case 0xdc:
		count = 3
		str = fmt.Sprintf("call c, $%02x%02x", ins[2], ins[1])
	case 0xdd:
		str = "xx"
	case 0xde:
		count = 2
		str = fmt.Sprintf("sbc $%02x", ins[1])
	case 0xdf:
		str = "rst $18"
	case 0xe0:
		count = 2
		str = fmt.Sprintf("ldh ($ff%02x), a", ins[1])
	case 0xe1:
		str = "pop hl"
	case 0xe2:
		str = "ldh (c), a"
	case 0xe3:
		str = "xx"
	case 0xe4:
		str = "xx"
	case 0xe5:
		str = "push hl"
	case 0xe6:
		count = 2
		str = fmt.Sprintf("and $%02x", ins[1])
	case 0xe7:
		str = "rst $20"
	case 0xe8:
		count = 2
		str = fmt.Sprintf("add sp, $%02x", ins[1])
	case 0xe9:
		str = "jp hl"
	case 0xea:
		count = 3
		str = fmt.Sprintf("ld ($%02x%02x), a", ins[2], ins[1])
	case 0xeb:
		str = "xx"
	case 0xec:
		str = "xx"
	case 0xed:
		str = "xx"
	case 0xee:
		count = 2
		str = fmt.Sprintf("xor $%02x", ins[1])
	case 0xef:
		str = "rst $28"
	case 0xf0:
		count = 2
		str = fmt.Sprintf("ldh a, ($ff%02x)", ins[1])
	case 0xf1:
		str = "pop af"
	case 0xf2:
		str = "ldh a, (c)"
	case 0xf3:
		str = "di"
	case 0xf4:
		str = "xx"
	case 0xf5:
		str = "push af"
	case 0xf6:
		count = 2
		str = fmt.Sprintf("or $%02x", ins[1])
	case 0xf7:
		str = "rst $30"
	case 0xf8:
		count = 2
		str = fmt.Sprintf("ldhl sp, $%02x", ins[1])
	case 0xf9:
		str = "ld sp, hl"
	case 0xfa:
		count = 3
		str = fmt.Sprintf("ld a, ($%02x%02x)", ins[2], ins[1])
	case 0xfb:
		str = "ei"
	case 0xfc:
		str = "xx"
	case 0xfd:
		str = "xx"
	case 0xfe:
		count = 2
		str = fmt.Sprintf("cp $%02x", ins[1])
	case 0xff:
		str = "rst $38"
	}

	return str, count
}
