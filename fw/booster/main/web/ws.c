#include "cJSON.h"
#include "console.h"
#include "driver/gptimer.h"
#include "driver/temperature_sensor.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_tls.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "lwip/sys.h"
#include <esp_event.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <esp_vfs.h>
#include <esp_wifi.h>
#include <stdarg.h>
#include <sys/param.h>

extern void start_dns_server(void);
extern uint32_t restart_timer;
extern bool flashing;

extern const char root_start[] asm("_binary_index_html_start");
extern const char root_end[] asm("_binary_index_html_end");

#define SCRATCH_BUFSIZE 1512
#define TAG "OTA"

bool is_updating = false;

char scratch[SCRATCH_BUFSIZE];
static esp_ota_handle_t handle = 0;

char ws_console_buffer[2048];
uint32_t ws_console_ptr = 0;

void ws_console(const char *s, ...)
{
	if(sizeof(ws_console_buffer) - ws_console_ptr < 3) return;
	if(ws_console_ptr > 0) ws_console_ptr--; // ignore previous \0
	va_list args;
	va_start(args, s);
	ws_console_ptr += vsnprintf(&ws_console_buffer[ws_console_ptr], sizeof(ws_console_buffer) - ws_console_ptr, s, args);
	ws_console_buffer[ws_console_ptr++] = '\n';
	ws_console_buffer[ws_console_ptr++] = 0;
	va_end(args);
}

static esp_err_t root_get_handler(httpd_req_t *req)
{
	const uint32_t root_len = root_end - root_start;
	httpd_resp_set_type(req, "text/html");
	httpd_resp_send(req, root_start, root_len);
	return ESP_OK;
}


static esp_err_t update_handler(httpd_req_t *req)
{
	flashing = true;
	is_updating = true;
	if(req->content_len == 0 /*|| req->content_len > MAX_FILE_SIZE*/)
	{
		httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File size error");
		return ESP_FAIL;
	}
	if(handle != 0)
	{
		esp_ota_end(handle);
		handle = 0;
	}
	esp_ota_mark_app_valid_cancel_rollback();
	const esp_partition_t *p = esp_ota_get_next_update_partition(NULL);
	if(esp_ota_begin(p, OTA_SIZE_UNKNOWN, &handle))
	{
		httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "OTA Begin failure");
		return ESP_FAIL;
	}
	int remaining = req->content_len;
	while(remaining > 0)
	{
		int received = httpd_req_recv(req, scratch, MIN(remaining, sizeof(scratch)));
		if(received <= 0)
		{
			if(received == HTTPD_SOCK_ERR_TIMEOUT) continue;
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
			return ESP_FAIL;
		}
		if(esp_ota_write(handle, (const void *)scratch, received))
		{
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA write failed");
			return ESP_FAIL;
		}
		remaining -= received;
	}
	if(esp_ota_end(handle))
	{
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA finish failure");
		return ESP_FAIL;
	}
	if(esp_ota_set_boot_partition(p))
	{
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Boot partition error");
		return ESP_FAIL;
	}
	httpd_resp_set_status(req, HTTPD_200);
	httpd_resp_set_type(req, "text/plain");
	httpd_resp_sendstr(req, "Download success");
	restart_timer = 1000;
	return ESP_OK;
}

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static esp_err_t console_handler(httpd_req_t *req)
{
	// int total_len = req->content_len;
	int len = strlen(&req->uri[0]);
	ESP_LOGI("ws", "console: %s (%d)", &req->uri[0], len);
	for(uint32_t i = 9; i < len; i++)
	{
		console_cb(ws_console, req->uri[i]);
	}
	console_cb(ws_console, CH_ENTER);
	httpd_resp_sendstr(req, "OK");
	return ESP_OK;
}

static esp_err_t rtd_get_handler(httpd_req_t *req)
{
	httpd_resp_set_type(req, "application/json");
	cJSON *root = cJSON_CreateObject();

	if(ws_console_ptr)
	{
		cJSON_AddStringToObject(root, "console", ws_console_buffer);
		ws_console_ptr = 0;
	}
	const char *sys_info = cJSON_Print(root);
	httpd_resp_sendstr(req, sys_info);
	free((void *)sys_info);
	cJSON_Delete(root);
	return ESP_OK;
}

static const httpd_uri_t root = {
	.uri = "/",
	.method = HTTP_GET,
	.handler = root_get_handler,
};
static const httpd_uri_t update = {
	.uri = "/update*",
	.method = HTTP_POST,
	.handler = update_handler,
};
static const httpd_uri_t rtd = {
	.uri = "/api/rtd",
	.method = HTTP_GET,
	.handler = rtd_get_handler,
};
static const httpd_uri_t console = {
	.uri = "/console/*",
	.method = HTTP_POST,
	.handler = console_handler,
};

esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
	httpd_resp_set_status(req, "302 Temporary Redirect");
	httpd_resp_set_hdr(req, "Location", "/");
	httpd_resp_send(req, "Redirect to the home", HTTPD_RESP_USE_STRLEN);
	return ESP_OK;
}

void ws_init(void)
{
	httpd_handle_t web_server = NULL;
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	config.max_open_sockets = 7;
	config.lru_purge_enable = true;
	config.uri_match_fn = httpd_uri_match_wildcard;

	ESP_LOGI("WS", "Starting httpd server on port: '%d'", config.server_port);
	if(httpd_start(&web_server, &config) == ESP_OK)
	{
		httpd_register_uri_handler(web_server, &root);
		httpd_register_uri_handler(web_server, &update);
		httpd_register_uri_handler(web_server, &rtd);
		httpd_register_uri_handler(web_server, &console);
		httpd_register_err_handler(web_server, HTTPD_404_NOT_FOUND, http_404_error_handler);
	}
	else
	{
		ESP_LOGE("WS", "httpd_start failed");
	}

	// start_dns_server();
}