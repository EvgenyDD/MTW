#ifndef GRAPH_H__
#define GRAPH_H__

#include "platform.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MAX_VAR_CNT 16
#define MAX_VAR_NAME_LEN 16

#define GR_TYPE_BOOL 0x01
#define GR_TYPE_INT8 0x02
#define GR_TYPE_INT16 0x03
#define GR_TYPE_INT32 0x04
#define GR_TYPE_INT64 0x15
#define GR_TYPE_UINT8 0x05
#define GR_TYPE_UINT16 0x06
#define GR_TYPE_UINT32 0x07
#define GR_TYPE_UINT64 0x1B
#define GR_TYPE_REAL32 0x08
#define GR_TYPE_REAL64 0x11

enum
{
	GR_CMD_GET_STATE = 0,
	GR_CMD_ENABLE,
	GR_CMD_DISABLE,
	GR_CMD_GET_HDR,
	GR_CMD_GET_VAR,
	GR_CMD_GET_DATA,
	GR_CMD_GET_POOL_SZ,
};

enum
{
	GR_ERR_OK = 0,
	GR_ERR_PARAM,
	GR_ERR_LEN,
	GR_ERR_OVF,
	GR_ERR_BUSY,  // capture active
	GR_ERR_EMPTY, // no data captured
};

enum
{
	GR_TRIG_NOW = 0,
	GR_TRIG_ST_CHNG,
	GR_TRIG_ERR,
};

enum
{
	GR_ST_OFF = 0,
	GR_ST_OFF_SAVED,
	GR_ST_REC_BG, // record in background (before event)
	GR_ST_REC_FG, // record in foreground (after event)
};

typedef struct gr_var_t gr_var_t;
struct gr_var_t
{
	const void *data;
	uint8_t size;						  // data size (or size of data if "getter" is present)
	uint8_t type_origin;				  // used by graph engine
	uint8_t type_visual;				  // used to parse result data
	float scale_capture;				  //    data => (type_origin) * scale_capture => (type_visual) => heap
										  //    heap => (type_visual) / scale_capture => disp_data
	char name[MAX_VAR_NAME_LEN];		  // name len
	void (*getter)(gr_var_t *v, void *p); // memcpy(ptr, &data, v->size);
};

void gr_set_state(uint32_t system_state, uint32_t err);
void gr_poll(void);
void gr_cb(const uint8_t *req, uint32_t req_len, uint8_t *resp, uint32_t *resp_len);

extern void gr_cb_init(void);

extern gr_var_t gr_vars[];
extern const uint32_t gr_vars_size;

#endif // GRAPH_H__