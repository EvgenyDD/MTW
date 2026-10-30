#ifndef FOC_H__
#define FOC_H__

#include "reg.h"
#include "utils_math.h"
#include <math.h>
#include <stdbool.h>
#include <string.h>

#define ISQRT3 (0.57735026919)

#define SVGENDQ_MACRO(uA, uB, T)                                \
	float tmp1 = (uB), tmp2 = (uB) * 0.5 + (SQRT3_BY_2 * (uA)); \
	float tmp3 = tmp2 - tmp1;                                   \
	float VecSector = 3;                                        \
	VecSector = (tmp2 > 0) ? (VecSector - 1) : VecSector;       \
	VecSector = (tmp3 > 0) ? (VecSector - 1) : VecSector;       \
	VecSector = (tmp1 < 0) ? (7 - VecSector) : VecSector;       \
	if(VecSector == 1 || VecSector == 4)                        \
	{                                                           \
		(T)[0] = tmp2, (T)[1] = tmp1 - tmp3, (T)[2] = -tmp2;    \
	}                                                           \
	else if(VecSector == 2 || VecSector == 5)                   \
	{                                                           \
		(T)[0] = tmp3 + tmp2, (T)[1] = tmp1, (T)[2] = -tmp1;    \
	}                                                           \
	else                                                        \
	{                                                           \
		(T)[0] = tmp3, (T)[1] = -tmp3, (T)[2] = -(tmp1 + tmp2); \
	}

typedef struct
{
	cplx_pi_reg_t i_reg;
	float ua, ub;
	float ud, uq;
	float id_fb, iq_fb;
	float ia, ib;
	float u_ph[3];
	float pwm[3]; // -1.0 ... 1.0
	float pwr;
	float pwr_filt;

	float id_fb_filt, iq_fb_filt;

	float _s, _c;

	bool was_foc_ctrl; // prev ctrl mode was FOC ctrl
} foc_ctrl_t;

static inline void foc_mon(volatile foc_ctrl_t *foc)
{
	if(foc->was_foc_ctrl)
	{
		cplx_pi_reg_reset(&foc->i_reg);
		foc->ua = foc->ub = 0;
		foc->ud = foc->uq = 0;
		foc->id_fb = foc->iq_fb = 0;
		foc->id_fb_filt = foc->iq_fb_filt = 0;
		foc->ia = foc->ib = 0;
		foc->u_ph[0] = foc->u_ph[1] = foc->u_ph[2] = 0;
		foc->pwm[0] = foc->pwm[1] = foc->pwm[2] = 0;
	}
	foc->was_foc_ctrl = false;
}

static inline float space_vector_modulation(float ua, float ub, float uc)
{
	int n = (ua > ub ? 1 : 0) |
			(ub > uc ? 2 : 0) |
			(uc > ua ? 4 : 0);
	switch(n)
	{
	case 1:
	case 6: return uc * 0.5f; break;
	case 2:
	case 5: return ua * 0.5f; break;
	case 4:
	case 3: return ub * 0.5f; break;
	default: return 0;
	}
}

static inline void foc_calc(volatile foc_ctrl_t *foc,
							float dt,
							float ub, // incl. max duty
							float iu, float iv,
							float iq, float id, float iq_comp,
							float we, // electrical velocity
							float ref_angle)
{
	foc->ia = iu;
	foc->ib = (iu + (iv * 2.0f)) * ONE_BY_SQRT3;

	float _cosine, _sine;
	utils_fast_sincos_better(ref_angle, &_sine, &_cosine);

	foc->id_fb = foc->ib * _sine + foc->ia * _cosine;
	foc->iq_fb = foc->ib * _cosine - foc->ia * _sine;

	UTILS_LP_FAST(foc->id_fb_filt, foc->id_fb, 0.05);
	UTILS_LP_FAST(foc->iq_fb_filt, foc->iq_fb, 0.05);

	cplx_pi_reg_update(&foc->i_reg, ub * ISQRT3,
					   id, iq + iq_comp,
					   foc->id_fb, foc->iq_fb,
					   0, 0,
					   we, dt);

	foc->ud = foc->i_reg.out_d;
	foc->uq = foc->i_reg.out_q;

	foc->ua = foc->ud * _cosine - foc->uq * _sine;
	foc->ub = foc->ud * _sine + foc->uq * _cosine;

	SVGENDQ_MACRO(foc->ua, foc->ub, foc->u_ph);

	foc->pwm[0] = foc->u_ph[0] / ub;
	foc->pwm[1] = foc->u_ph[1] / ub;
	foc->pwm[2] = foc->u_ph[2] / ub;
	foc->was_foc_ctrl = true;
}

#endif // FOC_H__