#ifndef WG_DOS_SB16_H
#define WG_DOS_SB16_H

#include "WOLF3D.h"

int WG_DOSSB16Init(const wolf3d_pcm_format_t *requested,
                   wolf3d_pcm_format_t *obtained);
void WG_DOSSB16Shutdown(void);
size_t WG_DOSSB16WritableFrames(void);
int WG_DOSSB16Submit(const int16_t *samples, size_t frame_count);

#endif
