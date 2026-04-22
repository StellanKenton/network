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

void cellularProcess(void)
{
	if (gCellularReady == 0u) {
		gCellularReady = 1u;
	}
}

/**************************End of file********************************/
