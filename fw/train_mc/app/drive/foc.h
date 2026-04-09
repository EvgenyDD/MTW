#ifndef FOC_H__
#define FOC_H__

#include "reg.h"
#include "utils_math.h"
#include <math.h>
#include <stdbool.h>
#include <string.h>

#define MAX_DUTY 0.95f

#define ONE_BY_SQRT3 (0.57735026919)

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

	bool was_foc_ctrl; // prev ctrl mode was FOC ctrl
} foc_ctrl_t;

static inline void foc_mon(volatile foc_ctrl_t *foc)
{
	if(foc->was_foc_ctrl) CplxPiRegReset(&foc->i_reg);
	foc->was_foc_ctrl = false;
}

static inline void foc_calc(volatile foc_ctrl_t *foc,
							float dt,
							float ub,
							float iu, float iv,
							float iq, float id, float iq_comp,
							float we,		 // electrical velocity
							float ref_angle) // angle rad
{
	foc->ia = iu;
	foc->ib = (iu + (iv * 2.0f)) * ONE_BY_SQRT3;

	float _cosine, _sine;
	utils_fast_sincos_better(ref_angle, &_sine, &_cosine);

	foc->id_fb = foc->ib * _sine + foc->ia * _cosine;
	foc->iq_fb = foc->ib * _cosine - foc->ia * _sine;

	CplxPiRegUpdate(&foc->i_reg, ub * MAX_DUTY,
					id, iq + iq_comp,
					foc->id_fb, foc->iq_fb,
					0, 0,
					-we, dt);

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