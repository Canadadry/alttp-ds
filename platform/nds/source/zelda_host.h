#pragma once

#include <nds.h>

void ZeldaHostInit(void);
void ZeldaHostOnKeys(u32 held, u32 down);
void ZeldaHostTick(u32 held);
void ZeldaHostBlit(u16 *fb);
void ZeldaHostAudio(void);
void ZeldaHostSave(void);
