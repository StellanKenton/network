#ifndef NETWORK_APP_SYSTEM_SYSTEM_RTT_H
#define NETWORK_APP_SYSTEM_SYSTEM_RTT_H

#ifdef __cplusplus
extern "C" {
#endif

void systemRttInit(void);
void systemRttProcess(void);
void systemRttLog(const char *tag, const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif
