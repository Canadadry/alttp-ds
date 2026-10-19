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

/* Carry limits. Bombs are 10 or 99. Arrows are 30 or 99. */
int ZeldaBombCap(void);
void ZeldaBombCapSet(int cap);
int ZeldaArrowCap(void);
void ZeldaArrowCapSet(int cap);

/* 1, 2, or 3. How far Link moves each step. */
int ZeldaRunSpeed(void);
void ZeldaRunSpeedSet(int mul);

/* 1: indoor rooms stay lit, so the lantern is not required. */
int ZeldaDarkRooms(void);
void ZeldaDarkRoomsSet(int bright);

/* 1: Link's hair is blonde. 0: the original pink. */
int ZeldaHairBlonde(void);
void ZeldaHairSet(int blonde);
