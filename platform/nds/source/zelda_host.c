#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "types.h"
#include "config.h"
#include "assets.h"
#include "util.h"
#include "audio.h"
#include "zelda_rtl.h"
#include "features.h"
#include "snes/ppu.h"

const uint8 *g_asset_ptrs[kNumberOfAssets];
uint32 g_asset_sizes[kNumberOfAssets];

void NORETURN Die(const char *error)
{
	consoleClear();
	iprintf("FATAL:\n%s\n", error ? error : "(null)");
	iprintf("\nHalted.\n");
	for (;;)
		swiWaitForVBlank();
}

void ZeldaApuLock(void)
{
}

void ZeldaApuUnlock(void)
{
}

static uint8 *try_read_asset_file(const char *path, size_t *length)
{
	iprintf("Trying %s\n", path);
	return ReadWholeFile(path, length);
}

void ZeldaLoadAssets(void)
{
	static const char *paths[] = {
		"nitro:/zelda3_assets.dat",
		"zelda3_assets.dat",
		"fat:/zelda3_assets.dat",
		"fat:/lttp-ds/zelda3_assets.dat",
		NULL
	};
	size_t length = 0;
	uint8 *data = NULL;
	int i;

	for (i = 0; paths[i] && !data; i++)
		data = try_read_asset_file(paths[i], &length);

	if (!data)
		Die("Failed to read zelda3_assets.dat");

	{
		static const char kAssetsSig[] = { kAssets_Sig };

		if (length < 16 + 32 + 32 + 8 + kNumberOfAssets * 4 ||
		    memcmp(data, kAssetsSig, 48) != 0 ||
		    *(uint32 *)(data + 80) != kNumberOfAssets)
			Die("Invalid assets file");
	}

	{
		uint32 offset = 88 + kNumberOfAssets * 4 + *(uint32 *)(data + 84);
		size_t ai;

		for (ai = 0; ai < kNumberOfAssets; ai++) {
			uint32 size = *(uint32 *)(data + 88 + ai * 4);
			offset = (offset + 3) & ~3;
			if ((uint64)offset + size > length)
				Die("Assets file corruption");
			g_asset_sizes[ai] = size;
			g_asset_ptrs[ai] = data + offset;
			offset += size;
		}
	}

	iprintf("Assets: %u blobs, %u KB\n",
		(unsigned)kNumberOfAssets, (unsigned)(length / 1024));
}

MemBlk FindInAssetArray(int asset, int idx)
{
	return FindIndexInMemblk((MemBlk) { g_asset_ptrs[asset], g_asset_sizes[asset] }, idx);
}

void ZeldaHostInit(void)
{
	ParseConfigFile(NULL);
	g_config.enable_msu = 0;
	g_config.enhanced_mode7 = false;
	g_config.extend_y = false;
	g_config.extended_aspect_ratio = 0;
	g_config.new_renderer = true;
	g_config.enable_audio = false;
	g_config.audio_freq = 22050;
	g_config.audio_channels = 1;

	ZeldaLoadAssets();
	ZeldaInitialize();
	g_zenv.ppu->extraLeftRight = 0;
	g_wanted_zelda_features = 0;
	ZeldaEnableMsu(0);
	ZeldaSetLanguage(NULL);
	iprintf("ZeldaInitialize: ok\n");

	ZeldaRunFrame(0);
	iprintf("ZeldaRunFrame: ok\n");
}
