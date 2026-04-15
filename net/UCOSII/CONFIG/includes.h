/*
************************************************************************************************
��Ҫ�İ����ļ�

�� ��: INCLUDES.C ucos�����ļ�
�� ��: Jean J. Labrosse
************************************************************************************************
*/

#ifndef __INCLUDES_H__
#define __INCLUDES_H__


#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>

#include "os.h"

#include "sys.h"

#include <stm32f4xx.h>	    

#define APP_TASK_OPT (OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR)

#define APP_RTOS_ASSERT(err)            \
	do {                               \
		if ((err) != OS_ERR_NONE) {    \
			while (1) {                \
			}                          \
		}                              \
	} while (0)

#endif































