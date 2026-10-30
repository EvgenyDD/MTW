#ifndef SAFETY_H__
#define SAFETY_H__

#include <stdbool.h>
#include <stdint.h>

enum
{
	ERR_OC = 0,
	ERR_UV,
	ERR_OV,
	ERR_HW,
};

void safety_set_error(uint32_t bit, bool value, float reg);
uint32_t safety_get_error_latched(void);
void safety_reset_error(void);
void safety_drive_enable(bool en);
void safety_poll(uint32_t diff_ms);

#endif // SAFETY_H__