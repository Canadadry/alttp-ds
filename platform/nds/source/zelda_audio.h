#pragma once

void ZeldaAudioInit(void);
void ZeldaAudioFrame(void);
/* One video frame of the song, for the title skip. Does not publish voices. */
void ZeldaAudioCatchupFrame(void);
