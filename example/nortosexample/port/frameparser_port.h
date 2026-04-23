/************************************************************************************
* @file     : frameparser_port.h
* @brief    : Project binding for rep frameparser protocol defaults.
* @details  : Declares the project-side protocol hook implementations used by
*             rep/comm/frameparser.
* @author   : GitHub Copilot
* @date     : 2026-04-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef CPRSENSORBOOT_FRAMEPARSER_PORT_H
#define CPRSENSORBOOT_FRAMEPARSER_PORT_H

#include <stdint.h>

#include "../../rep/comm/frameparser/framepareser.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FRAMEPARSER_PORT_UPDATE_MAX_FRAME_SIZE        512U
#define FRAMEPARSER_PORT_UPDATE_WAIT_PKT_TOUT_MS      1000U

typedef enum eFrameParserPortProtocolId {
	FRAMEPARSER_PORT_PROTOCOL_UPDATE = 0,
	FRAMEPARSER_PORT_PROTOCOL_MAX,
} eFrameParserPortProtocolId;

uint32_t frmPsrGetPlatformTickMs(void);
void frmPsrLoadPlatformDefaultProtoCfg(uint32_t protocolId, stFrmPsrProtoCfg *protoCfg);

#ifdef __cplusplus
}
#endif

#endif  // CPRSENSORBOOT_FRAMEPARSER_PORT_H
/**************************End of file********************************/
