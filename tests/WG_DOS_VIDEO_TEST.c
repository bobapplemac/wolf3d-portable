#include <assert.h>
#include <stdio.h>
#include "../platforms/dos/WG_DOS_VIDEO_STATE.h"

static unsigned calls;
static int fail;
static unsigned short segment = 0xf000, offset = 0xfa6e;
int int386(int number, union REGS *in, union REGS *out)
{
    unsigned short operation = in->w.ax;
    assert(number == 0x31 && in->h.bl == 0x43);
    ++calls;
    if (!fail)
    {
        if (operation == 0x0200)
        {
            out->w.cx = segment;
            out->w.dx = offset;
        }
        else
        {
            assert(operation == 0x0201);
            segment = in->w.cx;
            offset = in->w.dx;
        }
    }
    out->x.cflag = fail;
    return 0;
}

int main(void)
{
    wg_dos_font_vector_t saved = {0, 0, 0};
    unsigned i, before;
    unsigned char input[4000], guarded[4002];
    unsigned char *output = guarded + 1;
    assert(WG_DOSSaveFontVector(&saved));
    segment = 0xc000; offset = 0x1234; /* BIOS mode switch. */
    fail = 1;
    assert(!WG_DOSRestoreFontVector(&saved) && saved.valid);
    fail = 0;
    assert(WG_DOSRestoreFontVector(&saved));
    assert(segment == 0xf000 && offset == 0xfa6e);
    before = calls;
    assert(WG_DOSRestoreFontVector(&saved) && calls == before);
    fail = 1;
    assert(!WG_DOSSaveFontVector(&saved) && !saved.valid);
    before = calls;
    assert(WG_DOSRestoreFontVector(&saved) && calls == before);

    memset(guarded, 0xa5, sizeof(guarded));
    for (i = 0; i < 2000; ++i) { input[i*2] = ' '; input[i*2+1] = 0x4c; }
    input[0] = 'A'; input[160] = 'B';
    assert(WG_DOSComposeExitText(output, input, 80, 25) == 1);
    assert(memcmp(output, input, 320) == 0);
    for (i = 160; i < 2000; ++i)
        assert(output[i*2] == ' ' && output[i*2+1] == 7);
    /* Narrow input uses its own stride, and padded columns remain normal. */
    assert(WG_DOSComposeExitText(output, input, 40, 3) == 2);
    assert(output[320] == 'B' && output[321] == 0x4c);
    assert(output[80] == ' ' && output[81] == 7);
    /* Reserve the last row even if the artwork fills the screen. */
    input[3840] = 'Z';
    assert(WG_DOSComposeExitText(output, input, 80, 25) == 23);
    assert(output[0] == 'B' && output[3680] == 'Z');
    for (i = 1920; i < 2000; ++i)
        assert(output[i*2] == ' ' && output[i*2+1] == 7);
    memset(input, 0, sizeof(input));
    assert(WG_DOSComposeExitText(output, input, 80, 25) == 0);
    for (i = 0; i < 2000; ++i)
        assert(output[i*2] == ' ' && output[i*2+1] == 7);
    assert(guarded[0] == 0xa5 && guarded[4001] == 0xa5);
    puts("DOS video state regressions passed");
    return 0;
}
