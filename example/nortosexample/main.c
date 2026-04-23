#include "stm32f10x.h"
#include <string.h>
#include "bsp/bsprtt.h"
#include "bsp/bspusb.h"
#include "system/sysmgr.h"
#include "system/system.h"

int main(void)
{
    SystemInit();
    SystemCoreClockUpdate();
    bspUsbForceDisconnectEarly();
    systemManagerInit();
    while (1)
    {
        systemManagerProcess();
    }
}

/*************************************************************************************************
*									END OF FILE
*************************************************************************************************/
