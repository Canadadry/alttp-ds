#pragma once

/* Resolve the save and load it into g_zenv.sram. Call after ZeldaInitialize. */
void ZeldaSaveMount(void);

/* Write a dirty SRAM image. Safe to call every frame; returns without disk I/O when clean. */
void ZeldaSaveFlush(void);

/* Short HUD word: off, new, loaded, saved, fail. */
const char *ZeldaSaveStatus(void);

/* 1: status bar stays on the top screen. 0: that strip is left off the top. */
int ZeldaHudOnTop(void);
void ZeldaHudSetOnTop(int on);

/* 1: the low-health beep plays. 0: it stays quiet. */
int ZeldaLowHpBeep(void);
void ZeldaLowHpBeepSet(int on);
