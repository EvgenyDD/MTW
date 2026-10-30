#ifndef DCC_HW_H__
#define DCC_HW_H__

#include <stdbool.h>
#include <stdint.h>

void dcc_manual(bool on0, bool on1, bool state0, bool state1) __attribute__((deprecated("moved to ASM control")));
void dcc_enable(bool en);
// void dcc_diag(print_func_t pf);

bool dcc_is_enabled(void);

#endif // DCC_HW_H__