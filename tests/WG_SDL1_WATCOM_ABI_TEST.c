/* Compiled with the same options as the Win9x host. */
#include "WOLF3D.h"
#include "../platforms/sdl1/WG_SDL1_HEADERS.h"
#undef main
#include <stdio.h>
#include <stddef.h>
#include <assert.h>
typedef char engine_result_abi[sizeof(wolf3d_result_t)==1 ? 1 : -1];
typedef char engine_event_abi[sizeof(wolf3d_event_type_t)==1 ? 1 : -1];
typedef char sdl_key_abi[sizeof(SDLKey)==4 ? 1 : -1];
typedef char sdl_mod_abi[sizeof(SDLMod)==4 ? 1 : -1];
typedef char sdl_keysym_abi[sizeof(SDL_keysym)==16 ? 1 : -1];
typedef char sdl_audio_abi[sizeof(SDL_AudioSpec)==24 ? 1 : -1];
typedef char restored_enum_abi[sizeof(enum { test_enum_value=1 })==1 ? 1 : -1];
/* An engine enum return is valid even when the unused upper EAX bits are set. */
static wolf3d_result_t dirty_success(void);
#pragma aux dirty_success = "mov eax,12345600h" value [al] modify [eax];
int main(void)
{
    assert(dirty_success()==WOLF3D_RESULT_OK);
    puts("Watcom engine/SDL enum, struct and return-value ABI passed");
    return 0;
}
