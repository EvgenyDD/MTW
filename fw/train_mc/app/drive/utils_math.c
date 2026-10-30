#include "utils_math.h"
#include <stdlib.h>
#include <string.h>

/*
 * Map angle from 0 to 1 in the range min to max. If angle is
 * outside of the range it will be less truncated to the closest
 * angle. Angle units: Degrees
 */
float utils_map_angle(float angle, float min, float max)
{
	if(max == min)
	{
		return -1;
	}

	float range_pos = max - min;
	utils_norm_angle(&range_pos);
	float range_neg = min - max;
	utils_norm_angle(&range_neg);
	float margin = range_neg / 2.0;

	angle -= min;
	utils_norm_angle(&angle);
	if(angle > (360 - margin))
	{
		angle -= 360.0;
	}

	float res = angle / range_pos;
	utils_truncate_number(&res, 0.0, 1.0);

	return res;
}

/**
 * Truncate absolute values less than tres to zero. The value
 * tres will be mapped to 0 and the value max to max.
 */
void utils_deadband(float *value, float tres, float max)
{
	if(fabsf(*value) < tres)
	{
		*value = 0.0;
	}
	else
	{
		float k = max / (max - tres);
		if(*value > 0.0)
		{
			*value = k * *value + max * (1.0 - k);
		}
		else
		{
			*value = -(k * -*value + max * (1.0 - k));
		}
	}
}

/**
 * Get the difference between two angles. Will always be between -180 and +180 degrees.
 * @param angle1
 * The first angle
 * @param angle2
 * The second angle
 * @return
 * The difference between the angles
 */
float utils_angle_difference(float angle1, float angle2)
{
	float difference = angle1 - angle2;
	while(difference < -180.0)
		difference += 2.0 * 180.0;
	while(difference > 180.0)
		difference -= 2.0 * 180.0;
	return difference;
}

/**
 * Get the difference between two angles. Will always be between -pi and +pi radians.
 * @param angle1
 * The first angle in radians
 * @param angle2
 * The second angle in radians
 * @return
 * The difference between the angles in radians
 */
float utils_angle_difference_rad(float angle1, float angle2)
{
	float difference = angle1 - angle2;
	while(difference < -M_PI)
		difference += 2.0 * M_PI;
	while(difference > M_PI)
		difference -= 2.0 * M_PI;
	return difference;
}

/**
 * Takes the average of a number of angles.
 *
 * @param angles
 * The angles in radians.
 *
 * @param angles_num
 * The number of angles.
 *
 * @param weights
 * The weight of the summarized angles
 *
 * @return
 * The average angle.
 */
float utils_avg_angles_rad_fast(float *angles, float *weights, int angles_num)
{
	float s_sum = 0.0;
	float c_sum = 0.0;

	for(int i = 0; i < angles_num; i++)
	{
		float s, c;
		utils_fast_sincos_better(angles[i], &s, &c);
		s_sum += s * weights[i];
		c_sum += c * weights[i];
	}

	return utils_fast_atan2(s_sum, c_sum);
}

/**
 * Interpolate two angles in radians and normalize the result to
 * -pi to pi.
 *
 * @param a1
 * The first angle
 *
 * @param a2
 * The second angle
 *
 * @param weight_a1
 * The weight of the first angle. If this is 1.0 the result will
 * be a1 and if it is 0.0 the result will be a2.
 *
 */
float utils_interpolate_angles_rad(float a1, float a2, float weight_a1)
{
	while((a1 - a2) > M_PI)
		a2 += 2.0 * M_PI;
	while((a2 - a1) > M_PI)
		a1 += 2.0 * M_PI;

	float res = a1 * weight_a1 + a2 * (1.0 - weight_a1);
	res = utils_norm_angle_rad(res);
	return res;
}

/**
 * Get the middle value of three values
 *
 * @param a
 * First value
 *
 * @param b
 * Second value
 *
 * @param c
 * Third value
 *
 * @return
 * The middle value
 */
float utils_middle_of_3(float a, float b, float c)
{
	float middle;

	if((a <= b) && (a <= c))
	{
		middle = (b <= c) ? b : c;
	}
	else if((b <= a) && (b <= c))
	{
		middle = (a <= c) ? a : c;
	}
	else
	{
		middle = (a <= b) ? a : b;
	}
	return middle;
}

/**
 * Get the middle value of three values
 *
 * @param a
 * First value
 *
 * @param b
 * Second value
 *
 * @param c
 * Third value
 *
 * @return
 * The middle value
 */
int utils_middle_of_3_int(int a, int b, int c)
{
	int middle;

	if((a <= b) && (a <= c))
	{
		middle = (b <= c) ? b : c;
	}
	else if((b <= a) && (b <= c))
	{
		middle = (a <= c) ? a : c;
	}
	else
	{
		middle = (a <= b) ? a : b;
	}
	return middle;
}

/**
 * Fast atan2
 *
 * See http://www.dspguru.com/dsp/tricks/fixed-point-atan2-with-self-normalization
 *
 * @param y
 * y
 *
 * @param x
 * x
 *
 * @return
 * The angle in radians
 */
float utils_fast_atan2(float y, float x)
{
	float abs_y = fabsf(y) + 1e-20; // kludge to prevent 0/0 condition
	float angle;

	if(x >= 0)
	{
		float r = (x - abs_y) / (x + abs_y);
		float rsq = r * r;
		angle = ((0.1963 * rsq) - 0.9817) * r + (M_PI / 4.0);
	}
	else
	{
		float r = (x + abs_y) / (abs_y - x);
		float rsq = r * r;
		angle = ((0.1963 * rsq) - 0.9817) * r + (3.0 * M_PI / 4.0);
	}

	UTILS_NAN_ZERO(angle);

	if(y < 0)
	{
		return (-angle);
	}
	else
	{
		return (angle);
	}
}

float utils_fast_sin(float angle)
{
	while(angle < -M_PI)
	{
		angle += 2.0 * M_PI;
	}

	while(angle > M_PI)
	{
		angle -= 2.0 * M_PI;
	}

	float res = 0.0;

	if(angle < 0.0)
	{
		res = 1.27323954 * angle + 0.405284735 * angle * angle;
	}
	else
	{
		res = 1.27323954 * angle - 0.405284735 * angle * angle;
	}

	return res;
}

float utils_fast_cos(float angle)
{
	angle += 0.5 * M_PI;

	while(angle < -M_PI)
	{
		angle += 2.0 * M_PI;
	}

	while(angle > M_PI)
	{
		angle -= 2.0 * M_PI;
	}

	float res = 0.0;

	if(angle < 0.0)
	{
		res = 1.27323954 * angle + 0.405284735 * angle * angle;
	}
	else
	{
		res = 1.27323954 * angle - 0.405284735 * angle * angle;
	}

	return res;
}

/**
 * Fast sine and cosine implementation.
 *
 * See http://lab.polygonal.de/?p=205
 *
 * @param angle
 * The angle in radians
 * WARNING: Don't use too large angles.
 *
 * @param sin
 * A pointer to store the sine value.
 *
 * @param cos
 * A pointer to store the cosine value.
 */
void utils_fast_sincos(float angle, float *sin, float *cos)
{
	// always wrap input angle to -PI..PI
	while(angle < -M_PI)
	{
		angle += 2.0 * M_PI;
	}

	while(angle > M_PI)
	{
		angle -= 2.0 * M_PI;
	}

	// compute sine
	if(angle < 0.0)
	{
		*sin = 1.27323954 * angle + 0.405284735 * angle * angle;
	}
	else
	{
		*sin = 1.27323954 * angle - 0.405284735 * angle * angle;
	}

	// compute cosine: sin(x + PI/2) = cos(x)
	angle += 0.5 * M_PI;

	if(angle > M_PI)
	{
		angle -= 2.0 * M_PI;
	}

	if(angle < 0.0)
	{
		*cos = 1.27323954 * angle + 0.405284735 * angle * angle;
	}
	else
	{
		*cos = 1.27323954 * angle - 0.405284735 * angle * angle;
	}
}

/**
 * Fast sine and cosine implementation.
 *
 * See http://lab.polygonal.de/?p=205
 *
 * @param angle
 * The angle in radians
 * WARNING: Don't use too large angles.
 *
 * @param sin
 * A pointer to store the sine value.
 *
 * @param cos
 * A pointer to store the cosine value.
 */
void utils_fast_sincos_better(float angle, volatile float *sin, volatile float *cos)
{
	// always wrap input angle to -PI..PI
	while(angle < -M_PI)
		angle += 2.0 * M_PI;

	while(angle > M_PI)
		angle -= 2.0 * M_PI;

	// compute sine
	if(angle < 0.0)
	{
		*sin = 1.27323954 * angle + 0.405284735 * angle * angle;

		if(*sin < 0.0)
		{
			*sin = 0.225 * (*sin * -*sin - *sin) + *sin;
		}
		else
		{
			*sin = 0.225 * (*sin * *sin - *sin) + *sin;
		}
	}
	else
	{
		*sin = 1.27323954 * angle - 0.405284735 * angle * angle;

		if(*sin < 0.0)
		{
			*sin = 0.225 * (*sin * -*sin - *sin) + *sin;
		}
		else
		{
			*sin = 0.225 * (*sin * *sin - *sin) + *sin;
		}
	}

	// compute cosine: sin(x + PI/2) = cos(x)
	angle += 0.5 * M_PI;
	if(angle > M_PI) angle -= 2.0 * M_PI;

	if(angle < 0.0)
	{
		*cos = 1.27323954 * angle + 0.405284735 * angle * angle;

		if(*cos < 0.0)
			*cos = 0.225 * (*cos * -*cos - *cos) + *cos;
		else
			*cos = 0.225 * (*cos * *cos - *cos) + *cos;
	}
	else
	{
		*cos = 1.27323954 * angle - 0.405284735 * angle * angle;

		if(*cos < 0.0)
			*cos = 0.225 * (*cos * -*cos - *cos) + *cos;
		else
			*cos = 0.225 * (*cos * *cos - *cos) + *cos;
	}
}

float _isqrt_q3a_(float number)
{
	const float x2 = number * 0.5F;
	const float threehalfs = 1.5F;

	union
	{
		float f;
		uint32_t i;
	} conv = {number};
	conv.i = 0x5f3759df - (conv.i >> 1);
	conv.f *= (threehalfs - (x2 * conv.f * conv.f));
	return conv.f;
}

/**
 * Calculate the values with the lowest magnitude.
 *
 * @param va
 * The first value.
 *
 * @param vb
 * The second value.
 *
 * @return
 * The value with the lowest magnitude.
 */
float utils_min_abs(float va, float vb)
{
	float res;
	if(fabsf(va) < fabsf(vb))
	{
		res = va;
	}
	else
	{
		res = vb;
	}

	return res;
}

/**
 * Calculate the values with the highest magnitude.
 *
 * @param va
 * The first value.
 *
 * @param vb
 * The second value.
 *
 * @return
 * The value with the highest magnitude.
 */
float utils_max_abs(float va, float vb)
{
	float res;
	if(fabsf(va) > fabsf(vb))
	{
		res = va;
	}
	else
	{
		res = vb;
	}

	return res;
}

/**
 * Create string representation of the binary content of a byte
 *
 * @param x
 * The byte.
 *
 * @param b
 * Array to store the string representation in.
 */
void utils_byte_to_binary(int x, char *b)
{
	b[0] = '\0';

	int z;
	for(z = 128; z > 0; z >>= 1)
	{
		strcat(b, ((x & z) == z) ? "1" : "0");
	}
}

float utils_throttle_curve(float val, float curve_acc, float curve_brake, int mode)
{
	float ret = 0.0;

	if(val < -1.0)
	{
		val = -1.0;
	}

	if(val > 1.0)
	{
		val = 1.0;
	}

	float val_a = fabsf(val);

	float curve;
	if(val >= 0.0)
	{
		curve = curve_acc;
	}
	else
	{
		curve = curve_brake;
	}

	// See
	// http://math.stackexchange.com/questions/297768/how-would-i-create-a-exponential-ramp-function-from-0-0-to-1-1-with-a-single-val
	if(mode == 0)
	{ // Exponential
		if(curve >= 0.0)
		{
			ret = 1.0 - powf(1.0 - val_a, 1.0 + curve);
		}
		else
		{
			ret = powf(val_a, 1.0 - curve);
		}
	}
	else if(mode == 1)
	{ // Natural
		if(fabsf(curve) < 1e-10)
		{
			ret = val_a;
		}
		else
		{
			if(curve >= 0.0)
			{
				ret = 1.0 - ((expf(curve * (1.0 - val_a)) - 1.0) / (expf(curve) - 1.0));
			}
			else
			{
				ret = (expf(-curve * val_a) - 1.0) / (expf(-curve) - 1.0);
			}
		}
	}
	else if(mode == 2)
	{ // Polynomial
		if(curve >= 0.0)
		{
			ret = 1.0 - ((1.0 - val_a) / (1.0 + curve * val_a));
		}
		else
		{
			ret = val_a / (1.0 - curve * (1.0 - val_a));
		}
	}
	else
	{ // Linear
		ret = val_a;
	}

	if(val < 0.0)
	{
		ret = -ret;
	}

	return ret;
}

uint32_t utils_crc32c(uint8_t *data, uint32_t len)
{
	uint32_t crc = 0xFFFFFFFF;

	for(uint32_t i = 0; i < len; i++)
	{
		uint32_t byte = data[i];
		crc = crc ^ byte;

		for(int j = 7; j >= 0; j--)
		{
			uint32_t mask = -(crc & 1);
			crc = (crc >> 1) ^ (0x82F63B78 & mask);
		}
	}

	return ~crc;
}

// Yes, this is only the average...
void utils_fft32_bin0(float *real_in, float *real, float *imag)
{
	*real = 0.0;
	*imag = 0.0;

	for(int i = 0; i < 32; i++)
	{
		*real += real_in[i];
	}

	*real /= 32.0;
}

void utils_fft32_bin1(float *real_in, float *real, float *imag)
{
	*real = 0.0;
	*imag = 0.0;
	for(int i = 0; i < 32; i++)
	{
		*real += real_in[i] * utils_tab_cos_32_1[i];
		*imag -= real_in[i] * utils_tab_sin_32_1[i];
	}
	*real /= 32.0;
	*imag /= 32.0;
}

void utils_fft32_bin2(float *real_in, float *real, float *imag)
{
	*real = 0.0;
	*imag = 0.0;
	for(int i = 0; i < 32; i++)
	{
		*real += real_in[i] * utils_tab_cos_32_2[i];
		*imag -= real_in[i] * utils_tab_sin_32_2[i];
	}
	*real /= 32.0;
	*imag /= 32.0;
}

void utils_fft16_bin0(float *real_in, float *real, float *imag)
{
	*real = 0.0;
	*imag = 0.0;

	for(int i = 0; i < 16; i++)
	{
		*real += real_in[i];
	}

	*real /= 16.0;
}

void utils_fft16_bin1(float *real_in, float *real, float *imag)
{
	*real = 0.0;
	*imag = 0.0;
	for(int i = 0; i < 16; i++)
	{
		*real += real_in[i] * utils_tab_cos_32_1[2 * i];
		*imag -= real_in[i] * utils_tab_sin_32_1[2 * i];
	}
	*real /= 16.0;
	*imag /= 16.0;
}

void utils_fft16_bin2(float *real_in, float *real, float *imag)
{
	*real = 0.0;
	*imag = 0.0;
	for(int i = 0; i < 16; i++)
	{
		*real += real_in[i] * utils_tab_cos_32_2[2 * i];
		*imag -= real_in[i] * utils_tab_sin_32_2[2 * i];
	}
	*real /= 16.0;
	*imag /= 16.0;
}

void utils_fft8_bin0(float *real_in, float *real, float *imag)
{
	*real = 0.0;
	*imag = 0.0;

	for(int i = 0; i < 8; i++)
	{
		*real += real_in[i];
	}

	*real /= 8.0;
}

void utils_fft8_bin1(float *real_in, float *real, float *imag)
{
	*real = 0.0;
	*imag = 0.0;
	for(int i = 0; i < 8; i++)
	{
		*real += real_in[i] * utils_tab_cos_32_1[4 * i];
		*imag -= real_in[i] * utils_tab_sin_32_1[4 * i];
	}
	*real /= 8.0;
	*imag /= 8.0;
}

void utils_fft8_bin2(float *real_in, float *real, float *imag)
{
	*real = 0.0;
	*imag = 0.0;
	for(int i = 0; i < 8; i++)
	{
		*real += real_in[i] * utils_tab_cos_32_2[4 * i];
		*imag -= real_in[i] * utils_tab_sin_32_2[4 * i];
	}
	*real /= 8.0;
	*imag /= 8.0;
}

// A mapping of a samsung 30q cell for % remaining capacity vs. voltage from
// 4.2 to 3.2, note that the you lose 15% of the 3Ah rated capacity in this range
float utils_batt_liion_norm_v_to_capacity(float norm_v)
{
	// constants for polynomial fit of lithium ion battery
	const float li_p[] = {
		-2.979767, 5.487810, -3.501286, 1.675683, 0.317147};
	utils_truncate_number(&norm_v, 0.0, 1.0);
	float v2 = norm_v * norm_v;
	float v3 = v2 * norm_v;
	float v4 = v3 * norm_v;
	float v5 = v4 * norm_v;
	float capacity = li_p[0] * v5 + li_p[1] * v4 + li_p[2] * v3 +
					 li_p[3] * v2 + li_p[4] * norm_v;
	return capacity;
}
