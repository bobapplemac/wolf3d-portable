#include "WG_DOS_ADLIB.h"

#ifdef WG_INPOUT32
#include "../win32/WG_INPOUT32.h"
#define inp WG_InpOutRead
#define outp WG_InpOutWrite
#else
#include <conio.h>
#ifdef WG_WIN9X
#include <windows.h>
#else
#include <i86.h>
#endif

#endif

#define WG_DOS_ADLIB_ADDRESS_PORT 0x388U
#define WG_DOS_ADLIB_DATA_PORT 0x389U
#define WG_DOS_ADLIB_LAST_REGISTER 0xf5U

static uint8_t wg_dos_adlib_active;

static uint8_t WG_DOSAdLibStatus(void)
{
    return (uint8_t)inp(WG_DOS_ADLIB_ADDRESS_PORT);
}

static void WG_DOSAdLibWritePort(uint16_t address_port,
                                  uint8_t register_number, uint8_t value)
{
#ifdef WG_INPOUT32
    outp(address_port, register_number);
    WG_InpOutWait(4U);
    outp(address_port + 1U, value);
    WG_InpOutWait(24U);
#else
    unsigned delay;

    /* Preserve the original Wolfenstein 3-D alOut delays. The six address
       reads exceed the YM3812's 3.3 us address delay; the 35 data reads
       exceed its 23 us data delay on period ISA hardware. */
#ifndef WG_WIN9X
    _disable();
#endif
    outp(address_port, register_number);
    for (delay = 0U; delay < 6U; ++delay)
    {
        (void)inp(WG_DOS_ADLIB_ADDRESS_PORT);
    }
    outp(address_port + 1U, value);
#ifndef WG_WIN9X
    _enable();
#endif
    for (delay = 0U; delay < 35U; ++delay)
    {
        (void)inp(WG_DOS_ADLIB_ADDRESS_PORT);
    }
#endif
}

void WG_DOSAdLibWrite(uint16_t register_number, uint8_t value)
{
    if (register_number <= 0xffU)
    {
        WG_DOSAdLibWritePort(WG_DOS_ADLIB_ADDRESS_PORT,
                            (uint8_t)register_number, value);
    }
}

int WG_DOSAdLibInit(void)
{
    uint8_t status_before;
    uint8_t status_after;
#ifndef WG_INPOUT32
    unsigned delay;
#endif
    unsigned register_number;

#ifdef WG_INPOUT32
    if (wg_dos_adlib_active) return 1;
    if (!WG_InpOutOpen()) return 0;
#endif
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
#ifdef WG_INPOUT32
    WG_InpOutWait(100U);
#else
    for (delay = 0U; delay < 100U; ++delay)
    {
        (void)WG_DOSAdLibStatus();
    }
#endif
    status_after = WG_DOSAdLibStatus();
    WG_DOSAdLibWrite(4U, 0x60U);
    WG_DOSAdLibWrite(4U, 0x80U);

    if ((status_before & 0xe0U) != 0U || (status_after & 0xe0U) != 0xc0U)
    {
#ifdef WG_INPOUT32
        WG_InpOutClose();
        WG_InpOutError("OPL chip did not respond at 0x388");
#endif
        return 0;
    }

    /* OPL2 status bits 1/2 read as one; OPL3 reads them as zero.
       Windows MIDI drivers can leave OPL3 mode enabled. Original AdLib C0
       values have no OPL3 output-routing bits, so they would then be silent.
       Silence the extra bank and return detected OPL3 hardware to OPL2 mode.
       Do not touch the second address port on an original OPL2 device. */
    if ((status_before & 0x06U) == 0U)
    {
        for (register_number = 0xb0U; register_number <= 0xb8U;
             ++register_number)
        {
            WG_DOSAdLibWritePort(WG_DOS_ADLIB_ADDRESS_PORT + 2U,
                                (uint8_t)register_number, 0U);
        }
        WG_DOSAdLibWritePort(WG_DOS_ADLIB_ADDRESS_PORT + 2U, 4U, 0U);
        WG_DOSAdLibWritePort(WG_DOS_ADLIB_ADDRESS_PORT + 2U, 5U, 0U);
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
#ifdef WG_INPOUT32
    WG_InpOutClose();
#endif
}
