#ifndef DRIVE_H__
#define DRIVE_H__

#include "dcc_def.h"
#include "dcc_hw.h"
#include <stdbool.h>
#include <stdint.h>

// hw
void dcc_hw_init(void);
void dcc_cd_add_bit(bool is_1);
void dcc_cd_add_cutout(void);

// Decoder
void dcc_dc_push(int8_t pulse);
void dcc_dc_poll(void);

// Coder
void dcc_cd_add_pkt(const uint8_t *data, uint32_t sz);
void dcc_cd_cmd_custom(uint8_t addr, uint8_t cmd, const uint8_t *data, uint32_t sz);
void dcc_cd_cmd_advanced(uint8_t addr, uint8_t cmd, uint8_t val);
void dcc_cd_cmd_speed_128(uint8_t addr, int8_t speed, bool emcy_stop);
void dcc_cd_cmd_speed_28(uint8_t addr, int8_t speed, bool emcy_stop);
void dcc_cd_cmd_analog_func(uint8_t addr, uint8_t out, uint8_t value);
void dcc_cd_cmd_func_grp1(uint8_t addr, uint8_t value);
void dcc_cd_cmd_func_grp2(uint8_t addr, uint8_t value);

// Decoder CB
void dcc_dc_cb_spd_128(uint8_t code);
void dcc_dc_cb_an_func(uint8_t addr, uint8_t value);
void dcc_dc_cb_spd28_rev(uint8_t value);
void dcc_dc_cb_spd28_fwd(uint8_t value);

/////

#ifdef PROTO_DEBUG
#define PRINTF(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define PRINTF(fmt, ...) ;
#endif

/////

extern uint8_t g_dcc_self_id;

#endif // DRIVE_H__