#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../platforms/dos/WG_DOS_ADLIB.c"

static unsigned regs[512], address[2], writes, extended_writes;
static unsigned opl2, missing, timer, version = 0x80000004UL;
static int disabled;
unsigned long GetVersion(void) { return version; }
void _disable(void) { disabled = 1; }
void _enable(void) { disabled = 0; }
int inp(unsigned port)
{
    assert(port == 0x388);
    return missing ? 0xff : ((opl2 ? 6 : 0) | (timer ? 0xc0 : 0));
}
int outp(unsigned port, int value)
{
    unsigned bank = (port - 0x388) / 2;
    assert(port >= 0x388 && port <= 0x38b);
    ++writes;
    if (bank) { ++extended_writes; assert(!opl2 && !missing); }
    if ((port & 1) == 0) address[bank] = value;
    else
    {
        regs[bank * 256 + address[bank]] = value;
        if (!bank && address[0] == 4) timer = value == 0x21;
    }
    return value;
}
static void reset(unsigned old_opl2, unsigned absent)
{
    memset(regs, 0, sizeof(regs));
    writes = extended_writes = timer = 0; opl2 = old_opl2; missing = absent;
    wg_dos_adlib_active = 0;
}
int main(void)
{
    unsigned i, before;
    reset(0, 0);
    regs[0x105] = 1; regs[0x104] = 0x3f;
    for (i = 0xb0; i <= 0xb8; ++i) regs[0x100 + i] = 0x20;
    assert(WG_DOSAdLibInit());
    assert(regs[0x105] == 0 && regs[0x104] == 0 && extended_writes == 22);
    for (i = 0xb0; i <= 0xb8; ++i) assert(regs[0x100 + i] == 0);
    assert(regs[1] == 0x20 && !disabled);
    WG_DOSAdLibWrite(0xc0, 1);
    assert(regs[0xc0] == 1 && regs[0x105] == 0);
    before = writes; WG_DOSAdLibWrite(0x105, 1); assert(writes == before);
    WG_DOSAdLibShutdown(); assert(!wg_dos_adlib_active);
    reset(0, 0); assert(WG_DOSAdLibInit() && !regs[0x105]);
    reset(1, 0); assert(WG_DOSAdLibInit() && !extended_writes);
    reset(0, 1); assert(!WG_DOSAdLibInit() && !extended_writes);
#ifdef WG_WIN9X
    reset(0, 0); version = 5; assert(!WG_DOSAdLibInit() && !writes);
#endif
    puts("AdLib initialization: inherited OPL3 mode, OPL2, absent card and port guards passed.");
    return 0;
}
