/***********************************************************************************
* @file     : cellular.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "cellular.h"

static unsigned char gCellularReady = 0u;

bool cellularSendData(const uint8_t *buffer, uint16_t length)
{
	(void)buffer;
	(void)length;
	return false;
}

void cellularProcess(void)
{
	if (gCellularReady == 0u) {
		gCellularReady = 1u;
	}
}

/**************************End of file********************************/
