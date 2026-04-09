#include "console.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "lwip/sockets.h"
#include "nvs.h"
#include "web/wifi.h"
#include "web/ws.h"
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <string.h>

#define CHK(condition, format, ...) \
	if(condition)                   \
	{                               \
		pf(format, ##__VA_ARGS__);  \
		return;                     \
	}

void con_cb_fw_approve(print_func_t pf, const char *req, int len, int *ret)
{
	esp_ota_mark_app_valid_cancel_rollback(); // enable CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
	pf("FW approve OK");
}

void con_cb_info(print_func_t pf, const char *req, int len, int *ret)
{
	pf("Running on core: %d", xPortGetCoreID());
	pf("Free heap: %d, minimum: %ld bytes", xPortGetFreeHeapSize(), esp_get_minimum_free_heap_size());

	esp_chip_info_t chip_info;
	esp_chip_info(&chip_info);
	uint32_t size_flash_chip;
	esp_flash_get_size(NULL, &size_flash_chip);
	pf("This is %s chip with %d CPU core(s), WiFi%s%s, silicon revision: %d, %ldMB %s flash",
	   CONFIG_IDF_TARGET,
	   chip_info.cores,
	   (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
	   (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "",
	   chip_info.revision,
	   size_flash_chip / (1024 * 1024), (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
}

void con_cb_reset(print_func_t pf, const char *req, int len, int *ret) { esp_restart(); }

void con_cb_wifi_list(print_func_t pf, const char *req, int len, int *ret)
{
	pf("Count of scanned networks: %d", wifi_records_cnt);
	for(uint16_t i = 0; i < wifi_records_cnt; i++)
	{
		pf("\tWiFi AP (%d): %s (%d dBm)", i, wifi_records[i].ssid, wifi_records[i].rssi);
	}
}

void con_cb_wifi_conn(print_func_t pf, const char *req, int len, int *ret)
{
	CHK(len == 0, "Error! len=0");
	int p = 0, ap = 0;
	for(; p < len; p++)
	{
		if(req[p] == ' ')
		{
			ap = atoi(req);
			p++;
			break;
		}
	}
	CHK(p >= len, "Error! p>=len");
	CHK(ap < 0 || ap >= wifi_records_cnt, "ERROR! Wrong selector: %d", ap);
	pf("Trying connect to WiFi AP %s (%d dBm) with pass \"%s\"", wifi_records[ap].ssid, wifi_records[ap].rssi, &req[p]);

	nvs_handle_t handle;
	esp_err_t err;
	err = nvs_open("app_config", NVS_READWRITE, &handle);
	CHK(err != ESP_OK, "Error! Can't open NVS ws");

	err = nvs_set_str(handle, "sta_ssid", (const char *)wifi_records[ap].ssid);
	if(err != ESP_OK)
	{
		nvs_close(handle);
		pf("Error! Can't save AP SSID");
		return;
	}
	err = nvs_set_str(handle, "sta_pass", &req[p]);
	if(err != ESP_OK)
	{
		nvs_close(handle);
		pf("Error! Can't save AP pswd");
		return;
	}

	ESP_ERROR_CHECK(nvs_commit(handle));
	nvs_close(handle);
}

void con_cb_list_open_sockets(print_func_t pf, const char *req, int len, int *ret)
{
	for(int i = 0; i < LWIP_SOCKET_OFFSET + 10; i++)
	{
		int err;
		socklen_t len = sizeof(err);
		if(getsockopt(i, SOL_SOCKET, SO_ERROR, &err, &len) == 0)
		{
			struct sockaddr_in addr;
			socklen_t addr_len = sizeof(addr);
			if(getpeername(i, (struct sockaddr *)&addr, &addr_len) == 0)
			{
				pf("Socket %d: Connected to %s:%d", i, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
			}
			else if(getsockname(i, (struct sockaddr *)&addr, &addr_len) == 0)
			{
				pf("Socket %d: Listening on port %d", i, ntohs(addr.sin_port));
			}
			else
			{
				pf("Socket %d: Open but no connection info", i);
			}
		}
	}
}

static const char *nvs_type_to_str(nvs_type_t type)
{
	switch(type)
	{
	case NVS_TYPE_I8: return "i8";
	case NVS_TYPE_U8: return "u8";
	case NVS_TYPE_I16: return "i16";
	case NVS_TYPE_U16: return "u16";
	case NVS_TYPE_I32: return "i32";
	case NVS_TYPE_U32: return "u32";
	case NVS_TYPE_I64: return "i64";
	case NVS_TYPE_U64: return "u64";
	case NVS_TYPE_STR: return "str";
	case NVS_TYPE_BLOB: return "blob";
	case NVS_TYPE_ANY: return "any";
	default: return "unknown";
	}
}

static void list_ns(print_func_t pf, const char *name, bool print_header)
{
	if(print_header) pf("%-20s | %-25s | %-8s | %s [Namespace %s]", "Namespace", "Key", "Type", "Value", name);
	pf("----------------------------------------------------------------");
	nvs_handle_t ns_handle;
	if(nvs_open(name, NVS_READONLY, &ns_handle) != ESP_OK) return;

	nvs_iterator_t it = NULL;
	esp_err_t err = nvs_entry_find(NVS_DEFAULT_PART_NAME, name, NVS_TYPE_ANY, &it);

	while(err == ESP_OK)
	{
		nvs_entry_info_t info;
		nvs_entry_info(it, &info);

		switch(info.type)
		{
		case NVS_TYPE_I8:
		{
			int8_t val;
			if(nvs_get_i8(ns_handle, info.key, &val) == ESP_OK) pf("%-20s | %-25s | %-8s | %d", info.namespace_name, info.key, nvs_type_to_str(info.type), val);
			break;
		}
		case NVS_TYPE_U8:
		{
			uint8_t val;
			if(nvs_get_u8(ns_handle, info.key, &val) == ESP_OK) pf("%-20s | %-25s | %-8s | %u", info.namespace_name, info.key, nvs_type_to_str(info.type), val);
			break;
		}
		case NVS_TYPE_I16:
		{
			int16_t val;
			if(nvs_get_i16(ns_handle, info.key, &val) == ESP_OK) pf("%-20s | %-25s | %-8s | %d", info.namespace_name, info.key, nvs_type_to_str(info.type), val);
			break;
		}
		case NVS_TYPE_U16:
		{
			uint16_t val;
			if(nvs_get_u16(ns_handle, info.key, &val) == ESP_OK) pf("%-20s | %-25s | %-8s | %u", info.namespace_name, info.key, nvs_type_to_str(info.type), val);
			break;
		}
		case NVS_TYPE_I32:
		{
			int32_t val;
			if(nvs_get_i32(ns_handle, info.key, &val) == ESP_OK) pf("%-20s | %-25s | %-8s | %ld", info.namespace_name, info.key, nvs_type_to_str(info.type), val);
			break;
		}
		case NVS_TYPE_U32:
		{
			uint32_t val;
			if(nvs_get_u32(ns_handle, info.key, &val) == ESP_OK) pf("%-20s | %-25s | %-8s | %lu", info.namespace_name, info.key, nvs_type_to_str(info.type), val);
			break;
		}
		case NVS_TYPE_I64:
		{
			int64_t val;
			if(nvs_get_i64(ns_handle, info.key, &val) == ESP_OK) pf("%-20s | %-25s | %-8s | %lld", info.namespace_name, info.key, nvs_type_to_str(info.type), val);
			break;
		}
		case NVS_TYPE_U64:
		{
			uint64_t val;
			if(nvs_get_u64(ns_handle, info.key, &val) == ESP_OK) pf("%-20s | %-25s | %-8s | %llu", info.namespace_name, info.key, nvs_type_to_str(info.type), val);
			break;
		}
		case NVS_TYPE_STR:
		{
			size_t required_size;
			if(nvs_get_str(ns_handle, info.key, NULL, &required_size) == ESP_OK)
			{
				char *str_val = malloc(required_size);
				if(nvs_get_str(ns_handle, info.key, str_val, &required_size) == ESP_OK) pf("%-20s | %-25s | %-8s | %s", info.namespace_name, info.key, nvs_type_to_str(info.type), str_val);
				free(str_val);
			}
			break;
		}
		case NVS_TYPE_BLOB:
		{
			size_t required_size;
			if(nvs_get_blob(ns_handle, info.key, NULL, &required_size) == ESP_OK) pf("%-20s | %-25s | %-8s | [BLOB %d bytes]", info.namespace_name, info.key, nvs_type_to_str(info.type), (int)required_size);
			break;
		}
		default:
			pf("%-20s | %-25s | %-8s | [UNKNOWN TYPE]", info.namespace_name, info.key, nvs_type_to_str(info.type));
			break;
		}
		err = nvs_entry_next(&it);
	}

	nvs_close(ns_handle);
	if(it != NULL) nvs_release_iterator(it);
}

void con_cb_list_nvs(print_func_t pf, const char *req, int len, int *ret)
{
#define MAX_NAMESPACES 64
	char *namespaces[MAX_NAMESPACES];
	int ns_count = 0;

	nvs_iterator_t it = NULL;
	esp_err_t err = nvs_entry_find(NVS_DEFAULT_PART_NAME, NULL, NVS_TYPE_ANY, &it);

	while(err == ESP_OK)
	{
		nvs_entry_info_t info;
		nvs_entry_info(it, &info);
		bool found = false;
		for(int i = 0; i < ns_count; i++)
		{
			if(strcmp(namespaces[i], info.namespace_name) == 0)
			{
				found = true;
				break;
			}
		}

		if(!found && ns_count < MAX_NAMESPACES)
		{
			namespaces[ns_count] = strdup(info.namespace_name);
			ns_count++;
			// pf("%d. %s", ns_count, info.namespace_name);
			list_ns(pf, info.namespace_name, ns_count == 0);
		}
		err = nvs_entry_next(&it);
	}

	ns_count == 0 ? pf("No namespaces found in NVS") : pf("Total: %d namespace", ns_count);
	for(int i = 0; i < ns_count; i++)
	{
		free(namespaces[i]);
	}

	if(it != NULL) nvs_release_iterator(it);
}

void con_cb_erase_user_nvs(print_func_t pf, const char *req, int len, int *ret)
{
	nvs_handle_t handle;
	esp_err_t err = nvs_open("app_config", NVS_READWRITE, &handle);
	if(err != ESP_OK)
	{
		pf("Error opening NVS namespace: %s", esp_err_to_name(err));
		return;
	}

	err = nvs_erase_all(handle);
	if(err != ESP_OK)
	{
		pf("Error erasing namespace: %s", esp_err_to_name(err));
		nvs_close(handle);
		return;
	}

	err = nvs_commit(handle);
	nvs_close(handle);
	if(err != ESP_OK)
	{
		pf("Error committing erase: %s", esp_err_to_name(err));
		return;
	}
}

void con_cb_tasks_info(print_func_t pf, const char *req, int len, int *ret)
{
	UBaseType_t num_tasks = uxTaskGetNumberOfTasks();

	// Allocate memory for task status array
	TaskStatus_t *task_status_array = malloc(num_tasks * sizeof(TaskStatus_t));
	if(task_status_array == NULL)
	{
		pf("Failed to allocate memory for task status array");
		return;
	}

	num_tasks = uxTaskGetSystemState(task_status_array, num_tasks, NULL);

	pf("==================================================");
	pf("%d tasks", num_tasks);
	pf("==================================================");
	pf("%-20s %-10s %-8s %-8s %s", "Name", "State", "Priority", "Stack", "Task Number");
	pf("--------------------------------------------------");

	for(UBaseType_t i = 0; i < num_tasks; i++)
	{
		TaskStatus_t *task = &task_status_array[i];
		const char *state_str;
		switch(task->eCurrentState)
		{
		case eRunning: state_str = "Running"; break;
		case eReady: state_str = "Ready"; break;
		case eBlocked: state_str = "Blocked"; break;
		case eSuspended: state_str = "Suspended"; break;
		case eDeleted: state_str = "Deleted"; break;
		case eInvalid: state_str = "Invalid"; break;
		default: state_str = "Unknown"; break;
		}
		pf("%-20s %-10s %-8d %-8u %u", task->pcTaskName, state_str, task->uxCurrentPriority, (unsigned int)task->usStackHighWaterMark, task->xTaskNumber);
	}

	pf("==================================================");
	free(task_status_array);
}

static const char *ota_state_to_str(esp_ota_img_states_t state)
{
	switch(state)
	{
	case ESP_OTA_IMG_NEW: return "NEW";
	case ESP_OTA_IMG_PENDING_VERIFY: return "PEND_VERIFY";
	case ESP_OTA_IMG_VALID: return "VALID";
	case ESP_OTA_IMG_INVALID: return "INVALID";
	case ESP_OTA_IMG_ABORTED: return "ABORTED";
	case ESP_OTA_IMG_UNDEFINED: return "UNDEFINED";
	default: return "UNKNOWN";
	}
}

static const char *ota_app_subtype_to_str(uint32_t subtype)
{
	switch(subtype)
	{
	case ESP_PARTITION_SUBTYPE_APP_FACTORY: return "FACTORY";
	case ESP_PARTITION_SUBTYPE_APP_OTA_0: return "OTA_0";
	case ESP_PARTITION_SUBTYPE_APP_OTA_1: return "OTA_1";
	case ESP_PARTITION_SUBTYPE_APP_OTA_2: return "OTA_2";
	case ESP_PARTITION_SUBTYPE_APP_OTA_3: return "OTA_3";
	case ESP_PARTITION_SUBTYPE_APP_OTA_4: return "OTA_4";
	case ESP_PARTITION_SUBTYPE_APP_OTA_5: return "OTA_5";
	case ESP_PARTITION_SUBTYPE_APP_OTA_6: return "OTA_6";
	case ESP_PARTITION_SUBTYPE_APP_OTA_7: return "OTA_7";
	case ESP_PARTITION_SUBTYPE_APP_OTA_8: return "OTA_8";
	case ESP_PARTITION_SUBTYPE_APP_OTA_9: return "OTA_9";
	case ESP_PARTITION_SUBTYPE_APP_OTA_10: return "OTA_10";
	case ESP_PARTITION_SUBTYPE_APP_OTA_11: return "OTA_11";
	case ESP_PARTITION_SUBTYPE_APP_OTA_12: return "OTA_12";
	case ESP_PARTITION_SUBTYPE_APP_OTA_13: return "OTA_13";
	case ESP_PARTITION_SUBTYPE_APP_OTA_14: return "OTA_14";
	case ESP_PARTITION_SUBTYPE_APP_OTA_15: return "OTA_15";
	case ESP_PARTITION_SUBTYPE_APP_TEST: return "TEST";
	default: return "Unknown";
	}
}

static const char *ota_data_subtype_to_str(uint32_t subtype)
{
	switch(subtype)
	{
	case ESP_PARTITION_SUBTYPE_DATA_OTA: return "OTA";
	case ESP_PARTITION_SUBTYPE_DATA_PHY: return "PHY";
	case ESP_PARTITION_SUBTYPE_DATA_NVS: return "NVS";
	case ESP_PARTITION_SUBTYPE_DATA_COREDUMP: return "COREDUMP";
	case ESP_PARTITION_SUBTYPE_DATA_NVS_KEYS: return "NVS_KEYS";
	case ESP_PARTITION_SUBTYPE_DATA_EFUSE_EM: return "EFUSE_EM";
	case ESP_PARTITION_SUBTYPE_DATA_UNDEFINED: return "UNDEFINED";
	case ESP_PARTITION_SUBTYPE_DATA_ESPHTTPD: return "ESPHTTPD";
	case ESP_PARTITION_SUBTYPE_DATA_FAT: return "FAT";
	case ESP_PARTITION_SUBTYPE_DATA_SPIFFS: return "SPIFFS";
	case ESP_PARTITION_SUBTYPE_DATA_LITTLEFS: return "LITTLEFS";
	default: return "Unknown";
	}
}

void con_cb_partitions_switch(print_func_t pf, const char *req, int len, int *ret)
{
	const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);
	if(next == NULL)
	{
		pf("Failed to set boot partition");
		return;
	}
	esp_err_t err = esp_ota_set_boot_partition(next);
	if(err == ESP_OK)
	{
		pf("OK, next boot will be from %s", next->label);
	}
	else
	{
		pf("Failed to set boot partition: %s", esp_err_to_name(err));
	}
	return;
}

void con_cb_partitions_info(print_func_t pf, const char *req, int len, int *ret)
{
	pf("===== Partitions =====");

	pf("%-12s | %-8s | %-8s | %-5s | %-8s | %-12s | %-12s | %-8s",
	   "Name", "Running", "Next upd", "Type", "Subtype", "State", "Addr", "Size");
	const esp_partition_t *partition = NULL;
	const esp_partition_t *running = esp_ota_get_running_partition();
	const esp_partition_t *next_update = esp_ota_get_next_update_partition(NULL);
	esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
	while(it != NULL)
	{
		partition = esp_partition_get(it);

		esp_ota_img_states_t ota_state = ESP_OTA_IMG_UNDEFINED;
		esp_ota_get_state_partition(partition, &ota_state);

		pf(" %-11s | %-8s | %-8s | %-5s | %-8s | %-12s | x%-11x | %8d (%d kB)",
		   partition->label,
		   partition == running ? "   +" : "",
		   partition == next_update ? "   +" : "",
		   partition->type == 0 ? "APP" : "DATA",
		   partition->type == 0 ? ota_app_subtype_to_str(partition->subtype) : ota_data_subtype_to_str(partition->subtype),
		   ota_state != ESP_OTA_IMG_UNDEFINED ? ota_state_to_str(ota_state) : "",
		   partition->address,
		   partition->size,
		   partition->size / 1024);

		it = esp_partition_next(it);
	}
	if(it != NULL) esp_partition_iterator_release(it);
	pf("================================");
}