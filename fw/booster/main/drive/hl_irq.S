#include "sdkconfig.h"
#include "xtensa/config/specreg.h"
#include "soc/gpio_reg.h"
#include "soc/spi_reg.h"
#include "soc/timer_group_reg.h"

#define DELAY_CYCLES_SPI_TX_16B 63

#define LDAC_BITMASK  (1<<5)
#define COUNT_BITMASK (1<<13)

// don't use a0/a1

#define GPIO_W1TC_REG    a2   // GPIO_OUT_W1TC_REG
#define GPIO_W1TS_REG    a3   // GPIO_OUT_W1TS_REG
#define SPI_DATA         a4   // /*SPI_W0_REG(1)*/ (DR_REG_SPI2_BASE) + 0x80
#define SPI_CMD          a5   // /*SPI_CMD_REG(1)*/DR_REG_SPI2_BASE
#define SPI_CMD_USR      a6   // SPI_USR_M

#define RA               a15
#define RB               a14
#define RC               a13

#define R_NEXT_VAL       a12
#define R_ARRAY          a11
#define R_PTR            a10

.data
_l5_intr_stack:
    .space  44

.macro delay_cycles delay
    movi.n  RC, \delay
decr\@:
    addi.n  RC, RC, -1
    bnez    RC, decr\@
.endm

.macro spi_prepare idx hdr v ptr array
    movi    \v, \idx
    add.n   \v, \v, \ptr
    slli    \v, \v, 1 // align uint16_t (*2)
    add.n   \v, \array, \v
    memw

    l16ui   RA, \v, 0
    extui   RA, RA, 0, 16
    slli    \v, RA, 8
    srli    RA, RA, 8
    or      \v, \v, RA
    extui   \v, \v, 0, 16
    movi    RA, \hdr
    or      \v, \v, RA
    extui   \v, \v, 0, 16
.endm

.macro spi_load_and_start v
    s32i    \v, SPI_DATA, 0
    memw
    s32i    SPI_CMD_USR, SPI_CMD, 0
    memw
.endm

.macro cs_l pin_mask
    movi    RA, \pin_mask
    s32i    RA, GPIO_W1TC_REG, 0
    memw
.endm

.macro cs_h pin_mask
    movi    RA, \pin_mask
    s32i    RA, GPIO_W1TS_REG, 0
    memw
.endm

.macro bin_cnt_pulse
    cs_l    (COUNT_BITMASK)
    cs_h    (COUNT_BITMASK)
.endm

.macro ldac_inv_pulse
    movi    RA, LDAC_BITMASK
    s32i    RA, GPIO_W1TC_REG, 0
    memw
    delay_cycles 5 // 100ns+
    s32i    RA, GPIO_W1TS_REG, 0
    memw
.endm

    .section .iram1, "ax"
    .global xt_highint5
    .type   xt_highint5, @function
    .align  4

xt_highint5:
    // save register values
    movi    a0,  _l5_intr_stack
    s32i    a2,  a0, 0
    s32i    a3,  a0, 4
    s32i    a4,  a0, 8
    s32i    a5,  a0, 12
    s32i    a6,  a0, 16
    s32i    a10, a0, 20
    s32i    a11, a0, 24
    s32i    a12, a0, 28
    s32i    a13, a0, 32
    s32i    a14, a0, 36
    s32i    a15, a0, 40

    // enable timer alarm
    movi    RA, TIMG_T0CONFIG_REG(0)
    l32i.n  RB, RA, 0
    movi    RC, TIMG_T0_ALARM_EN_M
    or      RC, RB, RC
    memw
    s32i    RC, RA, 0
    memw

    // clear timer interrupt
    movi    RA, TIMG_INT_CLR_TIMERS_REG(0)
    movi    RB, TIMG_T0_INT_CLR_M
    s32i    RB, RA, 0
    memw

    // if(lpq_tail == lpq_head) { NO_NEW_FRAMES }
    movi    RA, [lpq_tail]
    memw
    l32i    R_PTR, RA, 0
    movi    RB, [lpq_head]
    memw
    l32i    RB, RB, 0
    memw
    movi.n  RC, 0
    beq     R_PTR, RB, NEW_FRAMES_UNAVAIL
    movi.n  RC, 1

    // lpq_tail++
    movi    RA, [lpq_tail]
    memw
    l32i    R_PTR, RA, 0
    addi.n  R_PTR, R_PTR, 1
    memw
    s32i.n  R_PTR, RA, 0
    memw

    // if(lpq_tail >= lpq_cnt) lpq_tail = 0;
    movi    RB, [lpq_cnt]
    memw
    l32i    RB, RB, 0
    memw
    bltu    R_PTR, RB, NOT_CLEAR_FRAME_CUR

    movi.n  R_PTR, 0
    memw
    s32i.n  R_PTR, RA, 0
    memw

NOT_CLEAR_FRAME_CUR:
NEW_FRAMES_UNAVAIL:

    // R_PTR - lpq_tail*6
    slli    RA, R_PTR, 2
    add.n   RA, RA, R_PTR
    add.n   R_PTR, RA, R_PTR
    memw

    // initialize registers
    movi    SPI_DATA, (DR_REG_SPI2_BASE) + 0x80
    movi    SPI_CMD, DR_REG_SPI2_BASE
    movi    SPI_CMD_USR, SPI_USR_M
    movi    GPIO_W1TC_REG, GPIO_OUT_W1TC_REG
    movi    GPIO_W1TS_REG, GPIO_OUT_W1TS_REG
    memw

    movi    R_ARRAY, lpq
    memw

    // set timer alarm value
    movi    RA, TIMG_T0ALARMLO_REG(0)
    slli    RB, R_PTR, 1 // align uint16_t (*2)
    add.n   RB, R_ARRAY, RB
    l16ui   RB, RB, 0
    memw
    s32i    RB, RA, 0
    memw

    beqz    RC, NO_NEW_FRAMES

    bin_cnt_pulse
    spi_prepare 1 176 R_NEXT_VAL R_PTR R_ARRAY    // ((1 << (15 - 8)) | (1 << (13 - 8)) | (1 << (12 - 8))) Galvo - X
    spi_load_and_start R_NEXT_VAL
        spi_prepare 5 48 R_NEXT_VAL R_PTR R_ARRAY // ((0 << (15 - 8)) | (1 << (13 - 8)) | (1 << (12 - 8))) B channel
    delay_cycles DELAY_CYCLES_SPI_TX_16B

    bin_cnt_pulse
    spi_load_and_start R_NEXT_VAL
        spi_prepare 2 48 R_NEXT_VAL R_PTR R_ARRAY // ((0 << (15 - 8)) | (1 << (13 - 8)) | (1 << (12 - 8))) Galvo - Y
    delay_cycles DELAY_CYCLES_SPI_TX_16B

    bin_cnt_pulse
    spi_load_and_start R_NEXT_VAL
        spi_prepare 3 176 R_NEXT_VAL R_PTR R_ARRAY // ((1 << (15 - 8)) | (1 << (13 - 8)) | (1 << (12 - 8))) R channel
    delay_cycles DELAY_CYCLES_SPI_TX_16B

    bin_cnt_pulse
    spi_load_and_start R_NEXT_VAL
        spi_prepare 4 176 R_NEXT_VAL R_PTR R_ARRAY // ((1 << (15 - 8)) | (1 << (13 - 8)) | (1 << (12 - 8))) G channel
    delay_cycles DELAY_CYCLES_SPI_TX_16B

    bin_cnt_pulse
    spi_load_and_start R_NEXT_VAL
    delay_cycles DELAY_CYCLES_SPI_TX_16B

RET_FROM_NO_NEW_FRAMES:
    // LDAC pulse (inv.)
    bin_cnt_pulse
    ldac_inv_pulse

    //restore register values
    movi    a0, _l5_intr_stack
    l32i    a2,  a0, 0
    l32i    a3,  a0, 4
    l32i    a4,  a0, 8
    l32i    a5,  a0, 12
    l32i    a6,  a0, 16
    l32i    a10, a0, 20
    l32i    a11, a0, 24
    l32i    a12, a0, 28
    l32i    a13, a0, 32
    l32i    a14, a0, 36
    l32i    a15, a0, 40
    rsync

    rsr     a0, EXCSAVE_5
    rfi     5

.macro spi_prepare_null hdr v
    movi    RA, 0
    extui   RA, RA, 0, 16
    slli    \v, RA, 8
    srli    RA, RA, 8
    or      \v, \v, RA
    extui   \v, \v, 0, 16
    movi    RA, \hdr
    or      \v, \v, RA
    extui   \v, \v, 0, 16
.endm

NO_NEW_FRAMES:
    bin_cnt_pulse
    spi_prepare 1 176 R_NEXT_VAL R_PTR R_ARRAY    // ((1 << (15 - 8)) | (1 << (13 - 8)) | (1 << (12 - 8))) Galvo - X
    spi_load_and_start R_NEXT_VAL
        spi_prepare_null 48 R_NEXT_VAL            // ((0 << (15 - 8)) | (1 << (13 - 8)) | (1 << (12 - 8)))
    delay_cycles DELAY_CYCLES_SPI_TX_16B

    bin_cnt_pulse
    spi_load_and_start R_NEXT_VAL
        spi_prepare 2 48 R_NEXT_VAL R_PTR R_ARRAY // ((0 << (15 - 8)) | (1 << (13 - 8)) | (1 << (12 - 8))) Galvo - Y
    delay_cycles DELAY_CYCLES_SPI_TX_16B

    bin_cnt_pulse
    spi_load_and_start R_NEXT_VAL
        spi_prepare_null 176 R_NEXT_VAL           // ((1 << (15 - 8)) | (1 << (13 - 8)) | (1 << (12 - 8)))
    delay_cycles DELAY_CYCLES_SPI_TX_16B

    bin_cnt_pulse
    spi_load_and_start R_NEXT_VAL
        spi_prepare_null 176 R_NEXT_VAL           // ((1 << (15 - 8)) | (1 << (13 - 8)) | (1 << (12 - 8)))
    delay_cycles DELAY_CYCLES_SPI_TX_16B

    bin_cnt_pulse
    spi_load_and_start R_NEXT_VAL
    delay_cycles DELAY_CYCLES_SPI_TX_16B
    J       RET_FROM_NO_NEW_FRAMES

 .global ld_include_xt_highint5
ld_include_xt_highint5:

//-------------------//
//-----/ OTHER /-----//
//-------------------//
    //while(DAC_SPI.cmd.usr) // Wait for SPI bus ready
//WAIT_SPI_RDY:
    //movi    RA, /*SPI_CMD_REG(1)*/DR_REG_SPI2_BASE400d04c4
    //memw
    //l32i.n  RA, RA, 0a8
    //bbsi    RA, SPI_USR_S, WAIT_SPI_RDY