/************************************************************************************
* @file     : drviic_port.h
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_DRVIIC_PORT_H
#define REBUILDCPR_DRVIIC_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eDrvIicPortMap {
    DRVIIC_BUS0 = 0,
    DRVIIC_BUS1,
    DRVIIC_BUS_MAX
} eDrvIicPortMap;

#define DRVIIC_DEFAULT_TIMEOUT_MS 100U

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
