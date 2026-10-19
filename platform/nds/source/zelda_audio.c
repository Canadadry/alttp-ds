#include <nds.h>
#include <malloc.h>
#include <string.h>

#include "zelda_audio.h"
#include "zelda_apu_ipc.h"
#include "zelda_rtl.h"
#include "audio.h"
#include "spc_player.h"
#include "snes/dsp.h"

extern void Die(const char *error);

static ZeldaApuIpc *ipc;
static uint32_t flushed_generation = 0xffffffffu;

static void *alloc_buf(size_t bytes)
{
	void *p = memalign(32, bytes);

	if (!p)
		Die("Out of audio RAM");
	memset(p, 0, bytes);
	DC_FlushRange(p, bytes);
	return p;
}

void ZeldaAudioInit(void)
{
	ipc = alloc_buf(sizeof *ipc);
	ipc->magic = ZELDA_APU_MAGIC;
	ipc->pcm = alloc_buf(ZELDA_APU_PCM_BYTES);
	ipc->echo_l_buf = alloc_buf(ZELDA_APU_ECHO_SAMPLES);
	ipc->echo_r_buf = alloc_buf(ZELDA_APU_ECHO_SAMPLES);
	ipc->noise = alloc_buf(ZELDA_APU_NOISE_SAMPLES);
	DC_FlushRange(ipc, sizeof *ipc);

	pxiWaitRemote(PxiChannel_User0);
	pxiSend(PxiChannel_User0, (u32)ipc);
}

static void publish(SpcPlayer *p)
{
	Dsp *dsp = p->dsp;
	uint32_t generation = SpcPlayer_SongGeneration();
	int i;

	if (generation != flushed_generation) {
		DC_FlushRange(p->ram, 65536);
		flushed_generation = generation;
	}

	ipc->seq++;
	DC_FlushRange(&ipc->seq, sizeof ipc->seq);

	ipc->generation = generation;
	ipc->dir_page = dsp->dirPage;
	ipc->master_l = dsp->masterVolumeL;
	ipc->master_r = dsp->masterVolumeR;
	ipc->echo_l = dsp->echoVolumeL;
	ipc->echo_r = dsp->echoVolumeR;
	ipc->echo_fb = dsp->feedbackVolume;
	ipc->mute = dsp->mute ? 1 : 0;
	ipc->reset = dsp->reset ? 1 : 0;
	ipc->noise_rate = dsp->noiseRate;
	ipc->spc_ram = p->ram;

	for (i = 0; i < ZELDA_APU_VOICES; i++) {
		DspChannel *c = &dsp->channel[i];
		ZeldaVoiceIpc *v = &ipc->voice[i];
		uint8_t flags = 0;

		if (c->useNoise)
			flags |= ZELDA_VOICE_NOISE;
		if (c->pitchModulation)
			flags |= ZELDA_VOICE_PMON;
		if (c->echoEnable)
			flags |= ZELDA_VOICE_ECHO;
		if (c->adsrState == 4)
			flags |= ZELDA_VOICE_RELEASE;

		v->pitch = c->pitch;
		v->volume_l = c->volumeL;
		v->volume_r = c->volumeR;
		v->srcn = c->srcn;
		v->epoch = dsp_voice_epoch[i];
		v->flags = flags;
		v->use_gain = c->useGain ? 1 : 0;
		v->direct_gain = c->directGain ? 1 : 0;
		v->gain_mode = c->gainMode;
		v->sustain = c->sustainLevel;
		v->gain_value = c->gainValue;
		v->adsr_rate[0] = c->adsrRates[0];
		v->adsr_rate[1] = c->adsrRates[1];
		v->adsr_rate[2] = c->adsrRates[2];
		v->adsr_rate[3] = c->adsrRates[3];
	}

	DC_FlushRange(ipc, sizeof *ipc);
	ipc->seq++;
	DC_FlushRange(&ipc->seq, sizeof ipc->seq);
}

void ZeldaAudioCatchupFrame(void)
{
	SpcPlayer *p = g_zenv.player;

	if (!p || !p->dsp)
		return;
	ZeldaPopApuState();
	SpcPlayer_Advance(p);
}

void ZeldaAudioFrame(void)
{
	SpcPlayer *p = g_zenv.player;

	if (!ipc || !p || !p->dsp)
		return;
	/* The game posts one APU command per frame. Nothing else drains it. */
	ZeldaPopApuState();
	SpcPlayer_Advance(p);
	publish(p);
}
