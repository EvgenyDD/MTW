#include "adc.h"
#include "graph.h"

#define V_S(x) &x, sizeof(x)

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
gr_var_t gr_vars[] = {
	{V_S(g_adc_val.ub), GR_TYPE_REAL32, GR_TYPE_UINT16, 100.0f, "Ub", NULL},
	{V_S(g_adc_val.i[0]), GR_TYPE_REAL32, GR_TYPE_UINT16, 1000.0f, "Iu", NULL},
	{V_S(g_adc_val.i[1]), GR_TYPE_REAL32, GR_TYPE_UINT16, 1000.0f, "Iv", NULL},
};
#pragma GCC diagnostic pop

const uint32_t gr_vars_size = sizeof(gr_vars) / sizeof(gr_vars[0]);