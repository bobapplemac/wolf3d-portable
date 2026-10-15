#ifndef WG_SDL1_DYNAMIC_H
#define WG_SDL1_DYNAMIC_H
#include "WG_SDL1_HEADERS.h"
#define WG_SDL1_FUNCTION(result,name,args) extern result (SDLCALL *wg_##name) args;
#include "WG_SDL1_FUNCTIONS.inc"
#undef WG_SDL1_FUNCTION
#ifndef WG_SDL1_NO_ALIASES
#define SDL_AudioDriverName wg_SDL_AudioDriverName
#define SDL_GL_GetAttribute wg_SDL_GL_GetAttribute
#define SDL_GL_LoadLibrary wg_SDL_GL_LoadLibrary
#define SDL_GL_SetAttribute wg_SDL_GL_SetAttribute
#define SDL_GL_GetProcAddress wg_SDL_GL_GetProcAddress
#define SDL_GL_SwapBuffers wg_SDL_GL_SwapBuffers
#define SDL_CloseAudio wg_SDL_CloseAudio
#define SDL_CreateRGBSurface wg_SDL_CreateRGBSurface
#define SDL_Delay wg_SDL_Delay
#define SDL_EnableKeyRepeat wg_SDL_EnableKeyRepeat
#define SDL_FillRect wg_SDL_FillRect
#define SDL_Flip wg_SDL_Flip
#define SDL_FreeSurface wg_SDL_FreeSurface
#define SDL_GetError wg_SDL_GetError
#define SDL_GetTicks wg_SDL_GetTicks
#define SDL_GetVideoInfo wg_SDL_GetVideoInfo
#define SDL_GetWMInfo wg_SDL_GetWMInfo
#define SDL_Init wg_SDL_Init
#define SDL_InitSubSystem wg_SDL_InitSubSystem
#define SDL_JoystickClose wg_SDL_JoystickClose
#define SDL_JoystickGetAxis wg_SDL_JoystickGetAxis
#define SDL_JoystickGetButton wg_SDL_JoystickGetButton
#define SDL_JoystickNumAxes wg_SDL_JoystickNumAxes
#define SDL_JoystickNumButtons wg_SDL_JoystickNumButtons
#define SDL_JoystickOpen wg_SDL_JoystickOpen
#define SDL_JoystickUpdate wg_SDL_JoystickUpdate
#define SDL_Linked_Version wg_SDL_Linked_Version
#define SDL_LockAudio wg_SDL_LockAudio
#define SDL_MapRGB wg_SDL_MapRGB
#define SDL_NumJoysticks wg_SDL_NumJoysticks
#define SDL_OpenAudio wg_SDL_OpenAudio
#define SDL_PauseAudio wg_SDL_PauseAudio
#define SDL_PollEvent wg_SDL_PollEvent
#define SDL_putenv wg_SDL_putenv
#define SDL_Quit wg_SDL_Quit
#define SDL_SetModuleHandle wg_SDL_SetModuleHandle
#define SDL_SetVideoMode wg_SDL_SetVideoMode
#define SDL_ShowCursor wg_SDL_ShowCursor
#define SDL_SoftStretch wg_SDL_SoftStretch
#define SDL_UnlockAudio wg_SDL_UnlockAudio
#define SDL_VideoDriverName wg_SDL_VideoDriverName
#define SDL_WasInit wg_SDL_WasInit
#define SDL_WM_GrabInput wg_SDL_WM_GrabInput
#define SDL_WM_SetCaption wg_SDL_WM_SetCaption
#endif
#endif
