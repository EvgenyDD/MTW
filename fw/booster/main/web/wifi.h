#ifndef WIFI_H__
#define WIFI_H__

#include <esp_wifi.h>

void wifi_init(void);
void wifi_poll(uint32_t diff_ms);

bool wifi_is_sta_connected(void);

extern wifi_ap_record_t *wifi_records;
extern uint16_t wifi_records_cnt;

#endif // WIFI_H__