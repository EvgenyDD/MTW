#ifndef UI_H__
#define UI_H__

#include <stdint.h>

void ui_init(void);
void ui_poll(uint32_t diff_ms);

#endif // UI_H__