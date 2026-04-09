#ifndef LP_H__
#define LP_H__

#include <stdbool.h>
#include <stdint.h>

#define POINT_BUF_CNT 6000

#define CLR_NULL 2
#define COORD_X 1
#define COORD_Y 2
#define CLR_R 3
#define CLR_G 4
#define CLR_B 5
#define CLR_WHT 6

void lp_init(void);

void lp_off(void);
void lp_on(void);

uint32_t lp_get_free_buf(void);
void lp_reset_q(void);

void lp_append_point(uint16_t time_us, uint16_t x, uint16_t y, uint16_t r, uint16_t g, uint16_t b);
void lp_append_pointa(uint16_t *a);

void lp_trace(void);

void lp_set_params(uint32_t value);

#endif // LP_H__