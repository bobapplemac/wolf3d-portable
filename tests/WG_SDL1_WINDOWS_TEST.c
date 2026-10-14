#undef NDEBUG
#include <assert.h>
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
typedef struct {int type;} wolf3d_event_t;
static HANDLE entered,release_draw,second;
static LONG draws,closed;
static int init(void) {return 1;}
static uint32_t devices(void) {return 7;}
static void pcm_shutdown(void) {}
static void shutdown_host(void) {InterlockedIncrement(&closed);}
static void mouse_capture(int value) {(void)value;}
static void title(const char *text) {(void)text;}
static int poll_event(wolf3d_event_t *event) {(void)event;return 0;}
static void present(const uint8_t *pixels,const uint8_t *palette)
{
    if(InterlockedIncrement(&draws)==1) {
        assert(pixels[0]==1 && palette[0]==1);SetEvent(entered);
        assert(WaitForSingleObject(release_draw,5000)==WAIT_OBJECT_0);
        assert(pixels[0]==1 && palette[0]==1);
    } else {assert(pixels[0]==3 && palette[0]==3);SetEvent(second);}
}
#include "../platforms/sdl1/WG_SDL1_WINDOWS.h"
int main(void)
{
    uint8_t pixels[64000],palette[768];wolf3d_event_t event;
    entered=CreateEvent(NULL,TRUE,FALSE,NULL);release_draw=CreateEvent(NULL,TRUE,FALSE,NULL);second=CreateEvent(NULL,TRUE,FALSE,NULL);
    assert(entered && release_draw && second && ui_init());assert(ui_get_devices()==7);
    memset(pixels,1,sizeof(pixels));memset(palette,1,sizeof(palette));ui_present(pixels,palette);
    assert(WaitForSingleObject(entered,5000)==WAIT_OBJECT_0);
    memset(pixels,2,sizeof(pixels));memset(palette,2,sizeof(palette));ui_present(pixels,palette);
    memset(pixels,3,sizeof(pixels));memset(palette,3,sizeof(palette));ui_present(pixels,palette);
    ui_set_title("mailbox remains responsive");ui_set_capture(1);assert(!ui_poll(&event));
    SetEvent(release_draw);assert(WaitForSingleObject(second,5000)==WAIT_OBJECT_0);
    ui_shutdown();ui_shutdown();assert(closed==1);
    CloseHandle(entered);CloseHandle(release_draw);CloseHandle(second);
    puts("SDL1 UI mailbox: blocked presentation, immutable frame, newest pending frame, shutdown passed");return 0;
}
