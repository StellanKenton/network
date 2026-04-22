#ifndef NETWORK_APP_SYSTEM_SYSTEM_H
#define NETWORK_APP_SYSTEM_SYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
	SYSTEM_MODE_INIT = 0,
	SYSTEM_MODE_NORMAL,
	SYSTEM_MODE_MAX
} eSystemMode;

eSystemMode systemGetMode(void);
void systemSetMode(eSystemMode mode);
const char *systemGetModeString(eSystemMode mode);

#ifdef __cplusplus
}
#endif

#endif
