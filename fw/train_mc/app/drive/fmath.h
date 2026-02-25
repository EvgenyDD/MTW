#ifndef FMATH_H__
#define FMATH_H__

#include "C28x_FPU_FastRTS.h"

float _atan2_(float y, float x);
float _atan2_a(float y, float x);
float _sqrt_(float a);
float _sin_(float x);
float _cos_(float x);
void _sin_cos_(float x, float *s, float *c);
float _atan_(float y, float x, float offset);

#define _ATAN2_ atan2
#define _SQRT_ sqrt
#define _SIN_ sin
#define _COS_ cos
#define _SINCOS_ sincos
#define _ISQRT_(x) (1.0 / _SQRT_(x))

#endif // FMATH_H__
