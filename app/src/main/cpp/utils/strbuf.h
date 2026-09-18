/* strbuf.h - growable byte buffer used for JSON writing and text formatting. */
#ifndef STONE_STRBUF_H
#define STONE_STRBUF_H

#include <stddef.h>

typedef struct {
    char  *data;
    size_t len;
    size_t cap;
    int    oom; /* sticky allocation-failure flag */
} StrBuf;

void  sb_init(StrBuf *sb);
void  sb_free(StrBuf *sb);
void  sb_reset(StrBuf *sb);
int   sb_reserve(StrBuf *sb, size_t extra);
void  sb_putc(StrBuf *sb, char c);
void  sb_write(StrBuf *sb, const char *s, size_t n);
void  sb_puts(StrBuf *sb, const char *s);
void  sb_printf(StrBuf *sb, const char *fmt, ...);
/* Detach the buffer: caller owns the returned pointer (free() it). */
char *sb_detach(StrBuf *sb);

void  stone_strlcpy(char *dst, const char *src, size_t dstsize);
int   stone_streq(const char *a, const char *b);

#endif /* STONE_STRBUF_H */
