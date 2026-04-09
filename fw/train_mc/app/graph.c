#include "graph.h"
#include "platform.h"
#include <string.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"

#define MIN(a, b) ((a) <= (b) ? (a) : (b))

static volatile struct
{
	uint32_t samples_bg;
	uint32_t samples_fg;
	gr_var_t *vars[MAX_VAR_CNT];
	uint32_t vars_cnt;
	uint32_t sample_size;
	uint8_t trig; // GR_TRIG_*
	uint32_t decimation;
} set = {0};

static volatile uint8_t pool[GR_POOL_SZ];
static volatile uint8_t state = GR_ST_OFF; // GR_ST_*
static volatile uint32_t prev_sys_state = 0, prev_sys_err = 0, decimation_cnt = 0;
static volatile uint32_t samples_bg_collect = 0, samples_fg_collect = 0;
static volatile uint32_t ptr_start = 0, ptr_cur = 0; // raw bytes

static uint32_t vtype2sz[] = {0, 1, 1, 2, 4, 1, 2, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 8, 0, 0, 0, 0, 0, 8};

#define SW_TYPE(GR_TYPE, TYPE)                   \
	case GR_TYPE:                                \
	{                                            \
		TYPE vh = vt;                            \
		memcpy(&pool[ptr_cur], &vh, sizeof(vh)); \
		ptr_cur += vtype2sz[v->type_visual];     \
		break;                                   \
	}
//  data => (type_origin) * scale_capture => (type_visual) => heap
#define F_PACK(GR_TYPE, TYPE)                      \
	static void pack_##TYPE(float vt, gr_var_t *v) \
	{                                              \
		vt *= v->scale_capture;                    \
		switch(v->type_visual)                     \
		{                                          \
			SW_TYPE(GR_TYPE_BOOL, bool)            \
			SW_TYPE(GR_TYPE_INT8, int8_t)          \
			SW_TYPE(GR_TYPE_INT16, int16_t)        \
			SW_TYPE(GR_TYPE_INT32, int32_t)        \
			SW_TYPE(GR_TYPE_INT64, int64_t)        \
			SW_TYPE(GR_TYPE_UINT8, uint8_t)        \
			SW_TYPE(GR_TYPE_UINT16, uint16_t)      \
			SW_TYPE(GR_TYPE_UINT32, uint32_t)      \
			SW_TYPE(GR_TYPE_UINT64, uint64_t)      \
			SW_TYPE(GR_TYPE_REAL32, float)         \
			SW_TYPE(GR_TYPE_REAL64, double)        \
		default: break;                            \
		}                                          \
	}

F_PACK(GR_TYPE_BOOL, bool)
F_PACK(GR_TYPE_INT8, int8_t)
F_PACK(GR_TYPE_INT16, int16_t)
F_PACK(GR_TYPE_INT32, int32_t)
F_PACK(GR_TYPE_INT64, int64_t)
F_PACK(GR_TYPE_UINT8, uint8_t)
F_PACK(GR_TYPE_UINT16, uint16_t)
F_PACK(GR_TYPE_UINT32, uint32_t)
F_PACK(GR_TYPE_UINT64, uint64_t)
F_PACK(GR_TYPE_REAL32, float)
F_PACK(GR_TYPE_REAL64, double)

static void write_cb(gr_var_t *v)
{
	const uint32_t lim_bytes = ((uint32_t)(GR_POOL_SZ / set.sample_size)) * set.sample_size;
	v->getter(v, &pool[ptr_cur]);

	ptr_cur += v->size;
	if(ptr_cur >= lim_bytes) ptr_cur = 0;
}

static void write(gr_var_t *v)
{
	const uint32_t lim_bytes = ((uint32_t)(GR_POOL_SZ / set.sample_size)) * set.sample_size;

#undef SW_TYPE
#define SW_TYPE(GR_TYPE, TYPE)         \
	case GR_TYPE:                      \
	{                                  \
		TYPE vt;                       \
		memcpy(&vt, v->data, v->size); \
		pack_##TYPE(vt, v);            \
		break;                         \
	}

	switch(v->type_origin)
	{
		SW_TYPE(GR_TYPE_BOOL, bool)
		SW_TYPE(GR_TYPE_INT8, int8_t)
		SW_TYPE(GR_TYPE_INT16, int16_t)
		SW_TYPE(GR_TYPE_INT32, int32_t)
		SW_TYPE(GR_TYPE_INT64, int64_t)
		SW_TYPE(GR_TYPE_UINT8, uint8_t)
		SW_TYPE(GR_TYPE_UINT16, uint16_t)
		SW_TYPE(GR_TYPE_UINT32, uint32_t)
		SW_TYPE(GR_TYPE_UINT64, uint64_t)
		SW_TYPE(GR_TYPE_REAL32, float)
		SW_TYPE(GR_TYPE_REAL64, double)
	default: break;
	}
	if(ptr_cur >= lim_bytes)
	{
		ptr_cur = 0;
	}
}

static void upd_start_ptr(void)
{
	const uint32_t lim_bytes = ((uint32_t)(GR_POOL_SZ / set.sample_size)) * set.sample_size;

	if(ptr_cur >= samples_bg_collect * set.sample_size)
	{
		ptr_start = ptr_cur - samples_bg_collect * set.sample_size;
	}
	else
	{
		ptr_start = lim_bytes - samples_bg_collect * set.sample_size + ptr_cur;
	}
}

static uint32_t get_rec_tot(void)
{
	// return (ptr_cur >= ptr_start)
	// 		   ? ptr_cur - ptr_start
	// 		   : ptr_cur + set.sample_size * (set.samples_bg + set.samples_fg) - ptr_start;
	return (samples_bg_collect + samples_fg_collect) * set.sample_size;
}

void gr_set_state(uint32_t system_state, uint32_t err)
{
	if(system_state != prev_sys_state && set.trig == GR_TRIG_ST_CHNG)
	{
		if(state == GR_ST_REC_BG)
		{
			upd_start_ptr();
			state = GR_ST_REC_FG;
		}
	}
	else if(err != prev_sys_err && set.trig == GR_TRIG_ERR)
	{
		if(state == GR_ST_REC_BG)
		{
			upd_start_ptr();
			state = GR_ST_REC_FG;
		}
	}
	prev_sys_state = system_state;
	prev_sys_err = err;
}

void gr_poll(void)
{
	if(state == GR_ST_REC_BG || state == GR_ST_REC_FG)
	{
		if(++decimation_cnt >= set.decimation)
		{
			decimation_cnt = 0;
			if(state == GR_ST_REC_BG || state == GR_ST_REC_FG)
			{
				if(state == GR_ST_REC_BG)
				{
					for(uint32_t i = 0; i < set.vars_cnt; i++)
					{
						set.vars[i]->getter ? write_cb(set.vars[i]) : write(set.vars[i]);
					}
					if(++samples_bg_collect >= set.samples_bg)
					{
						if(set.trig == GR_TRIG_NOW)
						{
							state = GR_ST_REC_FG;
							upd_start_ptr();
						}
						samples_bg_collect = set.samples_bg;
					}
				}
				else if(state == GR_ST_REC_FG)
				{
					if(samples_fg_collect < set.samples_fg)
					{
						for(uint32_t i = 0; i < set.vars_cnt; i++)
						{
							write(set.vars[i]);
						}
						if(++samples_fg_collect >= set.samples_fg) state = GR_ST_OFF_SAVED;
					}
					else
					{
						state = GR_ST_OFF_SAVED;
					}
				}
			}
		}
	}
}

void gr_cb(const uint8_t *req, uint32_t req_len, uint8_t *resp, uint32_t *resp_len)
{
	if(!req_len || *resp_len < 2)
	{
		*resp_len = 0;
		return;
	}

	resp[0] = req[0]; // 1str resp is req cmd

	switch(req[0])
	{
	case GR_CMD_GET_STATE:
		resp[1] = state;
		*resp_len = 2;
		break;

	case GR_CMD_ENABLE:
		*resp_len = 2;
		if(req_len < 15)
		{
			resp[1] = GR_ERR_LEN;
		}
		else if(req[13] > GR_TRIG_ERR)
		{
			resp[1] = GR_ERR_PARAM;
		}
		else
		{
			ptr_start = ptr_cur = 0;
			samples_bg_collect = samples_fg_collect = 0;
			memset(&set, 0, sizeof(set));
			state = GR_ST_OFF;
			decimation_cnt = 0;

			memcpy(&set.samples_bg, &req[1], 4);
			memcpy(&set.samples_fg, &req[5], 4);
			memcpy(&set.decimation, &req[9], 4); // 0 is same as 1
			set.trig = req[13];
			set.vars_cnt = req_len - 14;
			if(set.vars_cnt > MAX_VAR_CNT)
			{
				resp[1] = GR_ERR_PARAM;
				return;
			}
			for(uint32_t i = 0; i < set.vars_cnt; i++)
			{
				if(req[14 + i] > gr_vars_size)
				{
					resp[1] = GR_ERR_PARAM;
					return;
				}
				set.vars[i] = &gr_vars[req[14 + i]];

				set.sample_size += set.vars[i]->getter
									   ? set.vars[i]->size
									   : vtype2sz[set.vars[i]->type_visual];
			}
			uint32_t tot_samples = GR_POOL_SZ / set.sample_size;
			if(set.samples_bg > tot_samples) set.samples_bg = tot_samples;
			if(set.samples_fg > tot_samples - set.samples_bg) set.samples_fg = tot_samples - set.samples_bg;
			state = GR_ST_REC_BG;
			resp[1] = GR_ERR_OK;
		}
		break;

	case GR_CMD_DISABLE:
		if(state == GR_ST_REC_BG || state == GR_ST_REC_FG) state = GR_ST_OFF_SAVED;
		resp[1] = state;
		*resp_len = 2;
		break;

	case GR_CMD_GET_HDR:
	{
		if(*resp_len < 14 + set.vars_cnt)
		{
			resp[1] = GR_ERR_LEN;
			*resp_len = 2;
			return;
		}
		memcpy(&resp[1], &samples_bg_collect, 4); // cnt bg
		memcpy(&resp[5], &samples_fg_collect, 4); // cnt fg
		float freq = (float)platform_get_loop_freq() / (float)(set.decimation == 0 ? 1 : set.decimation);
		memcpy(&resp[9], &freq, 4); // freq
		resp[13] = set.trig;
		for(uint32_t i = 0; i < set.vars_cnt; i++)
			for(uint32_t j = 0; j < gr_vars_size; j++)
			{
				if(set.vars[i] == &gr_vars[j])
				{
					resp[14 + i] = j;
					break;
				}
			}
		*resp_len = 14 + set.vars_cnt;
		break;
	}

	case GR_CMD_GET_VAR:
		if(req_len != 2 || *resp_len < 8)
		{
			resp[1] = GR_ERR_LEN;
			*resp_len = 2;
			return;
		}
		else if(req[1] >= gr_vars_size)
		{
			resp[1] = GR_ERR_PARAM;
			*resp_len = 2;
			return;
		}
		else
		{
			resp[1] = gr_vars[req[1]].type_visual;
			memcpy(&resp[2], &gr_vars[req[1]].scale_capture, 4);
			size_t name_len = strlen(gr_vars[req[1]].name);
			name_len = MIN(name_len, *resp_len - 7);
			name_len = MIN(name_len, MAX_VAR_NAME_LEN);
			strncpy((char *)&resp[6], gr_vars[req[1]].name, name_len);
			*resp_len = 6 + name_len;
		}
		break;

	case GR_CMD_GET_DATA:
		if(state != GR_ST_OFF_SAVED)
		{
			resp[1] = GR_ERR_BUSY;
			*resp_len = 2;
			return;
		}
		if(state == GR_ST_OFF)
		{
			resp[1] = GR_ERR_EMPTY;
			*resp_len = 2;
			return;
		}
		if(req_len != 1 + 4 || *resp_len < 6)
		{
			resp[1] = GR_ERR_LEN;
			*resp_len = 2;
			return;
		}
		uint32_t off, sz = *resp_len - 5;
		memcpy(&off, &req[1], 4);
		const uint32_t lim = ((uint32_t)(GR_POOL_SZ / set.sample_size)) * set.sample_size;
		if(off > lim)
		{
			resp[1] = GR_ERR_OVF;
			*resp_len = 2;
			return;
		}
		memcpy(&resp[1], &off, 4);
		if(off + sz > get_rec_tot()) sz = (off >= get_rec_tot()) ? 0 : get_rec_tot() - off;
		if(sz)
		{
			uint32_t p = 5;
			if(ptr_start + off + sz >= lim)
			{
				uint32_t sz_from_0 = sz, to_copy = 0;
				if(ptr_start + off < lim)
				{
					to_copy = lim - (ptr_start + off);
					if(to_copy > sz) to_copy = sz;
					if(to_copy) memcpy(&resp[p], &pool[ptr_start + off], to_copy);
					p += to_copy;
					sz_from_0 -= to_copy;
				}
				if(sz) memcpy(&resp[p], &pool[off - lim + ptr_start + to_copy], sz_from_0);
			}
			else
			{
				if(sz) memcpy(&resp[p], &pool[ptr_start + off], sz);
			}
		}
		*resp_len = 5 + sz;
		break;

	case GR_CMD_GET_POOL_SZ:
		if(*resp_len < 1 + 4)
		{
			resp[1] = GR_ERR_LEN;
			*resp_len = 2;
			return;
		}
		uint32_t hp_sz = GR_POOL_SZ;
		memcpy(&resp[1], &hp_sz, 4);
		*resp_len = 1 + 4;
		break;

	default: *resp_len = 0; break;
	}
}
#pragma GCC diagnostic pop