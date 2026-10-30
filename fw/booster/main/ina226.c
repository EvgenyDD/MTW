#include "ina226.h"
#include "conf.h"
#include "safety.h"
#include <string.h>

#define INA226_REG_CONF 0x00
#define INA226_REG_SHUNT_VOLTAGE 0x01
#define INA226_REG_BUS_VOLTAGE 0x02
#define INA226_REG_POWER 0x03
#define INA226_REG_CURRENT 0x04
#define INA226_REG_CALIBRATION 0x05
#define INA226_REG_MASK_ENABLE 0x06
#define INA226_REG_ALERT_LIMIT 0x07
#define INA226_REG_MANUFACTURER_ID 0xFE
#define INA226_REG_DIE_ID 0xFF

#define INA226_ALERT_SHUNT_OVERVOLTAGE (1 << 15)
#define INA226_ALERT_SHUNT_UNDERVOLTAGE (1 << 14)
#define INA226_ALERT_BUS_OVERVOLTAGE (1 << 13)
#define INA226_ALERT_BUS_UNDERVOLTAGE (1 << 12)
#define INA226_ALERT_POWER_OVERLIMIT (1 << 11)
#define INA226_ALERT_READY (1 << 10)
#define INA226_ALERT_ACTIVE_HIGH (1 << 1)
#define INA226_ALERT_LATCH (1 << 0)

#define CUR_SCALE (0.0000025f / 0.010f)

ina226_t ina226 = {0};

static volatile uint8_t alert_cnt = 0;

static void reg_write(uint8_t addr, uint16_t value)
{
	i2c_cmd_handle_t cmd_handle = i2c_cmd_link_create();
	i2c_master_start(cmd_handle);
	i2c_master_write_byte(cmd_handle, (INA226_ADDR << 1) | I2C_MASTER_WRITE, true);
	i2c_master_write_byte(cmd_handle, addr, true);
	i2c_master_write_byte(cmd_handle, (uint8_t)(value >> 8), true);
	i2c_master_write_byte(cmd_handle, (uint8_t)value, true);
	i2c_master_stop(cmd_handle);
	i2c_master_cmd_begin(I2C_INA_NUM, cmd_handle, 1000 / portTICK_PERIOD_MS);
	i2c_cmd_link_delete(cmd_handle);
}

static uint16_t reg_read(uint8_t addr)
{
	uint16_t data = 0;
	uint8_t rx_buffer[2] = {0, 0};

	i2c_cmd_handle_t cmd_handle = i2c_cmd_link_create();
	i2c_master_start(cmd_handle);
	i2c_master_write_byte(cmd_handle, (INA226_ADDR << 1) | I2C_MASTER_WRITE, true);
	i2c_master_write_byte(cmd_handle, addr, true);

	i2c_master_start(cmd_handle);
	i2c_master_write_byte(cmd_handle, (INA226_ADDR << 1) | I2C_MASTER_READ, true);
	i2c_master_read_byte(cmd_handle, &rx_buffer[0], I2C_MASTER_ACK);
	i2c_master_read_byte(cmd_handle, &rx_buffer[1], I2C_MASTER_NACK);
	i2c_master_stop(cmd_handle);

	esp_err_t ret = i2c_master_cmd_begin(I2C_INA_NUM, cmd_handle, 1000 / portTICK_PERIOD_MS);
	i2c_cmd_link_delete(cmd_handle);

	if(ret == ESP_OK) data = (rx_buffer[0] << 8) | rx_buffer[1];
	if(ret != ESP_OK) ina226.err++;

	return data;
}

static void IRAM_ATTR isr_alert(void *arg)
{
	safety_set_error(ERR_OC, true, ina226.cur);
	alert_cnt++;
}

void ina226_init(void)
{
	i2c_config_t conf = {
		.mode = I2C_MODE_MASTER,
		.sda_io_num = 5,
		.scl_io_num = 18,
		.sda_pullup_en = GPIO_PULLUP_ENABLE,
		.scl_pullup_en = GPIO_PULLUP_ENABLE,
		.master.clk_speed = I2C_MASTER_FREQ_HZ,
	};

	i2c_param_config(I2C_INA_NUM, &conf);
	i2c_driver_install(I2C_INA_NUM, conf.mode, 0, 0, 0);

	reg_write(INA226_REG_CONF, 0x4127 | ((2 * 0) << 9 /* avg */));
	reg_write(INA226_REG_MASK_ENABLE, INA226_ALERT_SHUNT_OVERVOLTAGE | INA226_ALERT_LATCH);
	reg_write(INA226_REG_ALERT_LIMIT, (uint16_t)(LIMIT_OC / CUR_SCALE));

	gpio_install_isr_service(0);

	esp_rom_gpio_pad_select_gpio(14);
	gpio_set_direction(GPIO_NUM_14, GPIO_MODE_INPUT);
	gpio_set_pull_mode(GPIO_NUM_14, GPIO_PULLUP_ONLY);
	gpio_isr_handler_add(GPIO_NUM_14, isr_alert, NULL);
	gpio_set_intr_type(GPIO_NUM_14, GPIO_INTR_NEGEDGE);
	gpio_intr_enable(GPIO_NUM_14);
}

#define SAMPLE_COUNT 16
#define DROP_COUNT 4
#define AVG_COUNT (SAMPLE_COUNT - (2 * DROP_COUNT))

uint16_t get_v_filt(uint16_t new_sample)
{
	static uint16_t samples[SAMPLE_COUNT] = {0};
	static int idx = 0;

	samples[idx] = new_sample;
	idx = (idx + 1) % SAMPLE_COUNT;

	uint16_t sorted[SAMPLE_COUNT];
	memcpy(sorted, samples, sizeof(samples));

	for(int i = 1; i < SAMPLE_COUNT; i++)
	{
		uint16_t key = sorted[i];
		int j = i - 1;
		while(j >= 0 && sorted[j] > key)
		{
			sorted[j + 1] = sorted[j];
			j--;
		}
		sorted[j + 1] = key;
	}

	uint32_t sum = 0;
	for(int i = DROP_COUNT; i < (SAMPLE_COUNT - DROP_COUNT); i++)
	{
		sum += sorted[i];
	}

	return (uint16_t)(sum >> 3);
}

void ina226_poll(uint32_t diff_ms)
{
	static uint32_t tmr = 0;
	tmr += diff_ms;
	if(tmr >= 20)
	{
		tmr = 0;

		ina226.cur_raw = reg_read(INA226_REG_SHUNT_VOLTAGE);
		ina226.volt_raw = reg_read(INA226_REG_BUS_VOLTAGE);
		ina226.volt_raw_filt = get_v_filt(ina226.volt_raw);

		ina226.cur = (float)ina226.cur_raw * CUR_SCALE; // FS: 8.2A
		ina226.volt = (float)ina226.volt_raw_filt * 0.00125f;

		if(++ina226.cnt_acq > 25)
		{
			ina226.ready = true;
		}
	}
}

void ina226_trace(print_func_t pf)
{
	const uint8_t a[] = {0, 1, 2, 3, 4, 5, 6, 7, 0xfe, 0xff};
	for(uint32_t i = 0; i < sizeof(a) / sizeof(a[0]); i++)
	{
		uint16_t v = reg_read(a[i]);
		pf("reg x%x: x%x\n", a[i], v);
	}
	pf("err: %d\n", ina226.err);
}

uint32_t ina226_get_alert_count(void) { return alert_cnt; }
bool ina226_is_alert(void) { return !gpio_get_level(GPIO_NUM_14); }
void ina226_reset_alert(void) { reg_read(INA226_REG_MASK_ENABLE); }

void ina226_cur_limit_en(bool en)
{
	if(en && ina226.al_en == false)
	{
		reg_write(INA226_REG_MASK_ENABLE, INA226_ALERT_SHUNT_OVERVOLTAGE | INA226_ALERT_LATCH);
	}
	else if(en == false && ina226.al_en)
	{
		reg_write(INA226_REG_MASK_ENABLE, 0);
	}
	ina226.al_en = en;
}