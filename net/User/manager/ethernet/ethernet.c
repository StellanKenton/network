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

void ethernetProcess(void)
{
	if (gEthernetReady == 0u) {
		gEthernetReady = 1u;
	}
}

/**************************End of file********************************/
