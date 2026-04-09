#ifndef TIME_WRAPPER_H__
#define TIME_WRAPPER_H__

#include "esp_sntp.h"
#include <time.h>

void time_wrapper_init(void (*time_sync_cmplt)(void));

#endif // TIME_WRAPPER_H__
