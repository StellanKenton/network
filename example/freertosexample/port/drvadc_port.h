/************************************************************************************
* @file     : drvadc_port.h
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_DRVADC_PORT_H
#define REBUILDCPR_DRVADC_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eDrvAdcPortMap {
    DRVADC_BAT = 0,
    DRVADC_FORCE,
    DRVADC_DC,
    DRVADC_5V0,
    DRVADC_3V3,
} eDrvAdcPortMap;

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
