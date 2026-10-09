#pragma once

#include <stdint.h>
#include <string.h>

typedef int32_t SDL_Keycode;
typedef int32_t SDL_Keymod;

#define SDLK_SCANCODE_MASK (1 << 30)
#define SDL_SCANCODE_TO_KEYCODE(X) ((X) | SDLK_SCANCODE_MASK)

#define SDLK_UNKNOWN 0
#define SDLK_RETURN 13
#define SDLK_ESCAPE 27
#define SDLK_TAB 9
#define SDLK_SPACE 32
#define SDLK_a 97
#define SDLK_c 99
#define SDLK_e 101
#define SDLK_f 102
#define SDLK_k 107
#define SDLK_l 108
#define SDLK_o 111
#define SDLK_p 112
#define SDLK_r 114
#define SDLK_s 115
#define SDLK_t 116
#define SDLK_v 118
#define SDLK_w 119
#define SDLK_x 120
#define SDLK_z 122

#define SDLK_RIGHT SDL_SCANCODE_TO_KEYCODE(79)
#define SDLK_LEFT SDL_SCANCODE_TO_KEYCODE(80)
#define SDLK_DOWN SDL_SCANCODE_TO_KEYCODE(81)
#define SDLK_UP SDL_SCANCODE_TO_KEYCODE(82)
#define SDLK_F1 SDL_SCANCODE_TO_KEYCODE(58)
#define SDLK_F2 SDL_SCANCODE_TO_KEYCODE(59)
#define SDLK_F3 SDL_SCANCODE_TO_KEYCODE(60)
#define SDLK_F4 SDL_SCANCODE_TO_KEYCODE(61)
#define SDLK_F5 SDL_SCANCODE_TO_KEYCODE(62)
#define SDLK_F6 SDL_SCANCODE_TO_KEYCODE(63)
#define SDLK_F7 SDL_SCANCODE_TO_KEYCODE(64)
#define SDLK_F8 SDL_SCANCODE_TO_KEYCODE(65)
#define SDLK_F9 SDL_SCANCODE_TO_KEYCODE(66)
#define SDLK_F10 SDL_SCANCODE_TO_KEYCODE(67)
#define SDLK_LCTRL SDL_SCANCODE_TO_KEYCODE(224)
#define SDLK_LSHIFT SDL_SCANCODE_TO_KEYCODE(225)
#define SDLK_LALT SDL_SCANCODE_TO_KEYCODE(226)
#define SDLK_RCTRL SDL_SCANCODE_TO_KEYCODE(228)
#define SDLK_RSHIFT SDL_SCANCODE_TO_KEYCODE(229)
#define SDLK_RALT SDL_SCANCODE_TO_KEYCODE(230)

#define KMOD_NONE 0
#define KMOD_SHIFT 0x0003
#define KMOD_CTRL 0x00C0
#define KMOD_ALT 0x0100

static inline SDL_Keycode SDL_GetKeyFromName(const char *name)
{
	(void)name;
	return SDLK_UNKNOWN;
}
