/* Open Watcom/DOS integration smoke; no game data required. */
#include <stdio.h>
#include "../platforms/dos/WG_DOS_VIDEO_STATE.h"

static void mode(unsigned short value)
{
    union REGS r;
    memset(&r, 0, sizeof(r));
    r.w.ax = value;
    (void)int386(0x10, &r, &r);
}

int main(void)
{
    wg_dos_font_vector_t original, restored;
    unsigned char cells[4000];
    unsigned char *screen = (unsigned char *)0xb8000UL;
    unsigned i;
    uint16_t cursor;
    int ok;
    FILE *result;
    if (!WG_DOSSaveFontVector(&original)) return 1;
    mode(0x13);
    mode(3);
    for (i = 0; i < 2000; ++i)
    {
        cells[i * 2] = ' ';
        cells[i * 2 + 1] = 0x4c;
    }
    cells[0] = 'W';
    cursor = WG_DOSComposeExitText(screen, cells, 80, 25);
    ok = cursor == 0 && screen[0] == 'W' && screen[1] == 0x4c;
    for (i = 80; i < 2000; ++i)
        if (screen[i * 2] != ' ' || screen[i * 2 + 1] != 7) ok = 0;
    if (!WG_DOSRestoreFontVector(&original)) ok = 0;
    if (!WG_DOSSaveFontVector(&restored)) ok = 0;
    if (original.segment != restored.segment || original.offset != restored.offset) ok = 0;
    result = fopen("VIDEO.TXT", "w");
    if (result == NULL) return 2;
    fprintf(result, "DOS video smoke: %s\n", ok ? "PASS" : "FAIL");
    fclose(result);
    return ok ? 0 : 1;
}
