/* SPDX-License-Identifier: MPL-2.0 */
/* CORTEX VIDEO: the mirror's side, checked against what the simulated FM-1 sent (tests/scenarios/link.cvx logs its
 * SysEx raw). Every frame is replayed through this program's own engine (vfollow.h), and its sum must be the FM-1's:
 * no frame missing, none disagreeing, REC turned RECS times (default 2), at most SYNCS SYNCs (default 1).
 *   follow_test LOG [RECS [SYNCS [PPM]]]: PPM, the last frame there */
#include <stdio.h>
#include <stdlib.h>
#include "vfollow.h"

static uint16_t screen[240 * 240];
int main(int argc, char **argv)
{
    static uint8_t buf[1 << 20];
    FILE *f = fopen(argc > 1 ? argv[1] : "link.syx", "rb");
    vfollow_t s = {0};
    size_t n, i, start = 0;
    int recs = 0, was_rec = 0, fails = 0;
    if (!f) {
        printf("follow_test: no log\n");
        return 1;
    }
    n = fread(buf, 1, sizeof buf, f);
    fclose(f);
    for (i = 0; i < n; i++) {
        if (buf[i] == 0xF0)
            start = i;
        else if (buf[i] == 0xF7) {
            const int r = vfollow_msg(&s, buf + start, (uint32_t)(i - start + 1), screen);
            if (r == VF_FRAME && s.rec != was_rec) {
                recs++;
                was_rec = s.rec;
            }
        }
    }
    printf("follow_test: %u syncs, %u frames, %u gaps, %u disagreeing, %u bad, REC turned %d times\n", s.syncs,
           s.frames, s.gaps, s.mismatches, s.bad, recs);
    if (s.syncs < 1 || s.syncs > (unsigned)(argc > 3 ? atoi(argv[3]) : 1) || s.frames < 40 || s.gaps || s.mismatches ||
        s.bad || recs != (argc > 2 ? atoi(argv[2]) : 2))
        fails = 1;
    if (argc > 4) {
        FILE *p = fopen(argv[4], "wb");
        if (p) {
            fprintf(p, "P6 240 240 255\n");
            for (i = 0; i < 240u * 240u; i++) {
                const uint16_t c = (uint16_t)((screen[i] >> 8) | (screen[i] << 8));
                const unsigned char rgb[3] = {(unsigned char)((c >> 11) << 3), (unsigned char)(((c >> 5) & 63) << 2),
                                              (unsigned char)((c & 31) << 3)};
                fwrite(rgb, 1, 3, p);
            }
            fclose(p);
        }
    }
    printf("follow_test: %s\n", fails ? "FAILED" : "ok");
    return fails;
}
