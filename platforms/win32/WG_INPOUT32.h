#ifndef WG_INPOUT32_H
#define WG_INPOUT32_H
int WG_InpOutOpen(void);
void WG_InpOutClose(void);
int WG_InpOutRead(unsigned port);
int WG_InpOutWrite(unsigned port, int value);
void WG_InpOutWait(unsigned microseconds);
void WG_InpOutError(const char *message);
const char *WG_InpOutStatus(void);
#endif
