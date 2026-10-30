#include "console.h"
#include "dcc.h"
#include "esp_ota_ops.h"
#include "ina226.h"
#include "sh1107.h"

extern void dcc_diag(print_func_t pf);

void con_diag(print_func_t pf, const char *req, int len, int *ret)
{
	dcc_diag(pf);
}

void con_ina(print_func_t pf, const char *req, int len, int *ret)
{
	ina226_trace(pf);
}

const console_cmd_t console_cmd[] = {
	{"info", con_cb_info},
	{"reset", con_cb_reset},
	{"restart", con_cb_reset},
	{"wifi_list", con_cb_wifi_list},
	{"wifi_connect", con_cb_wifi_conn},
	{"sock", con_cb_list_open_sockets},
	{"nvs", con_cb_list_nvs},
	{"nvs_erase", con_cb_erase_user_nvs},
	{"tasks", con_cb_tasks_info},
	{"partitions", con_cb_partitions_info},
	{"switch_partitions", con_cb_partitions_switch},
	{"approve", con_cb_fw_approve},
	{"diag", con_diag},
	{"ina", con_ina},
	//--------------------------------
	//--------------------------------
};
const uint32_t console_cmd_sz = sizeof(console_cmd) / sizeof(console_cmd[0]);