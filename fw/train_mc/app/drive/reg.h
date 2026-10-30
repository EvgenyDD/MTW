#ifndef REG_H__
#define REG_H__

#include "utils_math.h"
#include <math.h>

typedef struct
{
	float kp, ki;
	float acc_d, acc_q;
	float out_d, out_q;
} cplx_pi_reg_t;

inline void cplx_pi_reg_reset(volatile cplx_pi_reg_t *reg)
{
	reg->acc_d = reg->acc_q = 0;
}

inline void cplx_pi_reg_update(volatile cplx_pi_reg_t *reg, float hi,
							   float td, float tq,
							   float fbd, float fbq,
							   float ffd, float ffq,
							   float w, float dt)
{
	float ed = td - fbd,
		  eq = tq - fbq;
	float dp = ed * reg->kp + ffd,
		  qp = eq * reg->kp + ffq;

	float acc_d = reg->acc_d + (ed * reg->ki - eq * reg->kp * w) * dt;
	float acc_q = reg->acc_q + (eq * reg->ki + ed * reg->kp * w) * dt;

	float AS = (acc_d + dp) * (acc_d + dp) +
			   (acc_q + qp) * (acc_q + qp);

	if(AS != 0)
	{
		float A = hi * _isqrt_q3a_(AS);
		if(A < 1.0)
		{
			acc_d = (acc_d + dp) * A - dp;
			acc_q = (acc_q + qp) * A - qp;
		}
	}

	reg->acc_d = acc_d;
	reg->acc_q = acc_q;

	reg->out_d = reg->acc_d + dp;
	reg->out_q = reg->acc_q + qp;
}

typedef struct
{
	float kp, ki;
	float acc;
	float out;
} pi_reg_t;

inline void pi_reg_reset(volatile pi_reg_t *reg)
{
	reg->acc = reg->out = 0;
}

inline void pi_reg_upd(volatile pi_reg_t *reg,
					   float v_err,
					   float lim,
					   float dt)
{
	reg->acc = CLIP(reg->acc + v_err * reg->ki * dt, -lim, lim);
	reg->out = CLIP(reg->acc + v_err * reg->kp, -lim, lim);
}

#endif // REG_H__
