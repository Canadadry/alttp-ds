#ifndef LTTP_DS_UNALIGNED_WORD_H_
#define LTTP_DS_UNALIGNED_WORD_H_

/*
 * zelda3 uses WORD()/DWORD() as lvalues via *(uint16 *)&byte.
 * That is fine on x86 and undefined on ARM9 (odd addresses rotate or fault).
 * Packed structs make GCC emit byte loads/stores instead.
 */
typedef struct { uint16 v; } __attribute__((packed, may_alias)) LttpUnaligned16;
typedef struct { uint32 v; } __attribute__((packed, may_alias)) LttpUnaligned32;

#undef WORD
#undef DWORD
#undef load24
#define WORD(x) (((LttpUnaligned16 *)(void *)&(x))->v)
#define DWORD(x) (((LttpUnaligned32 *)(void *)&(x))->v)
#define load24(x) (DWORD(x) & 0xffffff)

#endif
