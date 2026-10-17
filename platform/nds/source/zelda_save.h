#pragma once

/* Resolve the save and load it into g_zenv.sram. Call after ZeldaInitialize. */
void ZeldaSaveMount(void);

/* Write a dirty SRAM image. Safe to call every frame; returns without disk I/O when clean. */
void ZeldaSaveFlush(void);

/* Short HUD word: off, new, loaded, saved, fail. */
const char *ZeldaSaveStatus(void);
