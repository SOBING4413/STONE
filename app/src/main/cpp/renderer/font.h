/* font.h - bitmap font atlas baked into the binary.
 *
 * tools/gen_font.py rasterises Liberation Sans (SIL OFL 1.1) into an 8-bit
 * coverage atlas, RLE-compresses it and Base64-encodes it into font_data.c.
 * Nothing is loaded from disk or from the network at runtime.
 */
#ifndef STONE_FONT_H
#define STONE_FONT_H

#define STONE_FONT_GLYPHS 95   /* ASCII 32..126 */
#define STONE_FONT_FIRST  32

typedef struct {
    short u, v;      /* top-left of the glyph inside the atlas, in pixels */
    short w, h;      /* ink size                                          */
    short bx, by;    /* offset from the pen position (top-left origin)    */
    float advance;   /* pen advance at the atlas rasterisation size       */
} StoneGlyph;

typedef struct {
    int   atlas_w, atlas_h;
    int   cell_w, cell_h;
    float px;              /* size the atlas was rasterised at */
    float ascent, descent, line_h;
    const char       *b64;
    int               b64_len;
    const StoneGlyph *glyphs;
} StoneFontData;

extern const StoneFontData k_stone_font_regular;
extern const StoneFontData k_stone_font_bold;

/* Decodes a font blob into a freshly malloc'd 8-bit coverage bitmap.
   Returns NULL on failure; caller frees. */
unsigned char *stone_font_decode(const StoneFontData *fd, int *out_w, int *out_h);

#endif /* STONE_FONT_H */
