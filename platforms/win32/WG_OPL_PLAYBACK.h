#ifndef WG_OPL_PLAYBACK_H
#define WG_OPL_PLAYBACK_H
#include <windows.h>
#include <mmsystem.h>
#include <stdint.h>
int WG_OPLPlaybackStart(HWAVEOUT wave, uint32_t rate);
void WG_OPLPlaybackStop(void);
void WG_OPLPlaybackWrite(uint16_t reg, uint8_t value, uint64_t frame, uint32_t rate);
int WG_OPLPlaybackSubmit(const int16_t *samples, size_t frames, WAVEHDR *header);
int WG_OPLPlaybackSubmitFrames(size_t frames, uint64_t queued);
/* SDL1 callback consumption is a coarse buffer transfer, not a play cursor. */
int WG_OPLPlaybackSubmitClockedFrames(size_t frames);
#endif
