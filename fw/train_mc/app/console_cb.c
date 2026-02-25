#include "adc.h"
#include "config_system.h"
#include "console.h"
#include "drive_hal.h"
#include "fw_header.h"
#include "graph.h"
#include "platform.h"
#include <stdio.h>

extern uint8_t pending_can_node_id;

extern uint32_t special_counter, so;

static void info_cb(const char *req, int len, int *ret)
{
	for(uint32_t i = 0; i < FW_COUNT; i++)
	{
		_console_print("====== FW %s (%d) =====\n", ((const char *[]){"PREBOOT", "BOOT", "APP"})[i], g_fw_info[i].locked);
		if(g_fw_info[i].locked) break;
		_console_print("\tAddr: x%x | Size: %ld\n", g_fw_info[i].addr, g_fw_info[i].size);
		_console_print("\tVer: %ld.%ld.%ld\n", g_fw_info[i].ver_major, g_fw_info[i].ver_minor, g_fw_info[i].ver_patch);
		if(g_fw_info[i].field_product_ptr) _console_print("\tProd: %s\n", g_fw_info[i].field_product_ptr);
		if(g_fw_info[i].field_product_name_ptr) _console_print("\tName: %s\n", g_fw_info[i].field_product_name_ptr);
		const char *s = fw_fields_find_by_key_helper(&g_fw_info[i], "build_ts");
		if(s) _console_print("\tBuild: %s\n", s);
	}
	_console_print("UID: x%08lx.x%08lx.x%08lx\n", g_uid[0], g_uid[1], g_uid[2]);
	_console_print("Time: %ld\n", system_time_ms);
}

static void save_cb(const char *req, int len, int *ret)
{
	int sts = config_write_storage();
	_console_print("saved: %d\n", sts);
}

static void adc_cb(const char *req, int len, int *ret)
{
	_console_print("I: %.4f %.4f %d %d\n", g_adc_val.i[0], g_adc_val.i[1], g_adc_val.i_raw[0], g_adc_val.i_raw[1]);
	_console_print("Ub: %.2f\n", g_adc_val.ub);
	_console_print("Tm: %.1f Tinv: %.1f Tcpu: %.1f\n", g_adc_val.tmtr, g_adc_val.tinv, g_adc_val.tcpu);
}

const console_cmd_t console_cmd[] = {
	{"info", info_cb},
	{"adc", adc_cb},
	{"save", save_cb},
};
const uint32_t console_cmd_sz = sizeof(console_cmd) / sizeof(console_cmd[0]);