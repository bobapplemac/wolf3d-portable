#ifndef WG_ENGINE_COMPAT_H
#define WG_ENGINE_COMPAT_H

#include "WOLF3D.h"

/* Independently declared by the host; change only after adapting all hosts. */
#define W3P_ENGINE_API_VERSION 6U
#if WOLF3D_PLATFORM_API_VERSION != W3P_ENGINE_API_VERSION
#error Incompatible wolf3d-lib API: update wolf3d-portable or use a compatible engine
#endif

#endif
