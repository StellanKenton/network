/***********************************************************************************
* @file     : wireless.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "wireless.h"

static unsigned char gWirelessReady = 0u;

void wirelessProcess(void)
{
	if (gWirelessReady == 0u) {
		gWirelessReady = 1u;
	}
}

/**************************End of file********************************/
