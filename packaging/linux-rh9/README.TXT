Wolf3D portable - experimental Red Hat Linux 9 / original Pentium build

Run ./wolf3d from a terminal in X11. Keep the shared libraries beside it.
Place legally obtained game data beside the executable or use --data.
Use --help for common options, --diag for audio/data diagnostics.
Start with --opl dbopl; auto is the default. Native AdLib is deferred.
--sample-rate 11025 is useful on slower CPUs; auto prefers 22050 here.
--window-scale 1 starts at 320x240; default 2 is 640x480, always 4:3.
F11 toggles SDL fullscreen (may change the display mode).

PCM uses OSS (/dev/dsp). The OS driver must initialize your sound card and
mixer. SDL_AUDIODRIVER=dsp selects OSS; close other programs holding /dev/dsp.
No ALSA, PulseAudio, OpenGL, SDL2/SDL3 or newer kernel is required by design.
This is genuine SDL 1.2.15, not sdl12-compat.

Built/audited for i586 without CMOV, MMX or SSE. Real RH9 kernel, display,
audio and Pentium hardware validation remains pending. Docker tests alone
do not establish compatibility with Linux 2.4.
