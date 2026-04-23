#ifndef NETWORK_APP_SYSTEM_STORAGE_H
#define NETWORK_APP_SYSTEM_STORAGE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void systemStorageProcess(void);
bool systemStorageIsReady(void);
bool systemStorageIsBusy(void);

#ifdef __cplusplus
}
#endif

#endif
