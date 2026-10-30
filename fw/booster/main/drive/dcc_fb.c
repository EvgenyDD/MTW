#include "dcc_fb.h"
#include "driver/uart.h"
#include "esp_log.h"

#define UART UART_NUM_1
#define BUF_SIZE (512)

uint32_t g_proto_cnt[16] = {0};

static uint8_t rx_buffer[BUF_SIZE];

static QueueHandle_t uart0_queue;

#define RX_BUF_SZ 256
static volatile uint8_t rx_buf[RX_BUF_SZ];
static volatile uint16_t rx_ptr_tail = 0, rx_ptr_head = 0;

uint8_t last_ptrn[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
uint8_t last_ptrn_p = 0;
static uint32_t rx = 0;

void proto_append(const volatile uint8_t *data, uint32_t len)
{
	for(uint32_t i = 0; i < len; i++)
	{
		// 	rx_buf[rx_ptr_head] = data[i];
		// 	rx_ptr_head = (rx_ptr_head + 1) % RX_BUF_SZ;
		last_ptrn[last_ptrn_p] = data[i];
		last_ptrn_p = (last_ptrn_p + 1) % 6;
	}
	rx += len;
}

void proto_poll(void)
{
}

static void ser_rx_task(void *arg)
{
	uart_event_t event;
	for(;;)
	{
		if(xQueueReceive(uart0_queue, (void *)&event, (TickType_t)portMAX_DELAY))
		{
			switch(event.type)
			{
			case UART_DATA:
				g_proto_cnt[0]++;
				int rdd = uart_read_bytes(UART, rx_buffer, event.size, portMAX_DELAY);
				// ESP_LOGI("", "[DATA EVT]: %d %d", rdd, event.size);
				proto_append(rx_buffer, rdd);
				proto_poll();
				break;

			case UART_FIFO_OVF:
				g_proto_cnt[1]++;
				ESP_LOGI("", "hw fifo overflow");
				uart_flush_input(UART);
				xQueueReset(uart0_queue);
				break;

			case UART_BUFFER_FULL:
				g_proto_cnt[2]++;
				ESP_LOGI("", "ring buffer full");
				uart_flush_input(UART);
				xQueueReset(uart0_queue);
				break;

				// case UART_BREAK:
				// 	ESP_LOGI("", "uart rx break");
				// 	break;

				// case UART_PARITY_ERR:
				// 	ESP_LOGI("", "uart parity error");
				// 	break;

				// case UART_FRAME_ERR:
				// 	ESP_LOGI("", "uart frame error");
				// 	break;

			case UART_PATTERN_DET:
				g_proto_cnt[3]++;
				size_t buffered_size;
				uart_get_buffered_data_len(UART, &buffered_size);
				int pos = uart_pattern_pop_pos(UART);
				// ESP_LOGI("", "[UART PATTERN DETECTED] pos: %d, buffered size: %d", pos, buffered_size);
				if(pos == -1)
				{
					uart_flush_input(UART);
				}
				else
				{
					int rdd = uart_read_bytes(UART, rx_buffer, pos, 100 / portTICK_PERIOD_MS);
					// ESP_LOGI("", "[PAT EVT]: %d", rdd);
					// proto_append((uint8_t[]){0}, 1);
					proto_append(rx_buffer, rdd);
					proto_append((uint8_t[]){0}, 1);
					// for(uint8_t i = 0; i < rdd; i++)
					// {
					// printf("%x ", rx_buffer[i]);
					// }
					// ESP_LOGE("", " ");
					proto_poll();
				}
				break;

			default:
				ESP_LOGI("", "uart event type: %d", event.type);
				break;
			}
		}
	}
}

void dcc_dc_cb_init(void)
{
	uart_config_t uart_config = {
		.baud_rate = 250000,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.rx_flow_ctrl_thresh = 120,
		.source_clk = UART_SCLK_DEFAULT,
	};

	ESP_ERROR_CHECK(uart_driver_install(UART, BUF_SIZE * 2, BUF_SIZE * 2, 20, &uart0_queue, 0));
	ESP_ERROR_CHECK(uart_param_config(UART, &uart_config));
	ESP_ERROR_CHECK(uart_set_pin(UART, -1, 25, -1, -1));
	// uart_set_line_inverse(UART, UART_SIGNAL_RXD_INV);

	uart_enable_pattern_det_baud_intr(UART, 0, 1, 200, 0, 0);
	uart_pattern_queue_reset(UART, 256); // Reset the pattern queue length to record at most 20 pattern positions

	xTaskCreate(ser_rx_task, "ser_rx", 4096, NULL, 12, NULL);
}

uint32_t dcc_dc_cb_get_rx(void)
{
	return rx;
}