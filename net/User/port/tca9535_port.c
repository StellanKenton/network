/***********************************************************************************
* @file     : tca9535_port.c
* @brief    : TCA9535 project port implementation.
**********************************************************************************/
#include "tca9535_port.h"

#include <stddef.h>

#include "../../SYSTEM/sys/sys.h"
#include "../../rep/driver/drviic/drviic.h"
#include "../../rep/module/pca9535/pca9535_assembly.h"
#include "../../rep/service/rtos/rtos.h"

#include "drviic_port.h"

static const stPca9535IicInterface gTca9535IicInterface = {
    .init = drvIicInit,
    .writeReg = drvIicWriteRegister,
    .readReg = drvIicReadRegister,
};
static uint8_t gTca9535DetectedAddress = PCA9535_IIC_ADDRESS_LLL;

void pca9535LoadPlatformDefaultCfg(ePca9535MapType device, stPca9535Cfg *cfg)
{
    if ((device != PCA9535_DEV0) || (cfg == NULL)) {
        return;
    }

    cfg->address = PCA9535_IIC_ADDRESS_HHH;
    cfg->outputValue = 0x0000U;
    cfg->polarityMask = 0x0000U;
    cfg->directionMask = 0x0000U;
    cfg->resetBeforeInit = true;
}

const stPca9535IicInterface *pca9535GetPlatformIicInterface(ePca9535MapType device)
{
    if (device != PCA9535_DEV0) {
        return NULL;
    }

    return &gTca9535IicInterface;
}

bool pca9535PlatformIsValidAssemble(ePca9535MapType device)
{
    return device == PCA9535_DEV0;
}

uint8_t pca9535PlatformGetLinkId(ePca9535MapType device)
{
    (void)device;
    return DRVIIC_TCA9535;
}

void pca9535PlatformResetInit(void)
{
    GPIO_InitTypeDef lGpioInit;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
    GPIO_StructInit(&lGpioInit);
    lGpioInit.GPIO_Pin = GPIO_Pin_7;
    lGpioInit.GPIO_Mode = GPIO_Mode_OUT;
    lGpioInit.GPIO_OType = GPIO_OType_PP;
    lGpioInit.GPIO_PuPd = GPIO_PuPd_NOPULL;
    lGpioInit.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOD, &lGpioInit);
    GPIO_SetBits(GPIOD, GPIO_Pin_7);
}

void pca9535PlatformResetWrite(bool assertReset)
{
    if (assertReset) {
        GPIO_ResetBits(GPIOD, GPIO_Pin_7);
    } else {
        GPIO_SetBits(GPIOD, GPIO_Pin_7);
    }
}

uint32_t pca9535PlatformGetResetAssertDelayMs(void)
{
    return 5UL;
}

uint32_t pca9535PlatformGetResetReleaseDelayMs(void)
{
    return 10UL;
}

void pca9535PlatformDelayMs(uint32_t delayMs)
{
    (void)repRtosDelayMs(delayMs);
}

eDrvStatus tca9535PortInit(void)
{
    stPca9535Cfg lCfg;
    uint8_t lAddress;
    eDrvStatus lStatus = DRV_STATUS_ERROR;

    if (pca9535GetDefCfg(PCA9535_DEV0, &lCfg) != DRV_STATUS_OK) {
        return DRV_STATUS_INVALID_PARAM;
    }

    for (lAddress = PCA9535_IIC_ADDRESS_LLL; lAddress <= PCA9535_IIC_ADDRESS_HHH; lAddress++) {
        lCfg.address = lAddress;
        lStatus = pca9535SetCfg(PCA9535_DEV0, &lCfg);
        if (lStatus != DRV_STATUS_OK) {
            return lStatus;
        }

        lStatus = pca9535Init(PCA9535_DEV0);
        if (lStatus == DRV_STATUS_OK) {
            gTca9535DetectedAddress = lAddress;
            return DRV_STATUS_OK;
        }
    }

    return lStatus;
}

uint8_t tca9535PortGetAddress(void)
{
    return gTca9535DetectedAddress;
}

eDrvStatus tca9535PortRouteCellularUsb(void)
{
    return pca9535ModifyOutputPort(PCA9535_DEV0,
                                   TCA9535_USB_SEL1_MASK | TCA9535_USB_SEL2_MASK | TCA9535_USB_SEL3_MASK,
                                   0x0000U);
}

eDrvStatus tca9535PortSetCellularPower(bool enable)
{
    return pca9535ModifyOutputPort(PCA9535_DEV0,
                                   TCA9535_CELLULAR_POWER_MASK,
                                   enable ? TCA9535_CELLULAR_POWER_MASK : 0x0000U);
}

eDrvStatus tca9535PortApplyCellularStartupOutput(void)
{
    eDrvStatus lStatus;

    lStatus = pca9535SetDirectionPort(PCA9535_DEV0, 0x0000U);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    lStatus = tca9535PortRouteCellularUsb();
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    lStatus = tca9535PortSetCellularPower(true);
    if (lStatus == DRV_STATUS_OK) {
        (void)repRtosDelayMs(100U);
    }

    return lStatus;
}

/**************************End of file********************************/
