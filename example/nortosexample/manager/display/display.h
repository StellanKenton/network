/***********************************************************************************
* @file     : display.h
* @brief    : Display manager declarations.
* @details  : Provides display task hooks for the bare-metal cooperative scheduler.
* @author   : GitHub Copilot
* @date     : 2026-04-14
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef CPRSENSORBOOT_DISPLAY_H
#define CPRSENSORBOOT_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct stDisplayManagerInput {
	uint32_t heartbeat;
	uint16_t displayValue;
	bool isMemoryReady;
} stDisplayManagerInput;

void displayManagerReset(void);
bool displayManagerInit(void);
bool displayManagerProcess(const stDisplayManagerInput *input, uint16_t *displayValue);

#ifdef __cplusplus
}
#endif

#endif  // CPRSENSORBOOT_DISPLAY_H
/**************************End of file********************************/
