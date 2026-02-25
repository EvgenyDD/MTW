#ifndef REG_H__
#define REG_H__

#include <math.h>

typedef struct
{
	float lo, hi;
	float kp, ki;
	float acc_d, acc_q;
} cplx_pi_reg_t;

inline void CplxPiRegInit(cplx_pi_reg_t *reg)
{
	reg->acc_d = reg->acc_q = 0;
}

inline void CplxPiRegSetLimit(cplx_pi_reg_t *reg, float lo, float hi)
{
	reg->lo = lo;
	reg->hi = hi;
}

inline void CplxPiRegReset(cplx_pi_reg_t *reg)
{
	reg->acc_d = reg->acc_q = 0;
}

inline void CplxPiRegUpdate(cplx_pi_reg_t *reg,
							float td, float tq,
							float fbd, float fbq,
							float ffd, float ffq,
							float w, float dt,
							float *out_d, float *out_q)
{
	float ed = td - fbd, eq = tq - fbq;
	float dp = ed * reg->kp + ffd, qp = eq * reg->kp + ffq;

	reg->acc_d += (ed * reg->ki - eq * reg->kp * w) * dt;
	reg->acc_q += (eq * reg->ki + ed * reg->kp * w) * dt;

	float AS = (reg->acc_d + dp) * (reg->acc_d + dp) +
			   (reg->acc_q + qp) * (reg->acc_q + qp);

	if(AS != 0)
	{
		float A = reg->hi / sqrt(AS);
		if(A < 1.0)
		{
			reg->acc_d = (reg->acc_d + dp) * A - dp;
			reg->acc_q = (reg->acc_q + qp) * A - qp;
		}
	}

	*out_d = reg->acc_d + dp;
	*out_q = reg->acc_q + qp;
}

#endif // REG_H__
