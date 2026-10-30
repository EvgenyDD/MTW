#include "sdkconfig.h"
#include "xtensa/config/specreg.h"
#include "soc/gpio_reg.h"
#include "soc/spi_reg.h"
#include "soc/timer_group_reg.h"
#include "dcc_def.h"

#define EN0_BITMASK (1<<22)
#define EN1_BITMASK (1<<19)
#define LVL0_BITMASK (1<<23)
#define LVL1_BITMASK (1<<21)

// don't use a0/a1

#define GPIO_W1TC_REG   a2   // GPIO_OUT_W1TC_REG
#define GPIO_W1TS_REG   a3   // GPIO_OUT_W1TS_REG

#define R_PIN_OP        a4
#define R_P0            a5
#define R_P1            a6

#define R_ARRAY         a10

#define RA              a12
#define RB              a13
#define RC              a14
#define RD              a15


.data
_l5_intr_stack:
    .space  44

.macro pin_l pin_mask
    movi    R_PIN_OP, \pin_mask
    memw
    s32i    R_PIN_OP, GPIO_W1TC_REG, 0
    memw
.endm

.macro pin_h pin_mask
    movi    R_PIN_OP, \pin_mask
    memw
    s32i    R_PIN_OP, GPIO_W1TS_REG, 0
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

    // initialize registers
    movi    GPIO_W1TC_REG, GPIO_OUT_W1TC_REG
    movi    GPIO_W1TS_REG, GPIO_OUT_W1TS_REG

    // enable timer alarm REG_SET_BIT(TIMG_T0CONFIG_REG(0), TIMG_T0_ALARM_EN_M);
    movi    RA, TIMG_T0CONFIG_REG(0)
    movi    RC, TIMG_T0_ALARM_EN_M
    memw
    l32i.n  RB, RA, 0
    or      RC, RB, RC
    s32i    RC, RA, 0

    // clear timer interrupt REG_WRITE(TIMG_INT_CLR_TIMERS_REG(0), TIMG_T0_INT_CLR_M);
    movi    RA, TIMG_INT_CLR_TIMERS_REG(0)
    movi    RB, TIMG_T0_INT_CLR_M
    memw
    s32i    RB, RA, 0

    // if(dcc_on == false)
    movi    RA, [dcc_on]
    memw
    l32i    RB, RA, 0
    beqz    RB, DCC_OFF

    // if(dcc_tail == dcc_head || dcc_1_continue) { PLAIN_DCC_1 }
    movi    RA, [dcc_tail]
    movi    RB, [dcc_head]
    movi    RC, [dcc_1_continue]
    memw
    l32i    RA, RA, 0
    l32i    RB, RB, 0
    l32i    RC, RC, 0
    beq     RA, RB, PLAIN_DCC_1
    bnez    RC, PLAIN_DCC_1

    // dcc_tail++
    movi    RB, [dcc_cnt]
    movi    RA, [dcc_tail]
    movi    R_ARRAY, dccq
    memw
    l32i    RB, RB, 0
    l32i    RD, RA, 0
    addi.n  RD, RD, 1 # +1
    memw
    s32i.n  RD, RA, 0 # store dcc_tail

    // if(dcc_tail >= dcc_cnt) dcc_tail = 0;
    bltu    RD, RB, IGNORE_CLEAR_TAIL

    // dcc_tail = 0;
    movi    RD, 0
    s32i.n  RD, RA, 0

IGNORE_CLEAR_TAIL:

    pin_h    (EN0_BITMASK)
    pin_h    (EN1_BITMASK)

    slli    RA, RD, 1
    addi    RA, RA, 1
    slli    RA, RA, 1
    add     RA, R_ARRAY, RA
    l16ui   RB, RA, 0

    movi    RA, 1
    bne     RB, RA, .CLR_P0
    pin_h    (LVL0_BITMASK)
    J .PIN1_OP
.CLR_P0:
    pin_l    (LVL0_BITMASK)
.PIN1_OP:

    movi    RA, 0
    bne     RB, RA, .CLR_P1
    pin_h    (LVL1_BITMASK)
    J .PIN_OP_END
.CLR_P1:
    pin_l    (LVL1_BITMASK)
.PIN_OP_END:

    // set timer alarm value
    slli    RA, RD, 1
    slli    RA, RA, 1
    add     RA, R_ARRAY, RA
    l16ui   RB, RA, 0
    movi    RC, TIMG_T0ALARMLO_REG(0)
    memw
    s32i    RB, RC, 0

FIN:
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

PLAIN_DCC_1:
    // dcc_1_continue = dcc_1_continue ? 0 : 1;
    movi    RA, [dcc_1_continue]
    movi    RD, 1
    memw
    l8ui    RB, RA, 0
    xor     RB, RB, RD
    s8i     RB, RA, 0

    pin_h    (EN0_BITMASK)
    pin_h    (EN1_BITMASK)

    pin_l    (LVL0_BITMASK)
    pin_l    (LVL1_BITMASK)

    movi    R_P0, LVL1_BITMASK
    movi    R_P1, LVL0_BITMASK
    l8ui    RA, RA, 0 # move RA = [dcc_1_continue]

    movi    R_PIN_OP, LVL0_BITMASK
    movnez  R_P0, R_PIN_OP, RA # RA = [dcc_1_continue]

    movi    R_PIN_OP, LVL1_BITMASK
    movnez  R_P1, R_PIN_OP, RA # RA = [dcc_1_continue]

    // pin L
    mov     R_PIN_OP, R_P0
    memw
    s32i    R_PIN_OP, GPIO_W1TC_REG, 0

    // pin H
    mov     R_PIN_OP, R_P1
    memw
    s32i    R_PIN_OP, GPIO_W1TS_REG, 0

    // set timer reload time
    movi    RA, TIMG_T0ALARMLO_REG(0) # REG_WRITE(TIMG_T0ALARMLO_REG(0), DCC_LEN_1);
    movi    RB, DCC_LEN_1
    memw
    s32i    RB, RA, 0

    J       FIN

DCC_OFF:
    pin_l    (EN0_BITMASK)
    pin_l    (EN1_BITMASK)
    pin_l    (LVL0_BITMASK)
    pin_l    (LVL1_BITMASK)

    movi    RA, TIMG_T0ALARMLO_REG(0) # REG_WRITE(TIMG_T0ALARMLO_REG(0), DCC_LEN_1);
    movi    RB, DCC_LEN_1
    memw
    s32i    RB, RA, 0
    memw

    J       FIN

 .global ld_include_xt_highint5
ld_include_xt_highint5: