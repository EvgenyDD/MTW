#ifndef REG_H__
#define REG_H__

#include <math.h>

typedef struct
{
	float kp, ki;
	float acc_d, acc_q;
	float out_d, out_q;
} cplx_pi_reg_t;

inline void CplxPiRegInit(volatile cplx_pi_reg_t *reg)
{
	reg->acc_d = reg->acc_q = 0;
}

inline void CplxPiRegReset(volatile cplx_pi_reg_t *reg)
{
	reg->acc_d = reg->acc_q = 0;
}

inline void CplxPiRegUpdate(volatile cplx_pi_reg_t *reg, float hi,
							float td, float tq,
							float fbd, float fbq,
							float ffd, float ffq,
							float w, float dt)
{
	float ed = td - fbd,
		  eq = tq - fbq;
	float dp = ed * reg->kp + ffd,
		  qp = eq * reg->kp + ffq;

	reg->acc_d += (ed * reg->ki - eq * reg->kp * w) * dt;
	reg->acc_q += (eq * reg->ki + ed * reg->kp * w) * dt;

	float AS = (reg->acc_d + dp) * (reg->acc_d + dp) +
			   (reg->acc_q + qp) * (reg->acc_q + qp);

	if(AS != 0)
	{
		float A = hi / sqrt(AS);
		if(A < 1.0)
		{
			reg->acc_d = (reg->acc_d + dp) * A - dp;
			reg->acc_q = (reg->acc_q + qp) * A - qp;
		}
	}

	reg->out_d = reg->acc_d + dp;
	reg->out_q = reg->acc_q + qp;
}

#endif // REG_H__
