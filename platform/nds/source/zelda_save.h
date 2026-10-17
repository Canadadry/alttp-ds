#pragma once

/* Resolve the .sav path and load it into g_zenv.sram. Call after ZeldaInitialize. */
void ZeldaSaveMount(void);

/* Short HUD word: off, new, loaded, saved, fail. */
const char *ZeldaSaveStatus(void);
