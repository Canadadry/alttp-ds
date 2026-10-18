#include <calico.h>
#include <nds.h>
#include <calico/nds/arm7/sound.h>
#include <calico/system/tick.h>
#include <string.h>

#include "voices.h"

#define CACHE_SLOTS 128
#define BRR_BLOCKS_MAX 1024

typedef struct CachedSample {
	uint16_t src;
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
	uint32_t pos;
	uint32_t frac;
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
static int s_noise_ready;

static int clamp15(int s)
{
	if (s < -0x8000)
		s = -0x8000;
	if (s > 0x7fff)
		s = 0x7fff;
	return ((int16_t)((s & 0x7fff) << 1)) >> 1;
}

static int8_t clamp8(int s)
{
	if (s < -128)
		return -128;
	if (s > 127)
		return 127;
	return (int8_t)s;
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
 * The channel timer is 16 bits, so the hardware cannot go below ~256 Hz. */
static unsigned freq_from_pitch(unsigned pitch)
{
	unsigned freq;

	if (!pitch)
		return 0;
	if (pitch > 0x3fff)
		pitch = 0x3fff;
	freq = (32000u * pitch) >> 12;
	if (freq < 256)
		freq = 256;
	if (freq > 128000)
		freq = 128000;
	return freq;
}

static unsigned timer_from_freq(unsigned freq)
{
	unsigned timer;

	if (freq < 256)
		freq = 256;
	if (freq > 128000)
		freq = 128000;
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
 * volume has to carry both sides or a centered voice is half as loud. */
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

static int decode_sample(const uint8_t *ram, uint16_t src, uint16_t loop_addr)
{
	CachedSample *slot;
	int8_t *dst;
	int old = 0;
	int older = 0;
	uint32_t pos = 0;
	uint32_t loop_byte = 0xffffffffu;
	uint16_t addr = src;
	int blocks = 0;

	if (s_cache_count >= CACHE_SLOTS)
		return 0;
	s_pool_used = (s_pool_used + 3u) & ~3u;
	if (s_pool_used >= ZELDA_APU_PCM_BYTES)
		return 0;
	dst = s_ipc->pcm + s_pool_used;

	while (blocks < BRR_BLOCKS_MAX) {
		uint8_t header, filter, flags, shift, cur = 0;
		int i;

		if (addr == loop_addr)
			loop_byte = pos;
		if (s_pool_used + pos + 16 > ZELDA_APU_PCM_BYTES)
			break;
		header = ram[addr++];
		shift = header >> 4;
		filter = (header >> 2) & 3;
		flags = header & 3;
		for (i = 0; i < 16; i++) {
			int s;

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
			dst[pos++] = (int8_t)(s >> 8);
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

	for (srcn = 0; srcn < 256; srcn++) {
		uint16_t entry = (uint16_t)(ipc->dir_page + srcn * 4);
		uint16_t src = rd16(ram, entry);
		uint16_t loop = rd16(ram, (uint16_t)(entry + 2));

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

	if (v->flags & ZELDA_VOICE_NOISE) {
		unsigned rate = ipc->noise_rate;
		unsigned freq = rate ? 32000u / rate : 0;

		if (!freq)
			return 0;
		if (freq > 32000)
			freq = 32000;
		return freq;
	}
	if ((v->flags & ZELDA_VOICE_PMON) && prev) {
		int factor = (prev->last_out >> 4) + 0x400;

		pitch = (unsigned)(((int)pitch * factor) >> 10);
		if (pitch > 0x3fff)
			pitch = 0x3fff;
	}
	return freq_from_pitch(pitch);
}

static void attach_sample(Voice *v, const ZeldaApuIpc *ipc)
{
	const CachedSample *sample = NULL;

	v->pcm = NULL;
	v->bytes = 0;
	v->loop_byte = 0xffffffffu;
	v->ended = 0;
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

static void advance_voice(Voice *v, unsigned freq)
{
	int8_t s;

	v->frac += freq;
	if (v->frac >= 8000) {
		v->pos += v->frac / 8000;
		v->frac %= 8000;
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

static int echo_wanted(const ZeldaApuIpc *ipc)
{
	return ipc->echo_l != 0 || ipc->echo_r != 0;
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
	unsigned timer = timer_from_freq(8000);
	unsigned vol_l = (unsigned)iabs8(ipc->echo_l);
	unsigned vol_r = (unsigned)iabs8(ipc->echo_r);

	if (vol_l > 127)
		vol_l = 127;
	if (vol_r > 127)
		vol_r = 127;
	memset(ipc->echo_l_buf, 0, ZELDA_APU_ECHO_SAMPLES);
	memset(ipc->echo_r_buf, 0, ZELDA_APU_ECHO_SAMPLES);
	s_echo_done = 0;
	s_echo_t0 = tickGetCount();
	soundChPreparePcm(8, vol_l, SoundVolDiv_1, 0, timer, SoundMode_Repeat,
			  SoundFmt_Pcm8, ipc->echo_l_buf, 0,
			  ZELDA_APU_ECHO_SAMPLES / 4);
	soundChPreparePcm(9, vol_r, SoundVolDiv_1, 127, timer, SoundMode_Repeat,
			  SoundFmt_Pcm8, ipc->echo_r_buf, 0,
			  ZELDA_APU_ECHO_SAMPLES / 4);
	soundChStart(8);
	soundChStart(9);
	s_echo_on = 1;
}

static void echo_sample(ZeldaApuIpc *ipc, uint32_t pos)
{
	int in_l = 0;
	int in_r = 0;
	int i;
	uint32_t read = pos % ZELDA_APU_ECHO_SAMPLES;
	uint32_t write = (pos + ZELDA_APU_ECHO_DELAY) % ZELDA_APU_ECHO_SAMPLES;
	int delayed_l = ipc->echo_l_buf[read];
	int delayed_r = ipc->echo_r_buf[read];
	int fb = ipc->echo_fb;

	for (i = 0; i < ZELDA_APU_VOICES; i++) {
		Voice *v = &s_voice[i];

		if (!(v->flags & ZELDA_VOICE_ECHO))
			continue;
		in_l += (v->last_out * v->volume_l) >> 6;
		in_r += (v->last_out * v->volume_r) >> 6;
	}
	if (in_l < -0x8000)
		in_l = -0x8000;
	if (in_l > 0x7fff)
		in_l = 0x7fff;
	if (in_r < -0x8000)
		in_r = -0x8000;
	if (in_r > 0x7fff)
		in_r = 0x7fff;
	ipc->echo_l_buf[write] = clamp8((in_l >> 8) + (delayed_l * fb) / 128);
	ipc->echo_r_buf[write] = clamp8((in_r >> 8) + (delayed_r * fb) / 128);
}

static int echo_steps(void)
{
	u32 want;
	int ahead;

	if (!s_echo_on)
		return 8;
	want = (u32)(((tickGetCount() - s_echo_t0) * 8000ull) / TICK_FREQ);
	ahead = (int)(want - s_echo_done);
	if (ahead < 1)
		return 0;
	if (ahead > 48) {
		s_echo_done = want - 48;
		ahead = 48;
	}
	return ahead;
}

static void service_echo(ZeldaApuIpc *ipc, int steps)
{
	unsigned vol_l = (unsigned)iabs8(ipc->echo_l);
	unsigned vol_r = (unsigned)iabs8(ipc->echo_r);
	int i;

	if (vol_l > 127)
		vol_l = 127;
	if (vol_r > 127)
		vol_r = 127;
	if (!s_echo_on)
		return;
	soundChSetVolume(8, vol_l, SoundVolDiv_1);
	soundChSetVolume(9, vol_r, SoundVolDiv_1);
	for (i = 0; i < steps; i++) {
		unsigned n;

		for (n = 0; n < ZELDA_APU_VOICES; n++)
			advance_voice(&s_voice[n], voice_freq(ipc, &s_voice[n],
							     n ? &s_voice[n - 1] : NULL));
		echo_sample(ipc, s_echo_done + (u32)i);
	}
	s_echo_done += (u32)steps;
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
	int steps;
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
			v->last_out = 0;
			v->ended = 0;
			v->restart = 1;
			attach_sample(v, &ipc);
		}
		if ((v->flags & ZELDA_VOICE_RELEASE) && v->state != 4)
			v->state = 4;
		env_step(v, 32);
		freq[i] = voice_freq(&ipc, v, i ? &s_voice[i - 1] : NULL);
		program_voice(&ipc, (unsigned)i, v, freq[i]);
	}

	if (echo_wanted(&ipc)) {
		if (!s_echo_on && ipc.echo_l_buf && ipc.echo_r_buf)
			echo_start(&ipc);
	} else {
		echo_stop();
	}

	steps = echo_steps();
	if (!s_echo_on) {
		for (i = 0; i < steps; i++) {
			int n;

			for (n = 0; n < ZELDA_APU_VOICES; n++)
				advance_voice(&s_voice[n], freq[n]);
		}
	} else {
		service_echo(&ipc, steps);
	}
}
