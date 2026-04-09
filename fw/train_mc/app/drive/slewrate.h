#ifndef SLEWRATE_H_
#define SLEWRATE_H_

#include "utils.h"

typedef struct
{
	// Rate s^-1
	float rate_up;
	float rate_dwn;
} t_slew_rate_param;

typedef struct
{
	t_slew_rate_param *p;
	float acc;
} t_slew_rate;

inline float SlewRate(t_slew_rate *sl_rate, float inp, float dt)
{
	sl_rate->acc += CLIP(inp - sl_rate->acc, -sl_rate->p->rate_dwn * dt, sl_rate->p->rate_up * dt);
	return sl_rate->acc;
}

inline void SlewRateInit(t_slew_rate *sl_rate, t_slew_rate_param *sl_rate_param)
{
	sl_rate->p = sl_rate_param;
	sl_rate->acc = 0;
}

#define SlewRateReset(s) (s)->acc = 0
#define SlewRateSet(s, v) (s)->acc = v

#endif /* SLEWRATE_H_ */
