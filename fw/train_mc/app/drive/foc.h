#ifndef FOC_H__
#define FOC_H__

#include "reg.h"
#include "utils_math.h"
#include <math.h>
#include <stdbool.h>
#include <string.h>

#define MAX_DUTY 0.95f

#define ONE_BY_SQRT3 (0.57735026919)

#define CLARKE_MACRO(i1, i2, iA, iB) \
	(iA) = (i1);                     \
	(iB) = ((i1) + ((i2) * 2.0f)) * ONE_BY_SQRT3;

#define PARK_MACRO(iA, iB, angle, iD, iQ)              \
	utils_fast_sincos_better(angle, &_sine, &_cosine); \
	(iD) = (iB) * _sine + (iA) * _cosine;              \
	(iQ) = (iB) * _cosine - (iA) * _sine;

#define IPARK_MACRO(uD, uQ, uA, uB)       \
	(uA) = (uD) * _cosine - (uQ) * _sine; \
	(uB) = (uD) * _sine + (uQ) * _cosine;

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
	float pwm[3];

	bool was_foc_ctrl; // prev ctrl mode was FOC ctrl
} foc_ctrl_t;

static inline void foc(foc_ctrl_t *foc,
					   bool is_foc,
					   float dt,
					   float freq,
					   const float vb,
					   float iq,
					   float id,
					   const float velElectricalRad,
					   const float ref_angle, // rad
					   const float iu,
					   const float iv,
					   const float i_comp)
{
	if(is_foc)
	{
		float _cosine, _sine;
		CLARKE_MACRO(iu, iv, foc->ia, foc->ib);
		PARK_MACRO(foc->ia, foc->ib, ref_angle, foc->id_fb, foc->iq_fb);

		CplxPiRegSetLimit(&foc->i_reg, -vb * MAX_DUTY, vb * MAX_DUTY);
		CplxPiRegUpdate(&foc->i_reg,
						id, iq + i_comp,
						foc->id_fb, foc->iq_fb,
						0, 0,
						-velElectricalRad,
						dt,
						&foc->ud, &foc->uq);

		IPARK_MACRO(foc->ud, foc->uq, foc->ua, foc->ub);
		SVGENDQ_MACRO(foc->ua, foc->ub, foc->u_ph);

		foc->pwm[0] = foc->u_ph[0] / vb;
		foc->pwm[1] = foc->u_ph[1] / vb;
		foc->pwm[2] = foc->u_ph[2] / vb;
	}
	else
	{
		if(foc->was_foc_ctrl != is_foc) CplxPiRegReset(&foc->i_reg);
	}
	foc->was_foc_ctrl = is_foc;
}

#endif // FOC_H__