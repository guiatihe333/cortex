/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* AMB-1 FM6's DX7 banks: EN_NBANK sectors of the flash's data area from BANK_BASE, each holding a 32-voice bulk
 * dump's voices as the DX7 packs them (VMEM, 128 bytes a voice, 4096 a bank; an empty slot is erased flash). Part
 * of the unity build, after app.h.
 *
 * They come from a computer over USB-MIDI, as SysEx: a whole bulk dump is 4104 bytes and the FM-1 takes frames of
 * 640 at most, so tools/amb_syx.py sends it in 16 chunks of 256 (manufacturer 7D, the non-commercial one, then "AM"):
 *   7D 41 4D 01 bank chunk <256 bytes>   a chunk (chunk 0 erases the bank first)   answer 7D 41 4D 02 bank chunk ok
 *   7D 41 4D 03 bank                      the bank erased                          answer 7D 41 4D 04 bank ok
 *   7D 41 4D 05                           how many voices each bank holds          answer 7D 41 4D 06 <16 counts>
 * And a DX7 single voice dump (43 0n 00 01 1B <155> sum: what Dexed sends) goes straight into the voice whose pages
 * are shown, edited (OWN), as the FM pages leave it.
 *
 * Built in (bank_rom.h, tools/gen_banks.py): sixteen banks a fresh FM-1 has (one never sent a bank, or back from
 * M-VAVE's firmware, which erases the data area). A bank whose first slot is erased flash plays the built-in one;
 * a bank sent replaces it, and erasing it (7D 41 4D 03) brings the built-in one back. */
#include "bank_rom.h"
#include "bank_pads.h"                          /* B17: AMB-1 FM6's own pads (tools/gen_pads.py), built in only */
_Static_assert(EN_NBANK <= 16, "a built-in bank each");

static const uint8_t *bank_slot(int src)        /* a bank source's packed voice; 0: not a bank's */
{
    const uint8_t *f;
    src -= EN_NFACT;
    if (src >= EN_NBANK * 32 && src < (EN_NBANK + 1) * 32)
        return BANK_PADS + (uint32_t)(src % 32) * 128u;
    if (src < 0 || src >= EN_NBANK * 32)
        return 0;
    f = plat_data(BANK_BASE + (uint32_t)(src / 32) * 4096u);
    if (!f || (f[0] == 0xFFu && f[1] == 0xFFu))  /* (its first slot erased: none sent) */
        return BANK_ROM[src / 32] + (uint32_t)(src % 32) * 128u;
    return f + (uint32_t)(src % 32) * 128u;
}

int bank_voice(int voice, int src, uint8_t *p)
{
    const uint8_t *m;
    if ((unsigned)src < EN_NFACT) {
        en_factory(voice, src, proj.par[P_BTONE], p);
        return 1;
    }
    m = bank_slot(src);
    return m && en_unpack(m, p);
}

int bank_name(int src, char *b)
{
    uint8_t p[EN_PATCH + 1];
    int k, n = 0;
    if (!bank_voice(0, src, p))
        return 0;
    for (k = 0; k < 10; k++)
        b[k] = (char)p[EP_NAME + k];
    for (k = 0; k < 10; k++)                     /* trimmed */
        if (b[k] != ' ')
            n = k + 1;
    b[n] = 0;
    if (!n) {                                    /* (a voice with no name: its place) */
        en_param_text(P_PADTONE, src, b);
        return 1;
    }
    return 1;
}

/* the Patch knob's order: AMB-1's own eighteen, the pads (B17), then the banks B01 .. B16 */
static int src_pos(int s) { return s < EN_NFACT ? s : s >= EN_PADS_SRC ? s - EN_NBANK * 32 : s + 32; }
static int pos_src(int q) { return q < EN_NFACT ? q : q < EN_NFACT + 32 ? q + EN_NBANK * 32 : q - 32; }
int bank_next(int src, int dir)
{
    uint8_t p[EN_PATCH + 1];
    int q = src_pos(src), k;
    for (k = 0; k < EN_NSRC; k++) {
        int s;
        q += dir;
        if (q < 0 || q >= EN_NSRC)
            return src;                          /* the ends: it stays */
        s = pos_src(q);
        if (s < EN_NFACT || bank_voice(0, s, p))
            return s;
    }
    return src;
}

static int bank_count(int b)                     /* the voices a bank holds */
{
    uint8_t p[EN_PATCH + 1];
    int v, n = 0;
    for (v = 0; v < 32; v++)
        n += bank_voice(0, EN_NFACT + b * 32 + v, p);
    return n;
}

static void sx_answer(const uint8_t *a, uint32_t n)
{
    uint8_t f[24];
    uint32_t i;
    f[0] = 0xF0;
    for (i = 0; i < n && i < 22u; i++)
        f[1 + i] = a[i];
    f[1 + i] = 0xF7;
    plat_sysex_send(f, i + 2u);
}

/* the songs (tools/amb_mid.py): 7D 41 4D 07 slot chunk <256>  a chunk at chunk * 256 (a 4 KiB sector erased as one
 * begins)                                        answer 08 slot chunk ok (0)
 *   7D 41 4D 09 slot      its name and length    answer 0A slot <12 name> <sixteenths, two 7-bit>
 *   7D 41 4D 0B slot      sent: taken (played, if the piece's SONG is it)   answer 0C slot ok
 *   7D 41 4D 0D slot      erased                 answer 0E slot ok */
const uint8_t *song_rec(int slot)
{
    const uint8_t *r;
    int n;
    if ((unsigned)slot >= EN_NSONG || !(r = plat_data(SONG_BASE + (uint32_t)slot * SONG_SIZE)))
        return 0;
    n = r[5] << 7 | r[6];
    if (r[0] != 'A' || r[1] != 'S' || r[2] != 'N' || r[3] != 'G' || r[4] != 1 || n < 16 || n > EN_SONG_MAXN ||
        EN_SONG_HDR + 3 * ((n + 7) / 8) + 4 * n > (int)SONG_SIZE)
        return 0;
    return r;
}
void ui_song_writing(int slot);
void ui_song_in(int slot);
void ui_reel_in(int ok);

/* a reel from a file (FITA.html), its data 7-bit packed (each 7 bytes: a byte of their top bits, then their low 7):
 *   7D 41 4D 12            begin: the reel erased, nothing playing from it   answer 13 busy (1: the reel being saved)
 *   7D 41 4D 10 part hi lo <packed 256 bytes>   chunk hi << 7 | lo of part 0 (its splices, 80 bytes), 1 (its blocks'
 *                          states) or 2 (its samples)                       answer 11 part hi lo ok (0)
 *   7D 41 4D 14            end: its splices taken, the reel saved, SOURCE File   answer 15 ok (0) */
static uint8_t reel_meta_in[256];              /* (its keys, past FITA's 80 bytes, zero: studied here) */
static int unpack7(const uint8_t *p, uint32_t n, uint8_t *out, uint32_t max)
{
    uint32_t i = 0, k = 0;
    while (i < n) {
        const uint8_t hi = p[i++];
        uint32_t j;
        for (j = 0; j < 7u && i < n; j++, i++) {
            if (k >= max)
                return (int)k;
            out[k++] = (uint8_t)(p[i] | (hi >> j & 1u) << 7);
        }
    }
    return (int)k;
}

int app_sysex(const uint8_t *p, uint32_t n)
{
    if (n >= 4 && p[0] == 0x7D && p[1] == 0x41 && p[2] == 0x4D) {
        uint8_t a[22] = {0x7D, 0x41, 0x4D};
        if (p[3] == 0x01 && n == 6u + 256u && p[4] < EN_NBANK && p[5] < 16u) {
            uint32_t off = BANK_BASE + (uint32_t)p[4] * 4096u;
            int ok = 1;
            if (p[5] == 0)
                ok = plat_data_erase(off) == 0;
            ok = ok && plat_data_write(off + (uint32_t)p[5] * 256u, p + 6, 256u) == 0;
            a[3] = 0x02, a[4] = p[4], a[5] = p[5], a[6] = (uint8_t)!ok;
            sx_answer(a, 7);
            if (ok && p[5] == 15u)
                ui_bank_in(p[4], bank_count(p[4]));
        } else if (p[3] == 0x03 && n == 5u && p[4] < EN_NBANK) {
            a[3] = 0x04, a[4] = p[4], a[5] = (uint8_t)(plat_data_erase(BANK_BASE + (uint32_t)p[4] * 4096u) != 0);
            sx_answer(a, 6);
        } else if (p[3] == 0x07 && n == 6u + 256u && p[4] < EN_NSONG && p[5] < SONG_SIZE / 256u) {
            const uint32_t off = SONG_BASE + (uint32_t)p[4] * SONG_SIZE + (uint32_t)p[5] * 256u;
            int ok = 1;
            if (off % 4096u == 0u) {
                ui_song_writing(p[4]);           /* (the song playing let go first: its flash is rewritten) */
                ok = plat_data_erase(off) == 0;
            }
            ok = ok && plat_data_write(off, p + 6, 256u) == 0;
            a[3] = 0x08, a[4] = p[4], a[5] = p[5], a[6] = (uint8_t)!ok;
            sx_answer(a, 7);
        } else if ((p[3] == 0x09 || p[3] == 0x0B || p[3] == 0x0D) && n == 5u && p[4] < EN_NSONG) {
            const uint8_t *r;
            int k;
            if (p[3] == 0x0D) {                  /* erased: its first sector (its header) */
                ui_song_writing(p[4]);
                a[5] = (uint8_t)(plat_data_erase(SONG_BASE + (uint32_t)p[4] * SONG_SIZE) != 0);
            } else if (p[3] == 0x0B) {
                ui_song_in(p[4]);
                a[5] = (uint8_t)(song_rec(p[4]) == 0);
            }
            a[3] = (uint8_t)(p[3] + 1);
            a[4] = p[4];
            if (p[3] == 0x09) {
                r = song_rec(p[4]);
                for (k = 0; k < 12; k++)
                    a[5 + k] = r ? (uint8_t)(r[12 + k] & 0x7F) : (uint8_t)' ';
                a[17] = r ? r[5] : 0;
                a[18] = r ? r[6] : 0;
                sx_answer(a, 19);
            } else {
                sx_answer(a, 6);
            }
        } else if (p[3] == 0x12 && n == 4u) {
            a[3] = 0x13;
            a[4] = (uint8_t)(project_reel_progress() >= 0);
            if (!a[4]) {
                en_reel(EN_REEL_ERASE);          /* (nothing played from it while its blocks come in) */
                memset(reel_meta_in, 0, sizeof reel_meta_in);
            }
            sx_answer(a, 5);
        } else if (p[3] == 0x10 && n > 7u && p[4] <= 2u) {
            static uint8_t d[256];
            const uint32_t at = ((uint32_t)p[5] << 7 | p[6]) * 256u;
            const int k = unpack7(p + 7, n - 7u, d, sizeof d);
            uint32_t sz;
            uint8_t *dst = p[4] == 0 ? reel_meta_in : (uint8_t *)en_reel_mem(p[4] == 1 ? 1 : 3, &sz);
            int ok;
            if (p[4] == 0)
                sz = sizeof reel_meta_in;
            ok = at + (uint32_t)k <= sz;
            if (ok)
                memcpy(dst + at, d, (uint32_t)k);
            a[3] = 0x11, a[4] = p[4], a[5] = p[5], a[6] = p[6], a[7] = (uint8_t)!ok;
            sx_answer(a, 8);
        } else if (p[3] == 0x14 && n == 4u) {
            en_reel_take(reel_meta_in);
            ui_reel_in(1);
            a[3] = 0x15;
            a[4] = 0;
            sx_answer(a, 5);
        } else if (p[3] == 0x05) {
            int b;
            a[3] = 0x06;
            for (b = 0; b < EN_NBANK; b++)
                a[4 + b] = (uint8_t)bank_count(b);
            sx_answer(a, 4u + EN_NBANK);
        }
        return 1;
    }
    if (n == 1u + 1u + 3u + 155u + 1u && p[0] == 0x43 && (p[1] & 0xF0u) == 0 && p[2] == 0 && p[3] == 0x01 &&
        p[4] == 0x1B) {                          /* a DX7 single voice (VCED): into the voice on the screen */
        uint32_t k, sum = 0;
        int v = ui_voice();
        for (k = 0; k < 155u; k++)
            sum += p[5 + k];
        if (((128u - (sum & 127u)) & 127u) != p[160])
            return 1;                            /* (a bad checksum: taken, and dropped) */
        for (k = 0; k < 155u; k++)               /* (a value past its range: held to it, as a DX7 plays it) */
            vpatch[v][k] = p[5 + k] > en_patch_max((int)k) ? en_patch_max((int)k) : p[5 + k];
        proj.par[P_VX(v, VX_OWN)] = 1;
        en_set(P_VX(v, VX_OWN), 1);
        project_patch_send(v, 1);
        ui_patch_in(v);
        return 1;
    }
    return 0;
}
