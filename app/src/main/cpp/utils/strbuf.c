#include "strbuf.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void sb_init(StrBuf *sb)
{
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
    sb->oom = 0;
}

void sb_free(StrBuf *sb)
{
    free(sb->data);
    sb_init(sb);
}

void sb_reset(StrBuf *sb)
{
    sb->len = 0;
    sb->oom = 0;
    if (sb->data && sb->cap) sb->data[0] = '\0';
}

int sb_reserve(StrBuf *sb, size_t extra)
{
    size_t need;
    char *p;

    if (sb->oom) return 0;
    need = sb->len + extra + 1;
    if (need <= sb->cap) return 1;

    if (sb->cap == 0) sb->cap = 128;
    while (sb->cap < need) {
        if (sb->cap > ((size_t)1 << 27)) { sb->oom = 1; return 0; }
        sb->cap *= 2;
    }
    p = (char *)realloc(sb->data, sb->cap);
    if (!p) { sb->oom = 1; return 0; }
    sb->data = p;
    return 1;
}

void sb_putc(StrBuf *sb, char c)
{
    if (!sb_reserve(sb, 1)) return;
    sb->data[sb->len++] = c;
    sb->data[sb->len] = '\0';
}

void sb_write(StrBuf *sb, const char *s, size_t n)
{
    if (!s || n == 0) return;
    if (!sb_reserve(sb, n)) return;
    memcpy(sb->data + sb->len, s, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
}

void sb_puts(StrBuf *sb, const char *s)
{
    if (s) sb_write(sb, s, strlen(s));
}

void sb_printf(StrBuf *sb, const char *fmt, ...)
{
    va_list ap;
    char stack[256];
    int n;

    va_start(ap, fmt);
    n = vsnprintf(stack, sizeof(stack), fmt, ap);
    va_end(ap);
    if (n < 0) return;

    if ((size_t)n < sizeof(stack)) {
        sb_write(sb, stack, (size_t)n);
        return;
    }
    if (!sb_reserve(sb, (size_t)n)) return;
    va_start(ap, fmt);
    vsnprintf(sb->data + sb->len, (size_t)n + 1, fmt, ap);
    va_end(ap);
    sb->len += (size_t)n;
}

char *sb_detach(StrBuf *sb)
{
    char *p;
    if (sb->oom) { sb_free(sb); return NULL; }
    if (!sb->data) {
        if (!sb_reserve(sb, 1)) return NULL;
        sb->data[0] = '\0';
    }
    p = sb->data;
    sb_init(sb);
    return p;
}

void stone_strlcpy(char *dst, const char *src, size_t dstsize)
{
    size_t i;
    if (!dst || dstsize == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    for (i = 0; i + 1 < dstsize && src[i]; ++i) dst[i] = src[i];
    dst[i] = '\0';
}

int stone_streq(const char *a, const char *b)
{
    if (a == b) return 1;
    if (!a || !b) return 0;
    return strcmp(a, b) == 0;
}
