#ifndef PROTO_H__
#define PROTO_H__

#include <stdint.h>

#define PROTO_CLOOP 0x01
#define PROTO_GRAPH 0x30

enum
{
	DRV_MODE_OFF = 0,
	DRV_MODE_ERROR = 1, // or rst err
	DRV_MODE_OL = 2,
	DRV_MODE_CURRENT = 3,
	DRV_MODE_VELOCITY = 4,
};

enum
{
	ERR_FAULT = 0,
	ERR_OC,
	ERR_UV,
	ERR_OV,
	ERR_OT,
	ERR_TO,
};

typedef struct __attribute__((packed))
{
	uint8_t mode;
	uint16_t err_mask;
	float ub;
	uint16_t pwm[3];
	float i[2];
	float enc[2];
	float vel;
	float pos;
	int8_t temp[3];
} drv_in_t;

typedef struct __attribute__((packed))
{
	uint8_t mode;
	float iq;
	float id;
	float pos;
	float vel;
	float vel_rate;
} drv_out_t;

typedef struct __attribute__((packed))
{
	drv_in_t in;
	drv_out_t out;
} drv_data_t;

static inline const char *mode2str(int mode)
{
	switch(mode)
	{
	case DRV_MODE_OFF: return "OFF";
	case DRV_MODE_OL: return "OL ";
	case DRV_MODE_CURRENT: return "CUR";
	case DRV_MODE_VELOCITY: return "VEL";
	case DRV_MODE_ERROR: return "ERR";
	default: return "Undef";
	}
}

static inline const char *err2str(uint8_t bit)
{
	switch(bit)
	{
	case ERR_FAULT: return "FLT";
	case ERR_OC: return "OC";
	case ERR_OV: return "OV";
	case ERR_UV: return "UV";
	case ERR_OT: return "OT";
	case ERR_TO: return "TO";
	default: return "Undef.";
	}
}

#endif // PROTO_H__