#ifndef CONSOLE_H__
#define CONSOLE_H__

#include <stdint.h>

#define CH_ENTER 0x0D
#define CH_CTRL_C 0x03
#define CH_CTRL_D 0x04
#define CH_CTRL_X 0x18
#define CH_CTRL_V 0x16
#define CH_CTRL_Z 0x1A

typedef enum
{
	CON_CB_OK = 0,
	CON_CB_SILENT,
	CON_CB_ERR_CUSTOM,
	CON_CB_ERR_ARGS,
	CON_CB_ERR_BAD_PARAM,
	CON_CB_ERR_UNSAFE,
	CON_CB_ERR_NO_SPACE,
} console_cmd_cb_res_t;

typedef void (*print_func_t)(const char *s, ...);
typedef void (*console_cmd_cb_t)(print_func_t, const char *, int, int *);

typedef struct
{
	const char *name;
	console_cmd_cb_t cb;
} console_cmd_t;
extern const console_cmd_t console_cmd[];
extern const uint32_t console_cmd_sz;

void console_init(void);
void console_cb(print_func_t pf, const char ch);

// utility callbacks
extern void con_cb_info(print_func_t pf, const char *req, int len, int *ret);
extern void con_cb_reset(print_func_t pf, const char *req, int len, int *ret);
extern void con_cb_wifi_list(print_func_t pf, const char *req, int len, int *ret);
extern void con_cb_wifi_conn(print_func_t pf, const char *req, int len, int *ret);
extern void con_cb_list_open_sockets(print_func_t pf, const char *req, int len, int *ret);
extern void con_cb_list_nvs(print_func_t pf, const char *req, int len, int *ret);
extern void con_cb_erase_user_nvs(print_func_t pf, const char *req, int len, int *ret);
extern void con_cb_tasks_info(print_func_t pf, const char *req, int len, int *ret);
extern void con_cb_partitions_switch(print_func_t pf, const char *req, int len, int *ret);
extern void con_cb_partitions_info(print_func_t pf, const char *req, int len, int *ret);
extern void con_cb_fw_approve(print_func_t pf, const char *req, int len, int *ret);

extern print_func_t pf_last;

#endif // CONSOLE_H__