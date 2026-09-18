#include "font.h"

#include <stdlib.h>
#include <string.h>

static int b64_val(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

/* Base64 -> raw bytes. Returns malloc'd buffer, sets *out_len. */
static unsigned char *b64_decode(const char *src, int len, int *out_len)
{
    unsigned char *out;
    /* acc must be unsigned: shifting a signed accumulator left by six is
       undefined once the value passes INT_MAX, which UBSan flags. */
    unsigned int acc = 0;
    int i, bits = 0, n = 0;

    if (!src || len <= 0) return NULL;
    out = (unsigned char *)malloc((size_t)len / 4 * 3 + 4);
    if (!out) return NULL;

    for (i = 0; i < len; ++i) {
        int v;
        if (src[i] == '=' ) break;
        v = b64_val(src[i]);
        if (v < 0) continue;                  /* skip whitespace / newlines */
        acc = (acc << 6) | (unsigned int)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out[n++] = (unsigned char)((acc >> bits) & 0xFFu);
        }
    }
    *out_len = n;
    return out;
}

unsigned char *stone_font_decode(const StoneFontData *fd, int *out_w, int *out_h)
{
    unsigned char *rle, *img;
    int rle_len = 0, i, pos = 0, total;

    if (!fd) return NULL;
    total = fd->atlas_w * fd->atlas_h;
    if (total <= 0 || total > 16 * 1024 * 1024) return NULL;

    rle = b64_decode(fd->b64, fd->b64_len, &rle_len);
    if (!rle) return NULL;

    img = (unsigned char *)calloc(1, (size_t)total);
    if (!img) { free(rle); return NULL; }

    /* RLE pairs: <value><run length 1..255> */
    for (i = 0; i + 1 < rle_len && pos < total; i += 2) {
        int run = rle[i + 1];
        if (run > total - pos) run = total - pos;
        memset(img + pos, rle[i], (size_t)run);
        pos += run;
    }
    free(rle);

    if (out_w) *out_w = fd->atlas_w;
    if (out_h) *out_h = fd->atlas_h;
    return img;
}
