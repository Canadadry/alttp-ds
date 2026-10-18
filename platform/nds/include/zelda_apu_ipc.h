#pragma once

#include <stdint.h>

/* Shared between the ARM9 publisher and the ARM7 voice service.
 * Both CPUs are 32-bit. Fields are ordered so the layout matches. */

#define ZELDA_APU_MAGIC 0x5550415Au
#define ZELDA_APU_VOICES 8
#define ZELDA_APU_PCM_BYTES (96 * 1024)
#define ZELDA_APU_ECHO_SAMPLES 2048
#define ZELDA_APU_ECHO_DELAY 1024
#define ZELDA_APU_NOISE_SAMPLES 256

enum {
	ZELDA_VOICE_NOISE = 1,
	ZELDA_VOICE_PMON = 2,
	ZELDA_VOICE_ECHO = 4,
	ZELDA_VOICE_RELEASE = 8
};

typedef struct ZeldaVoiceIpc {
	uint16_t pitch;
	int8_t volume_l;
	int8_t volume_r;
	uint8_t srcn;
	uint8_t epoch;
	uint8_t flags;
	uint8_t use_gain;
	uint8_t direct_gain;
	uint8_t gain_mode;
	uint16_t sustain;
	uint16_t gain_value;
	uint16_t adsr_rate[4];
	uint16_t pad;
} ZeldaVoiceIpc;

typedef struct ZeldaApuIpc {
	uint32_t magic;
	uint32_t seq;
	uint32_t generation;
	uint32_t dir_page;
	int8_t master_l;
	int8_t master_r;
	int8_t echo_l;
	int8_t echo_r;
	int8_t echo_fb;
	uint8_t mute;
	uint8_t reset;
	uint8_t pad0;
	uint16_t noise_rate;
	uint16_t pad1;
	uint8_t *spc_ram;
	int8_t *pcm;
	int8_t *echo_l_buf;
	int8_t *echo_r_buf;
	int8_t *noise;
	ZeldaVoiceIpc voice[ZELDA_APU_VOICES];
} ZeldaApuIpc;
