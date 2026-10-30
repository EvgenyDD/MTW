#define DCC_POLAR_P 0
#define DCC_POLAR_N 1
#define DCC_POLAR_CUTOUT 2

#define DCC_BIT_UNDEF 0
#define DCC_BIT_0 1
#define DCC_BIT_1 2
#define DCC_BIT_00 3
#define DCC_BIT_01 4
#define DCC_BIT_10 5
#define DCC_BIT_11 6

// NMRA-DCC
#define DCC_LEN_0 100 // 95..9900
#define DCC_LEN_1 58  // 52..64

// non-NMRA-DCC
#define DCC_LEN_00 20 // 16..24
#define DCC_LEN_01 40 // 28..44
#define DCC_LEN_10 73 // 70..76
#define DCC_LEN_11 86 // 83..89

#define DCC_LEN_1_MIN 52
#define DCC_LEN_1_MAX 64
#define DCC_LEN_0_MIN 90
#define DCC_LEN_0_MAX 10000

#define DCC_LEN_00_MIN 16
#define DCC_LEN_00_MAX 24
#define DCC_LEN_01_MIN 28
#define DCC_LEN_01_MAX 44
#define DCC_LEN_10_MIN 70
#define DCC_LEN_10_MAX 76
#define DCC_LEN_11_MIN 83
#define DCC_LEN_11_MAX 89

#define DCC_ID_BROADCAST 0

#define DCC_MAX_BYTES 16
#define DCC_DECODE_MAX (12 + 9 * DCC_MAX_BYTES + 1 + 1)

#define PREAMBLE_LEN_MIN 12