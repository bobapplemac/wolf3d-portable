#ifndef WG_DOS_VIDEO_STATE_H
#define WG_DOS_VIDEO_STATE_H

#include <stdint.h>
#include <string.h>
#include <i86.h>
#include "WOLF3D.h"

typedef struct wg_dos_font_vector
{
    uint16_t segment;
    uint16_t offset;
    int valid;
} wg_dos_font_vector_t;

/* INT 43h is a real-mode BIOS font pointer, not a protected-mode ISR.
   Use DPMI 0200/0201 rather than the extender's DOS getvect/setvect wrappers. */
static int WG_DOSSaveFontVector(wg_dos_font_vector_t *saved)
{
    union REGS registers;
    memset(&registers, 0, sizeof(registers));
    saved->valid = 0;
    registers.w.ax = 0x0200U;
    registers.h.bl = 0x43U;
    (void)int386(0x31, &registers, &registers);
    if (registers.x.cflag != 0U) return 0;
    saved->segment = registers.w.cx;
    saved->offset = registers.w.dx;
    saved->valid = 1;
    return 1;
}

static int WG_DOSRestoreFontVector(wg_dos_font_vector_t *saved)
{
    union REGS registers;
    if (!saved->valid) return 1;
    memset(&registers, 0, sizeof(registers));
    registers.w.ax = 0x0201U;
    registers.h.bl = 0x43U;
    registers.w.cx = saved->segment;
    registers.w.dx = saved->offset;
    (void)int386(0x31, &registers, &registers);
    if (registers.x.cflag != 0U) return 0;
    saved->valid = 0;
    return 1;
}

/* Retain the artwork through its last nonblank row, but never leave its
   attributes in the command prompt area. A full-height screen scrolls up
   one line to reserve the final row, as DOS would do when printing a prompt.
   Return the cursor row before COMMAND.COM's leading newline. */
static uint16_t WG_DOSComposeExitText(uint8_t *destination,
                                    const uint8_t *cells,
                                    uint16_t columns, uint16_t rows)
{
    uint16_t content_rows = 0U;
    uint16_t row;
    uint16_t first_row;
    unsigned index;
    for (row = 0U; row < rows; ++row)
    {
        uint16_t column;
        for (column = 0U; column < columns; ++column)
        {
            uint8_t character = cells[((size_t)row * columns + column) * 2U];
            if (character != 0U && character != ' ')
            {
                content_rows = (uint16_t)(row + 1U);
                break;
            }
        }
    }
    for (index = 0U; index < WOLF3D_TEXT_COLUMNS * WOLF3D_TEXT_ROWS; ++index)
    {
        destination[index * 2U] = ' ';
        destination[index * 2U + 1U] = 0x07U;
    }
    first_row = content_rows == WOLF3D_TEXT_ROWS ? 1U : 0U;
    for (row = first_row; row < content_rows; ++row)
    {
        memcpy(destination + (size_t)(row - first_row) * WOLF3D_TEXT_COLUMNS * 2U,
               cells + (size_t)row * columns * 2U, (size_t)columns * 2U);
    }
    content_rows = (uint16_t)(content_rows - first_row);
    return content_rows != 0U ? (uint16_t)(content_rows - 1U) : 0U;
}

#endif
