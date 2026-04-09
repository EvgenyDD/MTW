#ifndef FONTS_H__
#define FONTS_H__

#include <stdint.h>

#define F(f) f, f##_sz

#define HDR
#include "fonts/font11x16_stm.c"
#include "fonts/font14x20_stm.c"
#include "fonts/font16x16_arial_bold.c"
#include "fonts/font16x16_arial_italic.c"
#include "fonts/font16x16_arial_normal.c"
#include "fonts/font16x16_big.c"
#include "fonts/font16x16_franklingothic.c"
#include "fonts/font16x16_hallfetica.c"
#include "fonts/font16x16_nadianne.c"
#include "fonts/font16x16_sinclair_m.c"
#include "fonts/font16x16_swiss721_outline.c"
#include "fonts/font16x22_dot_matrix_m.c"
#include "fonts/font16x24_16seg.c"
#include "fonts/font16x24_arial_round.c"
#include "fonts/font16x24_ocr_ext.c"
#include "fonts/font16x32_grotesk.c"
#include "fonts/font16x32_grotesk_bold.c"
#include "fonts/font16x32_retro.c"
#include "fonts/font17x24_stm.c"
#include "fonts/font24x32_inconsola.c"
#include "fonts/font24x32_ubuntu.c"
#include "fonts/font24x32_ubuntu_bold.c"
#include "fonts/font24x36_16seg.c"
#include "fonts/font24x48_grotesk.c"
#include "fonts/font24x48_grotesk_bold.c"
#include "fonts/font32x48_16seg.c"
#include "fonts/font32x50_16seg.c"
#include "fonts/font32x50_7seg.c"
#include "fonts/font32x50_sseg_rus.c"
#include "fonts/font32x52_segment18_xxl.c"
#include "fonts/font32x64_grotesk.c"
#include "fonts/font32x64_grotesk_bold.c"
#include "fonts/font3x5.c"
#include "fonts/font40x60_16seg.c"
#include "fonts/font5x8.c"
#include "fonts/font5x8_stm.c"
#include "fonts/font7x12_stm.c"
#include "fonts/font8x12_small.c"
#include "fonts/font8x12_tron.c"
#include "fonts/font8x16_retro.c"
#include "fonts/font8x8_sinclair_s.c"
#include "fonts/font8x8_tiny.c"

#include "fonts/num/font16x22_num_dot_matrix_m.c"
#include "fonts/num/font24x29_num_dot_matrix_l.c"
#include "fonts/num/font32x46_num_7seg.c"
#include "fonts/num/font32x48_num_calibri_bold.c"
#include "fonts/num/font32x50_num_7seg.c"
#include "fonts/num/font32x50_num_arial.c"
#include "fonts/num/font32x50_num_dot_matrix_xl.c"
#include "fonts/num/font48x72_num_16seg.c"
#include "fonts/num/font64x100_num_7seg_xxl.c"
#include "fonts/num/font64x96_num_16seg.c"
#include "fonts/num/font96x144_num_16seg.c"
#include "fonts/num/font96x144_num_7seg.c"

#include "fonts/symbols/font16x16_sym_var.c"
#include "fonts/symbols/font16x32_sym_var.c"
#include "fonts/symbols/font16x32_sym_var2.c"
#include "fonts/symbols/font32x32_sym_var.c"
#include "fonts/symbols/font48x24_sym_battery.c"
#undef HDR

extern const uint8_t font3x5[];
extern const uint8_t font5x8[];
extern const uint8_t font5x8_stm[];
extern const uint8_t font7x12_stm[];
extern const uint8_t font8x8_sinclair_s[];
extern const uint8_t font8x8_tiny[];
extern const uint8_t font8x12_small[];
extern const uint8_t font8x12_tron[];
extern const uint8_t font8x16_retro[];
extern const uint8_t font11x16_stm[];
extern const uint8_t font14x20_stm[];
extern const uint8_t font16x16_arial_bold[];
extern const uint8_t font16x16_arial_italic[];
extern const uint8_t font16x16_arial_normal[];
extern const uint8_t font16x16_big[];
extern const uint8_t font16x16_franklingothic[];
extern const uint8_t font16x16_hallfetica[];
extern const uint8_t font16x16_nadianne[];
extern const uint8_t font16x16_sinclair_m[];
extern const uint8_t font16x16_swiss721_outline[];
extern const uint8_t font16x22_dot_matrix_m[];
extern const uint8_t font16x24_16seg[];
extern const uint8_t font16x24_arial_round[];
extern const uint8_t font16x24_ocr_ext[];
extern const uint8_t font16x32_grotesk[];
extern const uint8_t font16x32_grotesk_bold[];
extern const uint8_t font16x32_retro[];
extern const uint8_t font17x24_stm[];
extern const uint8_t font24x32_inconsola[];
extern const uint8_t font24x32_ubuntu[];
extern const uint8_t font24x32_ubuntu_bold[];
extern const uint8_t font24x36_16seg[];
extern const uint8_t font24x48_grotesk[];
extern const uint8_t font24x48_grotesk_bold[];
extern const uint8_t font32x48_16seg[];
extern const uint8_t font32x50_16seg[];
extern const uint8_t font32x50_7seg[];
extern const uint8_t font32x50_sseg_rus[];
extern const uint8_t font32x52_segment18_xxl[];
extern const uint8_t font32x64_grotesk[];
extern const uint8_t font32x64_grotesk_bold[];
extern const uint8_t font40x60_16seg[];

// num
extern const uint8_t font16x22_num_dot_matrix_m[];
extern const uint8_t font24x29_num_dot_matrix_l[];
extern const uint8_t font32x46_num_7seg[];
extern const uint8_t font32x48_num_calibri_bold[];
extern const uint8_t font32x50_num_7seg[];
extern const uint8_t font32x50_num_arial[];
extern const uint8_t font32x50_num_dot_matrix_xl[];
extern const uint8_t font48x72_num_16seg[];
extern const uint8_t font64x96_num_16seg[];
extern const uint8_t font64x100_num_7seg_xxl[];
extern const uint8_t font96x144_num_16seg[];
extern const uint8_t font96x144_num_7seg[];

// sym
extern const uint8_t font16x16_sym_var[];
extern const uint8_t font16x32_sym_var[];
extern const uint8_t font16x32_sym_var2[];
extern const uint8_t font32x32_sym_var[];
extern const uint8_t font48x24_sym_battery[];

#endif // FONTS_H__