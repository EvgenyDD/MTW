#include "adc.h"
#include "drive.h"
#include "graph.h"

#define V_S(x) &x, sizeof(x)

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
gr_var_t gr_vars[] = {
	{V_S(g_adc_val.ub), GR_TYPE_REAL32, GR_TYPE_REAL32, 1.0f, "Ub", NULL},
	{V_S(g_adc_val.i[0]), GR_TYPE_REAL32, GR_TYPE_INT16, 1000.0f, "Iu", NULL},
	{V_S(g_adc_val.i[1]), GR_TYPE_REAL32, GR_TYPE_INT16, 1000.0f, "Iv", NULL},

	{V_S(ctrl.in_iq), GR_TYPE_REAL32, GR_TYPE_REAL32, 1000.0f, "iqi", NULL},
	{V_S(ctrl.in_id), GR_TYPE_REAL32, GR_TYPE_REAL32, 1000.0f, "idi", NULL},

	{V_S(ctrl.foc.id_fb), GR_TYPE_REAL32, GR_TYPE_REAL32, 1000.0f, "idf", NULL},
	{V_S(ctrl.foc.iq_fb), GR_TYPE_REAL32, GR_TYPE_REAL32, 1000.0f, "iqf", NULL},

	{V_S(ctrl.foc.ud), GR_TYPE_REAL32, GR_TYPE_REAL32, 1000.0f, "ud", NULL},
	{V_S(ctrl.foc.uq), GR_TYPE_REAL32, GR_TYPE_REAL32, 1000.0f, "uq", NULL},

	{V_S(ctrl.angle_enc), GR_TYPE_REAL32, GR_TYPE_REAL32, 1.0f, "Ae", NULL},
	{V_S(ctrl.angle_sim), GR_TYPE_REAL32, GR_TYPE_REAL32, 1.0f, "As", NULL},
	{V_S(ctrl.foc.pwm[0]), GR_TYPE_REAL32, GR_TYPE_REAL32, 1.0f, "Uo0", NULL},
	{V_S(ctrl.foc.pwm[1]), GR_TYPE_REAL32, GR_TYPE_REAL32, 1.0f, "Uo1", NULL},
	{V_S(ctrl.foc.pwm[2]), GR_TYPE_REAL32, GR_TYPE_REAL32, 1.0f, "Uo2", NULL},
	{V_S(TIM1->CCR1), GR_TYPE_UINT16, GR_TYPE_UINT16, 1.0f, "PWM0", NULL},
	{V_S(TIM1->CCR2), GR_TYPE_UINT16, GR_TYPE_UINT16, 1.0f, "PWM1", NULL},
	{V_S(TIM1->CCR3), GR_TYPE_UINT16, GR_TYPE_UINT16, 1.0f, "PWM2", NULL},
};
#pragma GCC diagnostic pop

const uint32_t gr_vars_size = sizeof(gr_vars) / sizeof(gr_vars[0]);