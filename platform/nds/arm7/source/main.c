/* Default ARM7 core, with the stock sound server left out.
 * This binary owns the sound registers for the eight SPC voices.
 *
 * Based on the devkitPro combined template:
 *   Copyright (C) 2005 - 2010
 *   Michael Noland, Jason Rogers, Dave Murphy
 */
#include <stdalign.h>

#include <calico.h>
#include <nds.h>

#include "voices.h"

static void apu_pxi(void *user, u32 packet)
{
	u32 imm = pxiPacketGetImmediate(packet);

	(void)user;
	if (imm >= 0x02000000 && imm < 0x03000000)
		ZeldaVoicesBind((ZeldaApuIpc *)imm);
	else if (packet >= 0x02000000 && packet < 0x03000000)
		ZeldaVoicesBind((ZeldaApuIpc *)packet);
}

static int voice_thread(void *arg)
{
	(void)arg;
	while (!pmShouldReset()) {
		ZeldaVoicesService();
		threadSleep(1000);
	}
	return 0;
}

int main(void)
{
	alignas(8) static u8 stack[8192];
	static Thread thread;

	envReadNvramSettings();
	keypadStartExtServer();

	lcdSetIrqMask(DISPSTAT_IE_ALL, DISPSTAT_IE_VBLANK);
	irqEnable(IRQ_VBLANK);

	rtcInit();
	rtcSyncTime();
	pmInit();
	/* Lid close must not cut the LCDs. Sleep blanks the picture and the song. */
	pmSetSleepAllowed(false);
	blkInit();

	touchInit();
	touchStartServer(80, MAIN_THREAD_PRIO);

	ZeldaVoicesInit();
	pxiSetHandler(PxiChannel_User0, apu_pxi, NULL);

	threadPrepare(&thread, voice_thread, NULL, &stack[sizeof stack],
		      MAIN_THREAD_PRIO + 4);
	threadAttachLocalStorage(&thread, NULL);
	threadStart(&thread);

	while (pmMainLoop())
		threadWaitForVBlank();

	return 0;
}
