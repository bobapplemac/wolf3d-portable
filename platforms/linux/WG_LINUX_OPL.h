#ifndef WG_LINUX_OPL_H
#define WG_LINUX_OPL_H
#include <stddef.h>
#include <stdint.h>
/* All entry points except the private worker run on the host/PCM thread. */
int WG_LinuxOPLActive(void);
void WG_LinuxOPLSetRate(uint32_t rate);
int WG_LinuxOPLInit(void);
void WG_LinuxOPLShutdown(void);
void WG_LinuxOPLWrite(uint16_t reg, uint8_t value);
void WG_LinuxOPLWriteTimed(uint16_t reg, uint8_t value,
                           uint64_t frame, uint32_t rate);
/* Publish only PCM accepted by the device. queued is application-rate frames. */
int WG_LinuxOPLSubmit(size_t frames, uint64_t queued);
#endif
