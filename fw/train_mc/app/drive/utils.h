#ifndef UTILS_H__
#define UTILS_H__

#include "utils_math.h"

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#define ABS(a) fabs(a)

#define CLIP(a, l, h) (MAX((MIN((a), (h))), (l)))
#define CLIPS(a, v) CLIP((a), -(v), (v))
#define CLIPL(a, l) (((a) < (l)) ? (l) : (a))
#define CLIPH(a, h) (((a) > (h)) ? (h) : (a))

#endif // UTILS_H__