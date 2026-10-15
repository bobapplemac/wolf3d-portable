#ifndef WG_SDL1_HEADERS_H
#define WG_SDL1_HEADERS_H
/* The Microsoft-built SDL1 DLL uses int enums and four-byte packing.
   Scope this to SDL: the Watcom engine DLL retains its native enum ABI. */
#ifdef __WATCOMC__
#pragma enum int
#pragma pack(push, 4)
#endif
#include <SDL/SDL.h>
#ifdef _WIN32
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#include <SDL/SDL_syswm.h>
#endif
#ifdef __WATCOMC__
#pragma pack(pop)
#pragma enum pop
#endif
#endif
