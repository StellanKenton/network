/***********************************************************************************
* @file     : ethernet.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "ethernet.h"

static unsigned char gEthernetReady = 0u;

bool ethernetSendData(const uint8_t *buffer, uint16_t length)
{
	(void)buffer;
	(void)length;
	return false;
}

void ethernetProcess(void)
{
	if (gEthernetReady == 0u) {
		gEthernetReady = 1u;
	}
}

/**************************End of file********************************/
