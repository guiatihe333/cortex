/* render one FM6 note, algorithm 5, the sixth operator fed back at fb, through CORE (the original or AMB-1's) */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "fm6_tables.h"
#include CORE
int main(int argc, char **argv)
{
    static uint8_t p[FP_SIZE + 1];
    fm6_note_t n;
    int32_t out[FM6_N], dt[6] = {0};
    int fb = atoi(argv[1]), b, k;
    for (k = 0; k < 6; k++) {                    /* every operator: ratio 1, full, held */
        uint8_t *o = p + k * FP_OP;
        o[FP_R1] = o[FP_R1 + 1] = o[FP_R1 + 2] = o[FP_R1 + 3] = 99;
        o[FP_L1] = o[FP_L1 + 1] = o[FP_L1 + 2] = 99;
        o[FP_OL] = k < 2 ? 85 : 0;               /* the sixth into the fifth */
        o[FP_FC] = 1; o[FP_DET] = 7;
    }
    p[FP_ALG] = 4; p[FP_FB] = (uint8_t)fb; p[FP_TRNSP] = 24;
    fm6_note_init(&n, p, 57, 100, 1);
    for (b = 0; b < 1000; b++) {
        int r = fm6_note_compute(&n, p, out, 1 << 23, 0, 50857777 + 57 * 1398101, 4, FB_ARG, dt, 0);
        if (!r) for (k = 0; k < FM6_N; k++) out[k] = 0;
        fwrite(out, 4, FM6_N, stdout);
    }
    return 0;
}
