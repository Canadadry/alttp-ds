#pragma once

void ZeldaAudioInit(void);
void ZeldaAudioFrame(void);
/* Drop this frame's command and silence the song without moving it. */
void ZeldaAudioHold(void);
/* One video frame of the song, for the title skip. Does not publish voices. */
void ZeldaAudioCatchupFrame(void);
void ZeldaAudioDropLeadIn(void);
/* Play the last half-second of caught-up music while the old picture stays up. */
void ZeldaAudioPlayLeadIn(void);
