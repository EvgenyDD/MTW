#include "wifi.h"
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <nvs.h>
#include <stdatomic.h>
#include <string.h>

#define WIFI_SSID "RW Booster"
#define WIFI_STA_NAME "RW Booster"
#define WIFI_PASS "87654321"
#define WIFI_CHNL 6
#define WIFI_MAX_STA_CONN 4

#define STA_DISCONN_TO 10000
#define AP_IDLE_TO 0 // 0 to disable turning off AP if no user connected

enum
{
	WIFI_PH_DISCONNECTED = 0,
	WIFI_PH_SCAN,
	WIFI_PH_STA_CONN,
	WIFI_PH_AP_CONN,
};
atomic_int phase = WIFI_PH_DISCONNECTED;

esp_netif_t *sta = 0, *ap = 0;
static int ap_connected_clients = 0;
static SemaphoreHandle_t client_count_mutex;
static bool sta_connected = false, wifi_scanned = false;
static uint32_t wifi_cnt_to = 0;

wifi_ap_record_t *wifi_records = NULL;
uint16_t wifi_records_cnt = 0;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	if(event_base == WIFI_EVENT)
	{
		if(event_id == WIFI_EVENT_STA_START)
		{
			ESP_LOGI("WIFI_EVT", "STA started");
			esp_wifi_connect();
		}
		else if(event_id == WIFI_EVENT_STA_CONNECTED)
		{
			ESP_LOGI("WIFI_EVT", "Connected to STA");
		}
		else if(event_id == WIFI_EVENT_STA_DISCONNECTED)
		{
			ESP_LOGI("WIFI_EVT", "Disconnected from STA");
			sta_connected = false;
		}
		else if(event_id == WIFI_EVENT_AP_STACONNECTED)
		{
			wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
			ESP_LOGI("WIFI_EVT", "station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
			esp_netif_ip_info_t ip;
			esp_netif_get_ip_info(ap, &ip);
			xSemaphoreTake(client_count_mutex, portMAX_DELAY);
			ap_connected_clients++;
			xSemaphoreGive(client_count_mutex);
		}
		else if(event_id == WIFI_EVENT_AP_STADISCONNECTED)
		{
			wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
			ESP_LOGI("WIFI_EVT", "station " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
			xSemaphoreTake(client_count_mutex, portMAX_DELAY);
			if(ap_connected_clients > 0) ap_connected_clients--;
			xSemaphoreGive(client_count_mutex);
		}
		else if(event_id == WIFI_EVENT_AP_START)
		{
			xSemaphoreTake(client_count_mutex, portMAX_DELAY);
			ap_connected_clients = 0;
			xSemaphoreGive(client_count_mutex);
		}
	}
	else if(event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
	{
		ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
		ESP_LOGI("WIFI_EVT", "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
		sta_connected = true;
	}
}

static void wifi_discover(void)
{
	esp_wifi_set_mode(WIFI_MODE_STA);
	esp_wifi_start();
	vTaskDelay(500 / portTICK_PERIOD_MS);

	wifi_scan_config_t scan_config = {.scan_time = {.active = {.min = 100, .max = 200}}};
	// ESP_LOGI("WIFI", "scan start");
	esp_wifi_scan_start(&scan_config, true);
	// ESP_LOGI("WIFI", "scan stop");

	uint16_t ap_count;
	esp_wifi_scan_get_ap_num(&ap_count);

	wifi_records_cnt = 0;
	if(ap_count > 0)
	{
		if(wifi_records) free(wifi_records);
		wifi_records = malloc(ap_count * sizeof(wifi_ap_record_t));
		ESP_LOGI("", "Discovered %d APs", ap_count);
		ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, wifi_records));
#if 0
		for(int i = 0; i < ap_count; i++)
		{
			ESP_LOGI("", "WiFi AP: %s (%d dBm)", wifi_records[i].ssid, wifi_records[i].rssi);
			ws_console("WiFi AP: %s (%d dBm)\n", wifi_records[i].ssid, wifi_records[i].rssi);
		}
#endif
		wifi_records_cnt = ap_count;
	}
	esp_wifi_stop();
}

static void switch_ap(void)
{
	ESP_LOGI("WIFI_MGR", "Starting AP SSID:%s password:%s channel:%d", WIFI_SSID, WIFI_PASS, WIFI_CHNL);
	wifi_config_t wifi_config = {.ap = {.ssid = WIFI_SSID,
										.ssid_len = strlen(WIFI_SSID),
										.channel = WIFI_CHNL,
										.password = WIFI_PASS,
										.max_connection = WIFI_MAX_STA_CONN,
										.authmode = WIFI_AUTH_WPA_WPA2_PSK}};
	if(strlen(WIFI_PASS) == 0) wifi_config.ap.authmode = WIFI_AUTH_OPEN;

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());
}

static int switch_sta_try(void)
{
	nvs_handle_t handle;
	esp_err_t err = nvs_open("app_config", NVS_READONLY, &handle);
	if(err != ESP_OK) return 1;

	char ssid[64], pswd[64];
	size_t required_size = sizeof(ssid);
	err = nvs_get_str(handle, "sta_ssid", ssid, &required_size);
	if(err != ESP_OK)
	{
		nvs_close(handle);
		return 2;
	}
	required_size = sizeof(ssid);
	err = nvs_get_str(handle, "sta_pass", pswd, &required_size);
	if(err != ESP_OK)
	{
		nvs_close(handle);
		return 3;
	}
	nvs_close(handle);

	ESP_LOGI("WIFI_MGR", "Found WiFi SSID: \"%s\" with pass: \"%s\"", ssid, pswd);
	esp_wifi_stop();

	wifi_config_t wifi_config = {.sta = {
									 .threshold.authmode = WIFI_AUTH_WPA2_PSK,
									 .pmf_cfg = {.capable = true, .required = false},
								 }};
	strcpy((char *)wifi_config.sta.ssid, ssid);
	strcpy((char *)wifi_config.sta.password, pswd);

	esp_wifi_set_mode(WIFI_MODE_STA);
	esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
	esp_wifi_start();
	return 0;
}

static void wifi_discover_task(void *pvParameters)
{
	wifi_discover();
	wifi_scanned = true;
	vTaskDelete(NULL);
}

void wifi_init(void)
{
	client_count_mutex = xSemaphoreCreateMutex();
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	sta = esp_netif_create_default_wifi_sta();
	ESP_ERROR_CHECK(esp_netif_set_hostname(sta, WIFI_STA_NAME));
	ap = esp_netif_create_default_wifi_ap();

	xTaskCreate(wifi_discover_task, "wifi_discover_task", 4096, NULL, 5, NULL);
	phase = WIFI_PH_SCAN;
}

void wifi_poll(uint32_t diff_ms)
{
	if(phase == WIFI_PH_SCAN)
	{
		if(wifi_scanned)
		{
			wifi_scanned = false;
			if(switch_sta_try() == 0)
			{
				phase = WIFI_PH_STA_CONN;
				wifi_cnt_to = STA_DISCONN_TO;
			}
			else
			{
				switch_ap();
				phase = WIFI_PH_AP_CONN;
				wifi_cnt_to = AP_IDLE_TO;
			}
		}
	}
	else if(phase == WIFI_PH_STA_CONN)
	{
		if(sta_connected) wifi_cnt_to = STA_DISCONN_TO;
		if(wifi_cnt_to > diff_ms)
		{
			wifi_cnt_to -= diff_ms;
		}
		else
		{
			ESP_LOGI("WIFI_MGR", "STA failed! Switch AP now");
			switch_ap();
			phase = WIFI_PH_AP_CONN;
			wifi_cnt_to = AP_IDLE_TO;
		}
	}
	else if(phase == WIFI_PH_AP_CONN)
	{
		if(wifi_cnt_to != 0)
		{
			xSemaphoreTake(client_count_mutex, portMAX_DELAY);
			if(ap_connected_clients >= 1) wifi_cnt_to = AP_IDLE_TO;
			xSemaphoreGive(client_count_mutex);

			if(wifi_cnt_to > diff_ms)
			{
				wifi_cnt_to -= diff_ms;
			}
			else
			{
				wifi_cnt_to = 0;
				ESP_LOGI("WIFI_MGR", "AP idle, no users! Turning off WiFi...");
				ESP_ERROR_CHECK(esp_wifi_stop());
			}
		}
	}
}

bool wifi_is_sta_connected(void) { return sta_connected; }