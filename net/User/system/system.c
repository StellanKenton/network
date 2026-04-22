#include "system.h"

static eSystemMode g_system_mode = SYSTEM_MODE_INIT;

eSystemMode systemGetMode(void)
{
	return g_system_mode;
}


void systemSetMode(eSystemMode mode)
{
	if (mode >= SYSTEM_MODE_MAX) {
		return;
	}

	g_system_mode = mode;
}

const char *systemGetModeString(eSystemMode mode)
{
	switch (mode) {
	case SYSTEM_MODE_INIT:
		return "INIT";
	case SYSTEM_MODE_NORMAL:
		return "NORMAL";
	default:
		return "UNKNOWN";
	}
}
