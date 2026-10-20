#include <calico.h>
#include <nds.h>
#include <calico/nds/arm7/sound.h>
#include <calico/system/tick.h>
#include <string.h>

#include "voices.h"

#define CACHE_SLOTS 128
#define BRR_BLOCKS_MAX 1024
/* Short BRR loops (the dungeon lead, the game-over tone) are a few
 * dozen samples. The SNES gaussian filter turns those into a smooth
 * wave. Playing the raw points buzzes. Four extra points per sample
 * is that filter, baked in once so the mixer can play it. */
#define BRR_UPSAMPLE 4
#define BRR_UPSAMPLE_LOOP 128

/* Same coefficients as dsp_interp in the SNES DSP. */
static const int16_t gaussValues[512] = {
	0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000,
	0x001, 0x001, 0x001, 0x001, 0x001, 0x001, 0x001, 0x001, 0x001, 0x001, 0x001, 0x002, 0x002, 0x002, 0x002, 0x002,
	0x002, 0x002, 0x003, 0x003, 0x003, 0x003, 0x003, 0x004, 0x004, 0x004, 0x004, 0x004, 0x005, 0x005, 0x005, 0x005,
	0x006, 0x006, 0x006, 0x006, 0x007, 0x007, 0x007, 0x008, 0x008, 0x008, 0x009, 0x009, 0x009, 0x00A, 0x00A, 0x00A,
	0x00B, 0x00B, 0x00B, 0x00C, 0x00C, 0x00D, 0x00D, 0x00E, 0x00E, 0x00F, 0x00F, 0x00F, 0x010, 0x010, 0x011, 0x011,
	0x012, 0x013, 0x013, 0x014, 0x014, 0x015, 0x015, 0x016, 0x017, 0x017, 0x018, 0x018, 0x019, 0x01A, 0x01B, 0x01B,
	0x01C, 0x01D, 0x01D, 0x01E, 0x01F, 0x020, 0x020, 0x021, 0x022, 0x023, 0x024, 0x024, 0x025, 0x026, 0x027, 0x028,
	0x029, 0x02A, 0x02B, 0x02C, 0x02D, 0x02E, 0x02F, 0x030, 0x031, 0x032, 0x033, 0x034, 0x035, 0x036, 0x037, 0x038,
	0x03A, 0x03B, 0x03C, 0x03D, 0x03E, 0x040, 0x041, 0x042, 0x043, 0x045, 0x046, 0x047, 0x049, 0x04A, 0x04C, 0x04D,
	0x04E, 0x050, 0x051, 0x053, 0x054, 0x056, 0x057, 0x059, 0x05A, 0x05C, 0x05E, 0x05F, 0x061, 0x063, 0x064, 0x066,
	0x068, 0x06A, 0x06B, 0x06D, 0x06F, 0x071, 0x073, 0x075, 0x076, 0x078, 0x07A, 0x07C, 0x07E, 0x080, 0x082, 0x084,
	0x086, 0x089, 0x08B, 0x08D, 0x08F, 0x091, 0x093, 0x096, 0x098, 0x09A, 0x09C, 0x09F, 0x0A1, 0x0A3, 0x0A6, 0x0A8,
	0x0AB, 0x0AD, 0x0AF, 0x0B2, 0x0B4, 0x0B7, 0x0BA, 0x0BC, 0x0BF, 0x0C1, 0x0C4, 0x0C7, 0x0C9, 0x0CC, 0x0CF, 0x0D2,
	0x0D4, 0x0D7, 0x0DA, 0x0DD, 0x0E0, 0x0E3, 0x0E6, 0x0E9, 0x0EC, 0x0EF, 0x0F2, 0x0F5, 0x0F8, 0x0FB, 0x0FE, 0x101,
	0x104, 0x107, 0x10B, 0x10E, 0x111, 0x114, 0x118, 0x11B, 0x11E, 0x122, 0x125, 0x129, 0x12C, 0x130, 0x133, 0x137,
	0x13A, 0x13E, 0x141, 0x145, 0x148, 0x14C, 0x150, 0x153, 0x157, 0x15B, 0x15F, 0x162, 0x166, 0x16A, 0x16E, 0x172,
	0x176, 0x17A, 0x17D, 0x181, 0x185, 0x189, 0x18D, 0x191, 0x195, 0x19A, 0x19E, 0x1A2, 0x1A6, 0x1AA, 0x1AE, 0x1B2,
	0x1B7, 0x1BB, 0x1BF, 0x1C3, 0x1C8, 0x1CC, 0x1D0, 0x1D5, 0x1D9, 0x1DD, 0x1E2, 0x1E6, 0x1EB, 0x1EF, 0x1F3, 0x1F8,
	0x1FC, 0x201, 0x205, 0x20A, 0x20F, 0x213, 0x218, 0x21C, 0x221, 0x226, 0x22A, 0x22F, 0x233, 0x238, 0x23D, 0x241,
	0x246, 0x24B, 0x250, 0x254, 0x259, 0x25E, 0x263, 0x267, 0x26C, 0x271, 0x276, 0x27B, 0x280, 0x284, 0x289, 0x28E,
	0x293, 0x298, 0x29D, 0x2A2, 0x2A6, 0x2AB, 0x2B0, 0x2B5, 0x2BA, 0x2BF, 0x2C4, 0x2C9, 0x2CE, 0x2D3, 0x2D8, 0x2DC,
	0x2E1, 0x2E6, 0x2EB, 0x2F0, 0x2F5, 0x2FA, 0x2FF, 0x304, 0x309, 0x30E, 0x313, 0x318, 0x31D, 0x322, 0x326, 0x32B,
	0x330, 0x335, 0x33A, 0x33F, 0x344, 0x349, 0x34E, 0x353, 0x357, 0x35C, 0x361, 0x366, 0x36B, 0x370, 0x374, 0x379,
	0x37E, 0x383, 0x388, 0x38C, 0x391, 0x396, 0x39B, 0x39F, 0x3A4, 0x3A9, 0x3AD, 0x3B2, 0x3B7, 0x3BB, 0x3C0, 0x3C5,
	0x3C9, 0x3CE, 0x3D2, 0x3D7, 0x3DC, 0x3E0, 0x3E5, 0x3E9, 0x3ED, 0x3F2, 0x3F6, 0x3FB, 0x3FF, 0x403, 0x408, 0x40C,
	0x410, 0x415, 0x419, 0x41D, 0x421, 0x425, 0x42A, 0x42E, 0x432, 0x436, 0x43A, 0x43E, 0x442, 0x446, 0x44A, 0x44E,
	0x452, 0x455, 0x459, 0x45D, 0x461, 0x465, 0x468, 0x46C, 0x470, 0x473, 0x477, 0x47A, 0x47E, 0x481, 0x485, 0x488,
	0x48C, 0x48F, 0x492, 0x496, 0x499, 0x49C, 0x49F, 0x4A2, 0x4A6, 0x4A9, 0x4AC, 0x4AF, 0x4B2, 0x4B5, 0x4B7, 0x4BA,
	0x4BD, 0x4C0, 0x4C3, 0x4C5, 0x4C8, 0x4CB, 0x4CD, 0x4D0, 0x4D2, 0x4D5, 0x4D7, 0x4D9, 0x4DC, 0x4DE, 0x4E0, 0x4E3,
	0x4E5, 0x4E7, 0x4E9, 0x4EB, 0x4ED, 0x4EF, 0x4F1, 0x4F3, 0x4F5, 0x4F6, 0x4F8, 0x4FA, 0x4FB, 0x4FD, 0x4FF, 0x500,
	0x502, 0x503, 0x504, 0x506, 0x507, 0x508, 0x50A, 0x50B, 0x50C, 0x50D, 0x50E, 0x50F, 0x510, 0x511, 0x511, 0x512,
	0x513, 0x514, 0x514, 0x515, 0x516, 0x516, 0x517, 0x517, 0x517, 0x518, 0x518, 0x518, 0x518, 0x518, 0x519, 0x519
};

typedef struct CachedSample {
	uint16_t src;
	uint8_t rate_scale;
	uint32_t loop_byte;
	uint32_t bytes;
	int8_t *pcm;
} CachedSample;

typedef struct Voice {
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
	uint16_t gain;
	uint16_t rate_counter;
	uint8_t state;
	uint8_t playing;
	uint8_t restart;
	uint8_t ended;
	uint16_t last_timer;
	uint8_t last_vol;
	uint8_t last_pan;
	uint8_t rate_scale;
	unsigned play_freq;
	uint32_t pos;
	uint32_t frac;
	uint32_t echo_pos;
	uint32_t echo_frac;
	uint32_t bytes;
	uint32_t loop_byte;
	int last_out;
	int8_t *pcm;
} Voice;

static ZeldaApuIpc *s_ipc;
static Voice s_voice[ZELDA_APU_VOICES];
static CachedSample s_cache[CACHE_SLOTS];
static unsigned s_cache_count;
static uint32_t s_pool_used;
static uint32_t s_generation;
static uint32_t s_dir_page;
static int s_have_cache;
static int s_muted;
static int s_echo_on;
static u64 s_echo_t0;
static u32 s_echo_done;
static unsigned s_echo_timer;
static unsigned s_delay_i;
static unsigned s_delay_len;
static unsigned s_fir_i;
static int16_t s_fir_l[8];
static int16_t s_fir_r[8];
static int s_sign_l;
static int s_sign_r;
/* 8 kHz samples kept ahead of the mixer. About three wakeups. */
#define ECHO_LEAD 24
static int s_noise_ready;

static int clamp15(int s)
{
	if (s < -0x8000)
		s = -0x8000;
	if (s > 0x7fff)
		s = 0x7fff;
	return ((int16_t)((s & 0x7fff) << 1)) >> 1;
}

static int clamp16(int s)
{
	if (s < -0x8000)
		return -0x8000;
	if (s > 0x7fff)
		return 0x7fff;
	return s;
}

static int iabs8(int v)
{
	return v < 0 ? -v : v;
}

static uint16_t rd16(const uint8_t *ram, uint16_t addr)
{
	return (uint16_t)ram[addr] | ((uint16_t)ram[(uint16_t)(addr + 1)] << 8);
}

static void gain_add(uint16_t *gain, int add)
{
	int g = (int)*gain + add;

	if (g < 0)
		g = 0;
	if (g > 0x7ff)
		g = 0x7ff;
	*gain = (uint16_t)g;
}

static void exp_dec(uint16_t *gain)
{
	uint16_t g = *gain;
	uint16_t sub = (uint16_t)(((uint16_t)(g - 1) >> 8) + 1);

	*gain = sub > g ? 0 : (uint16_t)(g - sub);
}

static void env_apply(Voice *v)
{
	switch (v->state) {
	case 0:
		gain_add(&v->gain, v->adsr_rate[0] == 1 ? 1024 : 32);
		if (v->gain >= 0x7e0)
			v->state = 1;
		break;
	case 1:
		exp_dec(&v->gain);
		if (v->gain < v->sustain)
			v->state = 2;
		break;
	case 2:
		exp_dec(&v->gain);
		break;
	case 3:
		switch (v->gain_mode & 3) {
		case 0:
			gain_add(&v->gain, -32);
			break;
		case 1:
			exp_dec(&v->gain);
			break;
		case 2:
			gain_add(&v->gain, 32);
			break;
		default:
			gain_add(&v->gain, v->gain < 0x600 ? 32 : 8);
			break;
		}
		break;
	default:
		break;
	}
}

static void env_step(Voice *v, int samples)
{
	if (v->state != 4 && v->use_gain && v->direct_gain) {
		v->gain = v->gain_value;
		if (v->gain > 0x7ff)
			v->gain = 0x7ff;
		return;
	}

	while (samples > 0) {
		uint16_t rate;
		int need;

		if (v->state == 4) {
			int g = (int)v->gain - 8 * samples;
			v->gain = g > 0 ? (uint16_t)g : 0;
			return;
		}
		rate = v->adsr_rate[v->state];
		if (rate == 0)
			return;
		need = (int)rate - (int)v->rate_counter;
		if (need <= 0)
			need = (int)rate;
		if (need > samples) {
			v->rate_counter = (uint16_t)(v->rate_counter + samples);
			return;
		}
		samples -= need;
		v->rate_counter = 0;
		env_apply(v);
	}
}

static void stop_channel(unsigned ch)
{
	soundChStop(ch);
}

/* One BRR sample per 4096 pitch counts, at the 32 kHz DSP clock.
 * Pitch 0x1000 is 32 kHz. Pitch 0x3FFF is just under 128 kHz.
 * Upsampled loops are played that many times faster, so the cap has
 * to sit above 128 kHz * BRR_UPSAMPLE. The timer itself cannot go
 * below about 256 Hz. */
static unsigned freq_from_pitch(unsigned pitch)
{
	if (!pitch)
		return 0;
	if (pitch > 0x3fff)
		pitch = 0x3fff;
	return (32000u * pitch) >> 12;
}

static unsigned limit_freq(unsigned freq)
{
	if (!freq)
		return 0;
	if (freq < 256)
		freq = 256;
	if (freq > 128000u * BRR_UPSAMPLE)
		freq = 128000u * BRR_UPSAMPLE;
	return freq;
}

static unsigned timer_from_freq(unsigned freq)
{
	unsigned timer;

	freq = limit_freq(freq);
	if (!freq)
		return 0;
	timer = soundTimerFromHz(freq);
	if (timer < 1)
		timer = 1;
	if (timer > 65535)
		timer = 65535;
	return timer;
}

static unsigned side_level(int gain, int vol, int master)
{
	int amp = (gain * iabs8(vol) * iabs8(master)) >> 18;

	if (amp < 0)
		amp = 0;
	if (amp > 127)
		amp = 127;
	return (unsigned)amp;
}

/* Hardware pan splits one volume across both speakers. The channel
 * volume has to carry both sides or a centered voice is half as loud.
 * Pan 0 is the left speaker and 127 is the right. */
static void voice_level(const ZeldaApuIpc *ipc, const Voice *v,
			unsigned *vol, unsigned *pan)
{
	unsigned l = side_level(v->gain, v->volume_l, ipc->master_l);
	unsigned r = side_level(v->gain, v->volume_r, ipc->master_r);
	unsigned sum = l + r;

	if (!sum) {
		*vol = 0;
		*pan = 64;
		return;
	}
	*pan = (r * 127u) / sum;
	if (*pan > 127)
		*pan = 127;
	if (sum > 126)
		*vol = 127;
	else
		*vol = (sum * 128u) / 127u;
}

static const CachedSample *cache_find(uint16_t src)
{
	unsigned i;

	for (i = 0; i < s_cache_count; i++) {
		if (s_cache[i].src == src)
			return &s_cache[i];
	}
	return NULL;
}

/* Same window as dsp_interp. hist[0] is the oldest sample. */
static int8_t gauss_pcm(const int16_t *hist, unsigned offset)
{
	int out;

	out = (gaussValues[0xff - offset] * hist[0]) >> 10;
	out += (gaussValues[0x1ff - offset] * hist[1]) >> 10;
	out += (gaussValues[0x100 + offset] * hist[2]) >> 10;
	out = (int16_t)out;
	out += (gaussValues[offset] * hist[3]) >> 10;
	out = clamp16(out);
	return (int8_t)(out >> 8);
}

/* A looping wave of a few dozen BRR samples is the dungeon lead and the
 * game-over tone. Stretch those through the gaussian kernel. Long
 * samples already sound like themselves without it. */
static int choose_scale(const uint8_t *ram, uint16_t src, uint16_t loop_addr)
{
	uint16_t addr = src;
	int blocks = 0;
	int end = 0;
	int samples, loop_at, loop_len;
	uint32_t bytes;

	while (blocks < BRR_BLOCKS_MAX) {
		uint8_t header = ram[addr];

		blocks++;
		end = header & 3;
		if (header & 1)
			break;
		addr = (uint16_t)(addr + 9);
	}
	if (!blocks || end != 3)
		return 1;
	samples = blocks * 16;
	if (loop_addr < src) {
		loop_at = 0;
	} else {
		loop_at = (int)((loop_addr - src) / 9) * 16;
		if (loop_at >= samples)
			loop_at = 0;
	}
	loop_len = samples - loop_at;
	if (loop_len <= 0 || loop_len > BRR_UPSAMPLE_LOOP)
		return 1;
	bytes = (uint32_t)samples * BRR_UPSAMPLE;
	bytes = (bytes + 3u) & ~3u;
	if (((s_pool_used + 3u) & ~3u) + bytes > ZELDA_APU_PCM_BYTES)
		return 1;
	return BRR_UPSAMPLE;
}

static int decode_sample(const uint8_t *ram, uint16_t src, uint16_t loop_addr)
{
	CachedSample *slot;
	int8_t *dst;
	int16_t hist[4];
	int old = 0;
	int older = 0;
	uint32_t pos = 0;
	uint32_t loop_byte = 0xffffffffu;
	uint16_t addr = src;
	int blocks = 0;
	int scale;

	if (s_cache_count >= CACHE_SLOTS)
		return 0;
	s_pool_used = (s_pool_used + 3u) & ~3u;
	if (s_pool_used >= ZELDA_APU_PCM_BYTES)
		return 0;
	scale = choose_scale(ram, src, loop_addr);
	dst = s_ipc->pcm + s_pool_used;
	hist[0] = hist[1] = hist[2] = hist[3] = 0;

	while (blocks < BRR_BLOCKS_MAX) {
		uint8_t header, filter, flags, shift, cur = 0;
		int i;

		if (addr == loop_addr)
			loop_byte = pos;
		if (s_pool_used + pos + 16u * (unsigned)scale > ZELDA_APU_PCM_BYTES)
			break;
		header = ram[addr++];
		shift = header >> 4;
		filter = (header >> 2) & 3;
		flags = header & 3;
		for (i = 0; i < 16; i++) {
			int s;
			int k;

			if (i & 1) {
				s = cur & 0xf;
			} else {
				cur = ram[addr++];
				s = cur >> 4;
			}
			if (s > 7)
				s -= 16;
			if (shift <= 0xc)
				s = (s << shift) >> 1;
			else
				s = (s >> 3) << 12;
			switch (filter) {
			case 1:
				s += old + (-old >> 4);
				break;
			case 2:
				s += 2 * old + ((3 * -old) >> 5) - older + (older >> 4);
				break;
			case 3:
				s += 2 * old + ((13 * -old) >> 6) - older + ((3 * older) >> 4);
				break;
			default:
				break;
			}
			s = clamp15(s);
			older = old;
			old = s;
			hist[0] = hist[1];
			hist[1] = hist[2];
			hist[2] = hist[3];
			hist[3] = (int16_t)s;
			if (scale == 1) {
				dst[pos++] = (int8_t)(s >> 8);
			} else {
				for (k = 0; k < BRR_UPSAMPLE; k++)
					dst[pos++] = gauss_pcm(hist, (unsigned)k * (256 / BRR_UPSAMPLE));
			}
		}
		blocks++;
		if (flags & 1) {
			if (!(flags & 2))
				loop_byte = 0xffffffffu;
			else if (loop_byte == 0xffffffffu)
				loop_byte = 0;
			break;
		}
		if ((blocks & 7) == 0)
			threadYield();
	}

	if (pos == 0)
		return 0;
	while (pos & 3) {
		if (s_pool_used + pos >= ZELDA_APU_PCM_BYTES)
			break;
		dst[pos++] = 0;
	}
	if (loop_byte != 0xffffffffu && loop_byte >= pos)
		loop_byte = 0xffffffffu;

	slot = &s_cache[s_cache_count++];
	slot->src = src;
	slot->rate_scale = (uint8_t)scale;
	slot->loop_byte = loop_byte;
	slot->bytes = pos;
	slot->pcm = dst;
	s_pool_used += pos;
	return 1;
}

static void rebuild_cache(const ZeldaApuIpc *ipc)
{
	const uint8_t *ram = ipc->spc_ram;
	unsigned srcn;

	s_cache_count = 0;
	s_pool_used = 0;
	s_have_cache = 1;
	s_generation = ipc->generation;
	s_dir_page = ipc->dir_page;
	if (!ram || ipc->generation == 0 || !ipc->pcm)
		return;

	/* The directory is 32 entries. Past that the bytes are the
	 * instrument table, and decoding them filled the pool. */
	for (srcn = 0; srcn < 32; srcn++) {
		uint16_t entry = (uint16_t)(ipc->dir_page + srcn * 4);
		uint16_t src = rd16(ram, entry);
		uint16_t loop = rd16(ram, (uint16_t)(entry + 2));

		if (src < 0x4000 || src >= 0xc000)
			continue;
		if (cache_find(src))
			continue;
		if (!decode_sample(ram, src, loop))
			break;
	}
}

static void fill_noise(int8_t *dst)
{
	int noise = -0x4000;
	int i;

	for (i = 0; i < ZELDA_APU_NOISE_SAMPLES; i++) {
		int bit = (noise & 1) ^ ((noise >> 1) & 1);

		noise = ((noise >> 1) & 0x3fff) | (bit << 14);
		noise = (int16_t)(((noise & 0x7fff) << 1)) >> 1;
		dst[i] = (int8_t)(noise >> 8);
	}
	s_noise_ready = 1;
}

static int snapshot(ZeldaApuIpc *dst)
{
	ZeldaApuIpc *ipc = s_ipc;
	u32 s1, s2;

	if (!ipc)
		return 0;
	s1 = *(volatile u32 *)&ipc->seq;
	if (s1 & 1)
		return 0;
	__asm__ volatile("" ::: "memory");
	memcpy(dst, ipc, sizeof *dst);
	__asm__ volatile("" ::: "memory");
	s2 = *(volatile u32 *)&ipc->seq;
	if (s1 != s2 || dst->magic != ZELDA_APU_MAGIC)
		return 0;
	return 1;
}

static void start_pcm(unsigned ch, unsigned vol, unsigned pan, unsigned freq,
		      const int8_t *pcm, uint32_t bytes, uint32_t loop_byte)
{
	SoundMode mode;
	unsigned pnt, len, timer;

	if (!pcm || bytes < 4)
		return;
	if (loop_byte == 0xffffffffu || loop_byte >= bytes) {
		mode = SoundMode_OneShot;
		pnt = 0;
		len = bytes / 4;
	} else {
		mode = SoundMode_Repeat;
		pnt = loop_byte / 4;
		len = (bytes - loop_byte) / 4;
	}
	if (len < 1)
		len = 1;
	timer = timer_from_freq(freq);
	soundChStop(ch);
	soundChPreparePcm(ch, vol, SoundVolDiv_1, pan, timer, mode,
			  SoundFmt_Pcm8, pcm, pnt, len);
	soundChStart(ch);
}

static void silence_all(void)
{
	int i;

	for (i = 0; i < 10; i++)
		stop_channel(i);
	for (i = 0; i < ZELDA_APU_VOICES; i++)
		s_voice[i].playing = 0;
	s_echo_on = 0;
}

static unsigned voice_freq(const ZeldaApuIpc *ipc, const Voice *v, const Voice *prev)
{
	unsigned pitch = v->pitch;
	unsigned freq;

	if (v->flags & ZELDA_VOICE_NOISE) {
		unsigned rate = ipc->noise_rate;
		unsigned freq = rate ? 32000u / rate : 0;

		if (freq > 32000)
			freq = 32000;
		return limit_freq(freq);
	}
	if ((v->flags & ZELDA_VOICE_PMON) && prev) {
		int factor = (prev->last_out >> 4) + 0x400;

		pitch = (unsigned)(((int)pitch * factor) >> 10);
		if (pitch > 0x3fff)
			pitch = 0x3fff;
	}
	freq = freq_from_pitch(pitch);
	if (v->rate_scale > 1)
		freq *= v->rate_scale;
	return limit_freq(freq);
}

/* The driver publishes pitch once a frame. A slide, like the game-over
 * sweep, moves more than a semitone in that time, so the hardware
 * stair-steps. Walk toward the new pitch across the next wakes. A new
 * note snaps; so does a jump of more than an octave and a fifth. */
static unsigned glide_freq(Voice *v, unsigned freq, int snap)
{
	unsigned cur, step;

	if (snap || !freq || !v->play_freq || freq > v->play_freq * 3u ||
	    v->play_freq > freq * 3u) {
		v->play_freq = freq;
		return freq;
	}
	cur = v->play_freq;
	if (cur == freq)
		return freq;
	step = (freq > cur ? freq - cur : cur - freq) / 16u;
	if (!step)
		step = 1;
	if (freq > cur) {
		cur += step;
		if (cur > freq)
			cur = freq;
	} else {
		cur -= step;
		if (cur < freq)
			cur = freq;
	}
	v->play_freq = cur;
	return cur;
}

static void attach_sample(Voice *v, const ZeldaApuIpc *ipc)
{
	const CachedSample *sample = NULL;

	v->pcm = NULL;
	v->bytes = 0;
	v->loop_byte = 0xffffffffu;
	v->ended = 0;
	v->rate_scale = 1;
	if (v->flags & ZELDA_VOICE_NOISE) {
		if (!ipc->noise)
			return;
		v->pcm = ipc->noise;
		v->bytes = ZELDA_APU_NOISE_SAMPLES;
		v->loop_byte = 0;
		return;
	}
	if (ipc->spc_ram) {
		uint16_t entry = (uint16_t)(ipc->dir_page + v->srcn * 4);
		uint16_t src = rd16(ipc->spc_ram, entry);

		sample = cache_find(src);
	}
	if (!sample)
		return;
	v->pcm = sample->pcm;
	v->bytes = sample->bytes;
	v->loop_byte = sample->loop_byte;
	v->rate_scale = sample->rate_scale ? sample->rate_scale : 1;
}

static int8_t sample_at(Voice *v)
{
	uint32_t pos;

	if (v->ended || !v->pcm || v->bytes == 0)
		return 0;
	pos = v->pos;
	if (v->loop_byte != 0xffffffffu) {
		if (pos >= v->bytes) {
			uint32_t span = v->bytes - v->loop_byte;

			if (span == 0)
				pos = v->loop_byte;
			else
				pos = v->loop_byte + (pos - v->loop_byte) % span;
			v->pos = pos;
		}
	} else if (pos >= v->bytes) {
		v->ended = 1;
		return 0;
	}
	return v->pcm[pos];
}

/* Pitch-mod source only. Echo has its own cursor, so a delay line cannot
 * drag the dry voices off the samples the mixer is playing. */
static void advance_voice(Voice *v, unsigned freq)
{
	int8_t s;

	v->frac += freq;
	while (v->frac >= 8000) {
		v->frac -= 8000;
		v->pos++;
	}
	s = sample_at(v);
	/* sampleOut is (BRR >> 1) * gain >> 11. PCM8 already dropped 8 bits. */
	v->last_out = ((int)s * (int)v->gain) >> 4;
}

static void program_voice(const ZeldaApuIpc *ipc, unsigned ch, Voice *v, unsigned freq)
{
	unsigned vol, pan, timer;

	voice_level(ipc, v, &vol, &pan);
	if (!freq) {
		if (v->playing)
			stop_channel(ch);
		v->playing = 0;
		v->restart = 0;
		return;
	}
	if (v->state == 4 && v->gain == 0) {
		if (v->playing)
			stop_channel(ch);
		v->playing = 0;
		v->restart = 0;
		return;
	}
	if (!v->pcm) {
		if (v->playing)
			stop_channel(ch);
		v->playing = 0;
		v->restart = 0;
		return;
	}
	timer = timer_from_freq(freq);
	if (v->restart || !v->playing) {
		start_pcm(ch, vol, pan, freq, v->pcm, v->bytes, v->loop_byte);
		v->playing = 1;
		v->restart = 0;
		v->last_vol = (uint8_t)vol;
		v->last_pan = (uint8_t)pan;
		v->last_timer = (uint16_t)timer;
		return;
	}
	if (vol != v->last_vol) {
		soundChSetVolume(ch, vol, SoundVolDiv_1);
		v->last_vol = (uint8_t)vol;
	}
	if (pan != v->last_pan) {
		soundChSetPan(ch, pan);
		v->last_pan = (uint8_t)pan;
	}
	if (timer != v->last_timer) {
		soundChSetTimer(ch, timer);
		v->last_timer = (uint16_t)timer;
	}
}

/* Same sums as dsp_handleEcho, at 8 kHz. The delay count is the DSP's
 * 32 kHz length divided by 4, so the title's EDL 2 stays 32 ms. FIR preset
 * 2 is 12, 33, 43, 43, 19, -2, -13, -7. */

static int echo_wanted(const ZeldaApuIpc *ipc)
{
	return ipc->echo_writes || ipc->echo_l != 0 || ipc->echo_r != 0;
}

static unsigned echo_vol(int v)
{
	if (v < 0)
		v = -v;
	if (v > 127)
		v = 127;
	return (unsigned)v;
}

static u32 echo_hw_pos(void)
{
	u64 ticks = tickGetCount() - s_echo_t0;

	if (!s_echo_timer)
		return 0;
	return (u32)((ticks * (u64)SOUND_CLOCK) /
		     ((u64)TICK_FREQ * s_echo_timer));
}

static void echo_reset_line(ZeldaApuIpc *ipc, unsigned len)
{
	memset(ipc->echo_delay_l, 0, ZELDA_APU_ECHO_DELAY_MAX * sizeof(int16_t));
	memset(ipc->echo_delay_r, 0, ZELDA_APU_ECHO_DELAY_MAX * sizeof(int16_t));
	memset(s_fir_l, 0, sizeof s_fir_l);
	memset(s_fir_r, 0, sizeof s_fir_r);
	s_delay_i = 0;
	s_fir_i = 0;
	s_delay_len = len;
}

static void echo_set_delay(ZeldaApuIpc *ipc)
{
	unsigned len = ipc->echo_delay / 4;

	if (len < 1)
		len = 1;
	if (len > ZELDA_APU_ECHO_DELAY_MAX)
		len = ZELDA_APU_ECHO_DELAY_MAX;
	if (len != s_delay_len)
		echo_reset_line(ipc, len);
}

static int8_t echo_pcm8(int s, int negative)
{
	s >>= 8;
	if (negative)
		s = -s;
	if (s < -128)
		s = -128;
	if (s > 127)
		s = 127;
	return (int8_t)s;
}

static int8_t echo_at(Voice *v)
{
	uint32_t pos;

	if (!v->pcm || v->bytes == 0)
		return 0;
	pos = v->echo_pos;
	if (v->loop_byte != 0xffffffffu) {
		if (pos >= v->bytes) {
			uint32_t span = v->bytes - v->loop_byte;

			if (span == 0)
				pos = v->loop_byte;
			else
				pos = v->loop_byte + (pos - v->loop_byte) % span;
			v->echo_pos = pos;
		}
	} else if (pos >= v->bytes) {
		return 0;
	}
	return v->pcm[pos];
}

static int echo_tap(Voice *v, unsigned freq)
{
	int8_t s;

	v->echo_frac += freq;
	while (v->echo_frac >= ZELDA_APU_ECHO_RATE) {
		v->echo_frac -= ZELDA_APU_ECHO_RATE;
		v->echo_pos++;
	}
	s = echo_at(v);
	return ((int)s * (int)v->gain) >> 4;
}

static void echo_sample(ZeldaApuIpc *ipc, const unsigned *freq, uint32_t pos)
{
	int in_l = 0;
	int in_r = 0;
	int sum_l = 0;
	int sum_r = 0;
	int i;
	unsigned ring = pos & (ZELDA_APU_ECHO_RING - 1);
	int fb = ipc->echo_fb;

	for (i = 0; i < ZELDA_APU_VOICES; i++) {
		Voice *v = &s_voice[i];
		int tap;

		if (!(v->flags & ZELDA_VOICE_ECHO))
			continue;
		tap = echo_tap(v, freq[i]);
		in_l = clamp16(in_l + ((tap * v->volume_l) >> 6));
		in_r = clamp16(in_r + ((tap * v->volume_r) >> 6));
	}

	s_fir_l[s_fir_i] = (int16_t)(ipc->echo_delay_l[s_delay_i] >> 1);
	s_fir_r[s_fir_i] = (int16_t)(ipc->echo_delay_r[s_delay_i] >> 1);
	for (i = 0; i < 7; i++) {
		unsigned idx = (s_fir_i + (unsigned)i + 1) & 7;
		int c = ipc->fir[i];

		sum_l += (s_fir_l[idx] * c) >> 6;
		sum_r += (s_fir_r[idx] * c) >> 6;
	}
	/* Hardware wraps the accumulator to 16 bits before the last tap. */
	sum_l = (int16_t)sum_l;
	sum_r = (int16_t)sum_r;
	sum_l += (s_fir_l[s_fir_i] * ipc->fir[7]) >> 6;
	sum_r += (s_fir_r[s_fir_i] * ipc->fir[7]) >> 6;
	sum_l = clamp16(sum_l);
	sum_r = clamp16(sum_r);

	ipc->echo_l_buf[ring] = echo_pcm8(sum_l, s_sign_l < 0);
	ipc->echo_r_buf[ring] = echo_pcm8(sum_r, s_sign_r < 0);

	if (ipc->echo_writes) {
		int w_l = clamp16(in_l + ((sum_l * fb) >> 7)) & 0xfffe;
		int w_r = clamp16(in_r + ((sum_r * fb) >> 7)) & 0xfffe;

		ipc->echo_delay_l[s_delay_i] = (int16_t)w_l;
		ipc->echo_delay_r[s_delay_i] = (int16_t)w_r;
	}
	s_fir_i = (s_fir_i + 1) & 7;
	if (++s_delay_i >= s_delay_len)
		s_delay_i = 0;
}

static void echo_stop(void)
{
	if (!s_echo_on)
		return;
	stop_channel(8);
	stop_channel(9);
	s_echo_on = 0;
}

static void echo_start(ZeldaApuIpc *ipc)
{
	unsigned words = ZELDA_APU_ECHO_RING / 4;

	s_echo_timer = timer_from_freq(ZELDA_APU_ECHO_RATE);
	s_sign_l = ipc->echo_l < 0 ? -1 : 1;
	s_sign_r = ipc->echo_r < 0 ? -1 : 1;
	memset(ipc->echo_l_buf, 0, ZELDA_APU_ECHO_RING);
	memset(ipc->echo_r_buf, 0, ZELDA_APU_ECHO_RING);
	s_delay_len = 0;
	echo_set_delay(ipc);
	s_echo_done = ECHO_LEAD;
	s_echo_t0 = tickGetCount();
	soundChPreparePcm(8, echo_vol(ipc->echo_l), SoundVolDiv_1, 0,
			  s_echo_timer, SoundMode_Repeat, SoundFmt_Pcm8,
			  ipc->echo_l_buf, 0, words);
	soundChPreparePcm(9, echo_vol(ipc->echo_r), SoundVolDiv_1, 127,
			  s_echo_timer, SoundMode_Repeat, SoundFmt_Pcm8,
			  ipc->echo_r_buf, 0, words);
	soundChStart(8);
	soundChStart(9);
	s_echo_on = 1;
}

static void echo_flip_unplayed(int8_t *buf, u32 from, u32 to)
{
	u32 i;

	for (i = from; i != to; i++) {
		unsigned k = i & (ZELDA_APU_ECHO_RING - 1);
		int s = buf[k];

		if (s == -128)
			s = 127;
		else
			s = -s;
		buf[k] = (int8_t)s;
	}
}

static void echo_sync_sign(ZeldaApuIpc *ipc)
{
	int sl = ipc->echo_l < 0 ? -1 : 1;
	int sr = ipc->echo_r < 0 ? -1 : 1;
	u32 hw;

	if (sl == s_sign_l && sr == s_sign_r)
		return;
	hw = echo_hw_pos();
	if (hw < s_echo_done) {
		if (sl != s_sign_l)
			echo_flip_unplayed(ipc->echo_l_buf, hw, s_echo_done);
		if (sr != s_sign_r)
			echo_flip_unplayed(ipc->echo_r_buf, hw, s_echo_done);
	}
	s_sign_l = sl;
	s_sign_r = sr;
}

static int echo_due(void)
{
	u32 hw = echo_hw_pos();
	u32 target;
	int steps;

	if (hw > s_echo_done && hw - s_echo_done > ZELDA_APU_ECHO_RING / 2)
		s_echo_done = hw;
	target = hw + ECHO_LEAD;
	if (s_echo_done >= target)
		return 0;
	steps = (int)(target - s_echo_done);
	if (steps > 32)
		steps = 32;
	return steps;
}

static void service_echo(ZeldaApuIpc *ipc, const unsigned *freq, int steps)
{
	int i;

	if (!s_echo_on)
		return;
	echo_set_delay(ipc);
	soundChSetVolume(8, echo_vol(ipc->echo_l), SoundVolDiv_1);
	soundChSetVolume(9, echo_vol(ipc->echo_r), SoundVolDiv_1);
	echo_sync_sign(ipc);
	for (i = 0; i < steps; i++) {
		echo_sample(ipc, freq, s_echo_done);
		s_echo_done++;
	}
}

void ZeldaVoicesInit(void)
{
	int i;

	pmPowerOn(POWCNT_SOUND);
	pmSoundSetAmpPower(true);
	REG_SOUNDBIAS = 0x200;
	REG_SOUNDCNT = SOUNDCNT_ENABLE | SOUNDCNT_VOL(127);
	soundSetMixerConfig(SoundOutSrc_Mixer, SoundOutSrc_Mixer, false, false);
	for (i = 0; i < 10; i++)
		soundChStop(i);
}

void ZeldaVoicesBind(ZeldaApuIpc *ipc)
{
	s_ipc = ipc;
}

void ZeldaVoicesService(void)
{
	ZeldaApuIpc ipc;
	int i;
	unsigned freq[ZELDA_APU_VOICES];

	if (!snapshot(&ipc))
		return;
	if (!s_noise_ready && ipc.noise)
		fill_noise(ipc.noise);
	if (!s_have_cache || ipc.generation != s_generation || ipc.dir_page != s_dir_page)
		rebuild_cache(&ipc);

	if (ipc.mute || ipc.reset) {
		if (!s_muted)
			silence_all();
		s_muted = 1;
		for (i = 0; i < ZELDA_APU_VOICES; i++)
			s_voice[i].epoch = ipc.voice[i].epoch;
		return;
	}
	s_muted = 0;

	for (i = 0; i < ZELDA_APU_VOICES; i++) {
		Voice *v = &s_voice[i];
		const ZeldaVoiceIpc *src = &ipc.voice[i];

		v->pitch = src->pitch;
		v->volume_l = src->volume_l;
		v->volume_r = src->volume_r;
		v->srcn = src->srcn;
		v->flags = src->flags;
		v->use_gain = src->use_gain;
		v->direct_gain = src->direct_gain;
		v->gain_mode = src->gain_mode;
		v->sustain = src->sustain;
		v->gain_value = src->gain_value;
		v->adsr_rate[0] = src->adsr_rate[0];
		v->adsr_rate[1] = src->adsr_rate[1];
		v->adsr_rate[2] = src->adsr_rate[2];
		v->adsr_rate[3] = src->adsr_rate[3];
		if (v->epoch != src->epoch) {
			v->epoch = src->epoch;
			v->state = v->use_gain ? 3 : 0;
			v->gain = 0;
			v->rate_counter = 0;
			v->pos = 0;
			v->frac = 0;
			v->echo_pos = 0;
			v->echo_frac = 0;
			v->last_out = 0;
			v->ended = 0;
			v->restart = 1;
			attach_sample(v, &ipc);
		}
		if ((v->flags & ZELDA_VOICE_RELEASE) && v->state != 4)
			v->state = 4;
		env_step(v, 32);
		freq[i] = voice_freq(&ipc, v, i ? &s_voice[i - 1] : NULL);
		freq[i] = glide_freq(v, freq[i], v->restart);
		program_voice(&ipc, (unsigned)i, v, freq[i]);
	}

	/* Eight steps at 8 kHz is one millisecond, matching the thread wake.
	 * This cursor only feeds pitch modulation. */
	for (i = 0; i < 8; i++) {
		int n;

		for (n = 0; n < ZELDA_APU_VOICES; n++)
			advance_voice(&s_voice[n], freq[n]);
	}

	if (echo_wanted(&ipc) && ipc.echo_l_buf && ipc.echo_r_buf &&
	    ipc.echo_delay_l && ipc.echo_delay_r) {
		if (!s_echo_on)
			echo_start(&ipc);
		service_echo(&ipc, freq, echo_due());
	} else {
		echo_stop();
	}
}
