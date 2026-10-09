#pragma once

/* Minimal Opus stub: MSU-1 / Opuz is disabled on DS. */

typedef struct OpusDecoder {
	int unused;
} OpusDecoder;

#define OPUS_OK 0
#define OPUS_RESET_STATE 4028

static inline OpusDecoder *opus_decoder_create(int Fs, int channels, int *error)
{
	(void)Fs;
	(void)channels;
	if (error)
		*error = OPUS_OK;
	return 0;
}

static inline void opus_decoder_destroy(OpusDecoder *st)
{
	(void)st;
}

static inline int opus_decoder_ctl(OpusDecoder *st, int request, ...)
{
	(void)st;
	(void)request;
	return OPUS_OK;
}

static inline int opus_decode(OpusDecoder *st, const unsigned char *data, int len,
			      short *pcm, int frame_size, int decode_fec)
{
	(void)st;
	(void)data;
	(void)len;
	(void)pcm;
	(void)frame_size;
	(void)decode_fec;
	return 0;
}
