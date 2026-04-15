#ifndef __LOCAL_UCOSIII_OS_CPU_H__
#define __LOCAL_UCOSIII_OS_CPU_H__

#include <stm32f4xx.h>

typedef unsigned int OS_STK;
typedef unsigned int OS_CPU_SR;

#define OS_CRITICAL_METHOD 3u
#define OS_STK_GROWTH      1u

OS_CPU_SR OS_CPU_SR_Save(void);
void      OS_CPU_SR_Restore(OS_CPU_SR cpu_sr);
void      OSCtxSw(void);
void      OSIntCtxSw(void);
void      OSStartHighRdy(void);

#define OS_TASK_SW()         OSCtxSw()
#define OS_ENTER_CRITICAL()  do { cpu_sr = OS_CPU_SR_Save(); } while (0)
#define OS_EXIT_CRITICAL()   do { OS_CPU_SR_Restore(cpu_sr); } while (0)

#endif
