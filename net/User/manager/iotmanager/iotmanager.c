/***********************************************************************************
* @file     : iotmanager.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "iotmanager.h"

static unsigned char gIotManagerReady = 0u;

void iotManagerProcess(void)
{
	if (gIotManagerReady == 0u) {
		gIotManagerReady = 1u;
	}
}

/**************************End of file********************************/
