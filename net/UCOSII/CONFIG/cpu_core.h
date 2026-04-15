#ifndef __LOCAL_UCOSIII_CPU_CORE_H__
#define __LOCAL_UCOSIII_CPU_CORE_H__

#include "../PORT/os_cpu.h"

#define CPU_SR_ALLOC()        OS_CPU_SR cpu_sr = 0u
#define CPU_CRITICAL_ENTER()  do { cpu_sr = OS_CPU_SR_Save(); } while (0)
#define CPU_CRITICAL_EXIT()   do { OS_CPU_SR_Restore(cpu_sr); } while (0)

void CPU_Init(void);

#endif
