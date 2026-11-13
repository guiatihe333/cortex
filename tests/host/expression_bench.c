/* Reproducible host microbenchmark: CPU time per source/route block. */
#include <stdio.h>
#include <time.h>
#include "../../app/dsp/amb.c"
void en_midi_out(uint32_t s, uint32_t a, uint32_t b) { (void)s; (void)a; (void)b; }
int main(void)
{
    const int iterations = 1000000;
    int mode, i, r, src[XS_N];
    volatile unsigned checksum = 0;
    for (mode = 0; mode < 5; mode++) {
        clock_t start;
        en_init();
        apply(P_EX(0, EX_AMOUNT), mode == 1 ? 0 : 100);
        for (r = 0; r < EN_XROUTES; r++) {
            apply(P_XR(0, r, XR_SOURCE), mode == 2 ? 0 : XS_DENSITY);
            apply(P_XR(0, r, XR_TARGET), XT_CUTOFF);
            apply(P_XR(0, r, XR_DEPTH), 40);
            apply(P_XR(0, r, XR_SMOOTH), 70);
        }
        start = clock();
        for (i = 0; i < iterations; i++) {
            if (mode == 0) { expr_sources(0, -1, -1, -1, src); checksum += (unsigned)src[XS_ENERGY]; }
            else if (mode == 4) {
                for (r = 0; r < NPOLY; r++) expr_observe(0, &vc[0].n[r].ex);
                expr_observation_block(); checksum += xdensity[0] + xduration[0];
            } else { expr_block(0); checksum += en_expression[0][0]; }
        }
        printf("%s: %.1f ns/call\n", mode == 0 ? "sources" : mode == 1 ? "amount-zero" : mode == 2 ? "routes-off" : mode == 3 ? "8-routes" : "observe-four-notes", (double)(clock()-start)*1e9/CLOCKS_PER_SEC/iterations);
    }
    printf("checksum: %u\n", checksum);
    return 0;
}
