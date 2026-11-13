/* SPDX-License-Identifier: MPL-2.0 */
/* Exercise the real canvas with a deferred DMA reader. A buffer must remain
 * unchanged until lcd_sync consumes it, including the return to ordinary UI. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
static const uint16_t *pending;
static uint16_t snapshot[240 * 144];
static uint32_t pending_n, transfers;
static void lcd_sync(void)
{
    if (pending) {
        assert(memcmp(pending, snapshot, pending_n * sizeof *pending) == 0);
        pending = 0;
    }
}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)
{
    (void)x; (void)y;
    lcd_sync();
    pending = px;
    pending_n = w * h;
    memcpy(snapshot, px, pending_n * sizeof *px);
    transfers++;
}
#include "../../base/device/gfx.c"
int main(void)
{
    const uint32_t heights[] = {28, 72, 72, 68};
    uint32_t frame, band, i;
    for (frame = 0; frame < 10; frame++) {
        lcd_sync();
        for (band = 0; band < 4; band++) {
            cv_begin_pingpong(240, heights[band], band);
            assert(cv_px == cv_storage + (band & 1u) * (CV_MAX / 2u));
            for (i = 0; i < 240 * heights[band]; i++)
                cv_px[i] = (uint16_t)(i + frame * 73 + band * 13);
            /* Reading may continue throughout painting the other half. */
            if (pending)
                assert(memcmp(pending, snapshot, pending_n * sizeof *pending) == 0);
            cv_blit(0, 0);
        }
        cv_begin(240, 144, 0x1234);
        assert(!pending && cv_px == cv_storage);
        for (i = 0; i < CV_MAX; i++)
            assert(cv_px[i] == swap16(0x1234));
        cv_blit(0, 28);
    }
    lcd_sync();
    assert(transfers == 50);
    puts("video canvas: deferred DMA, alternating halves, full-canvas restoration passed");
    return 0;
}
