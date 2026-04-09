
#include "console.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <string.h>

#define REQ_SIZE 256

#include "ws.h"
print_func_t pf_last = ws_console;

static QueueHandle_t uart_queue;

static const uart_config_t uart_cfg = {
	.baud_rate = 115200, // 1.5M
	.data_bits = UART_DATA_8_BITS,
	.parity = UART_PARITY_DISABLE,
	.stop_bits = UART_STOP_BITS_1,
	.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
	.rx_flow_ctrl_thresh = 127,
	.source_clk = UART_SCLK_DEFAULT,
};

static const char *error_str = "";

static uint8_t prev_request[256] = {0};
static int prev_request_len = 0;

static uint8_t req[256] = {0};
static int req_len = 0;

void console_set_error_string(const char *str) { error_str = str; }

static void print_sys_uart(const char *s, ...)
{
	va_list args;
	va_start(args, s);
	esp_log_write(ESP_LOG_INFO, "", LOG_COLOR_I "I(%lu) %s:", esp_log_timestamp(), "");
	esp_log_writev(ESP_LOG_INFO, "", s, args);
	esp_log_write(ESP_LOG_INFO, "", LOG_RESET_COLOR "\n");
	va_end(args);
}

static int strlen_first_space_or_plus(const char *str)
{
	int length = 0;
	while(str[length] != '\0' && str[length] != ' ' && str[length] != '+')
		length++;
	return length;
}

void console_cb(print_func_t pf, const char ch)
{
	pf_last = pf;
	if(ch != CH_ENTER && (ch < 0x20 || ch > 0x7E)) return;
	// pf("+%x", ch);

	if(ch != CH_ENTER)
	{
		req[req_len++] = ch;
		if(req_len >= sizeof(req) + 1) req_len = 0;
		return;
	}

	// pf("> ENTER CAPTURED %d %d", req_len, prev_request_len);

	uint8_t *data = req;
	req[req_len] = 0;

	uint16_t len_req = req_len;

	if(req_len == 0)
	{
		data = prev_request;
		len_req = prev_request_len;
		if(prev_request[0] != '\n' && prev_request[0] != '\0') pf(">");
	}
	else
	{
		memcpy(prev_request, data, len_req);
		prev_request[len_req] = 0;
		prev_request_len = len_req;
	}

	if(strncmp((char *)data, "help", 4) == 0)
	{
		pf("Available commands:");
		for(uint32_t i = 0; i < console_cmd_sz; i++)
		{
			pf("\t%s", console_cmd[i].name);
		}
	}
	else if((strcmp((char *)data, "") != 0) && (data[0] != '#') && data[0])
	{
		for(uint32_t i = 0; i < console_cmd_sz; i++)
		{
			if(console_cmd[i].cb)
			{
				uint16_t l = strlen(console_cmd[i].name), l2 = strlen_first_space_or_plus((char *)data);
				if(l == l2)
				{
					if(strncmp((char *)data, (const char *)console_cmd[i].name, l) == 0)
					{
						int p = l;
						while(data[p] != '\0' && data[p] == ' ' && p < len_req)
							p++;
						const char *param = (len_req - l) > 0
												? (len_req > p
													   ? (const char *)(data + p)
													   : 0)
												: 0;
						int error_code = CON_CB_SILENT;
						console_cmd[i].cb(pf, param, len_req - p, &error_code);
						if(error_code > CON_CB_SILENT)
						{
							pf("Error: ");
							switch(error_code)
							{
							case CON_CB_ERR_CUSTOM: pf("%s", error_str); break;
							case CON_CB_ERR_UNSAFE: pf("Unsafe operation"); break;
							case CON_CB_ERR_NO_SPACE: pf("No space left"); break;
							case CON_CB_ERR_BAD_PARAM: pf("Bad parameter"); break;
							case CON_CB_ERR_ARGS: pf("To few arguments"); break;
							default: break;
							}
						}
						else if(!error_code)
						{
							pf("Ok");
						}
						break;
					}
				}
			}
			if(i == console_cmd_sz - 1)
			{
				pf("command not found (%s)", data);
			}
		}
	}
	else
	{
		pf("");
	}
	req_len = 0;
}

static void console_uart_task(void *pvParameters)
{
	uart_event_t event;
	char ch;
	int len;
	for(;;)
	{
		if(xQueueReceive(uart_queue, (void *)&event, (TickType_t)portMAX_DELAY))
		{
			switch(event.type)
			{
			case UART_DATA:
				do
				{
					len = uart_read_bytes(UART_NUM_0, &ch, 1, 0);
					if(len) console_cb(print_sys_uart, ch);
				} while(len != 0);
				break;
			case UART_FIFO_OVF:
			case UART_BUFFER_FULL:
			case UART_BREAK:
			case UART_PARITY_ERR:
			case UART_FRAME_ERR:
			default: break;
			}
		}
	}
}

void console_init(void)
{
	ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &uart_cfg));
	uart_set_pin(UART_NUM_0, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
	uart_driver_install(UART_NUM_0, 1024, 1024, 8, &uart_queue, 0);
	xTaskCreate(console_uart_task, "uart_task", 4096, NULL, 8, NULL);
}