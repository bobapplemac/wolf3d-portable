#include "WG_DOS_ADLIB.h"

#include <conio.h>
#ifdef WG_WIN9X
#include <windows.h>
#else
#include <i86.h>
#endif

#define WG_DOS_ADLIB_ADDRESS_PORT 0x388U
#define WG_DOS_ADLIB_DATA_PORT 0x389U
#define WG_DOS_ADLIB_LAST_REGISTER 0xf5U

static uint8_t wg_dos_adlib_active;

static uint8_t WG_DOSAdLibStatus(void)
{
    return (uint8_t)inp(WG_DOS_ADLIB_ADDRESS_PORT);
}

void WG_DOSAdLibWrite(uint16_t register_number, uint8_t value)
{
    unsigned delay;

    if (register_number > 0xffU)
    {
        return;
    }

    /* Preserve the original Wolfenstein 3-D alOut delays. The six address
       reads exceed the YM3812's 3.3 us address delay; the 35 data reads
       exceed its 23 us data delay on period ISA hardware. */
#ifndef WG_WIN9X
    _disable();
#endif
    outp(WG_DOS_ADLIB_ADDRESS_PORT, (uint8_t)register_number);
    for (delay = 0U; delay < 6U; ++delay)
    {
        (void)inp(WG_DOS_ADLIB_ADDRESS_PORT);
    }
    outp(WG_DOS_ADLIB_DATA_PORT, value);
#ifndef WG_WIN9X
    _enable();
#endif
    for (delay = 0U; delay < 35U; ++delay)
    {
        (void)inp(WG_DOS_ADLIB_ADDRESS_PORT);
    }
}

int WG_DOSAdLibInit(void)
{
    uint8_t status_before;
    uint8_t status_after;
    unsigned delay;
    unsigned register_number;

#ifdef WG_WIN9X
    /* NT does not allow user-mode ISA port I/O. Never probe it there. */
    if ((GetVersion() & 0x80000000UL) == 0UL)
    {
        return 0;
    }
#endif
    WG_DOSAdLibWrite(4U, 0x60U);
    WG_DOSAdLibWrite(4U, 0x80U);
    status_before = WG_DOSAdLibStatus();
    WG_DOSAdLibWrite(2U, 0xffU);
    WG_DOSAdLibWrite(4U, 0x21U);
    for (delay = 0U; delay < 100U; ++delay)
    {
        (void)WG_DOSAdLibStatus();
    }
    status_after = WG_DOSAdLibStatus();
    WG_DOSAdLibWrite(4U, 0x60U);
    WG_DOSAdLibWrite(4U, 0x80U);

    if ((status_before & 0xe0U) != 0U || (status_after & 0xe0U) != 0xc0U)
    {
        return 0;
    }

    for (register_number = 1U;
         register_number <= WG_DOS_ADLIB_LAST_REGISTER;
         ++register_number)
    {
        WG_DOSAdLibWrite((uint16_t)register_number, 0U);
    }
    WG_DOSAdLibWrite(1U, 0x20U);
    WG_DOSAdLibWrite(8U, 0U);
    wg_dos_adlib_active = 1U;
    return 1;
}

void WG_DOSAdLibShutdown(void)
{
    unsigned register_number;

    if (!wg_dos_adlib_active)
    {
        return;
    }
    WG_DOSAdLibWrite(0xbdU, 0U);
    for (register_number = 0xb0U; register_number <= 0xb8U;
         ++register_number)
    {
        WG_DOSAdLibWrite((uint16_t)register_number, 0U);
    }
    WG_DOSAdLibWrite(4U, 0x60U);
    WG_DOSAdLibWrite(4U, 0x80U);
    wg_dos_adlib_active = 0U;
}
