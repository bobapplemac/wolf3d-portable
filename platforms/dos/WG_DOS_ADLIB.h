#ifndef WG_DOS_ADLIB_H
#define WG_DOS_ADLIB_H

#include <stdint.h>

int WG_DOSAdLibInit(void);
void WG_DOSAdLibShutdown(void);
void WG_DOSAdLibWrite(uint16_t register_number, uint8_t value);

#endif
