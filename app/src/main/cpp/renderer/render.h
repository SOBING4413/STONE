/* render.h - a small immediate-mode OpenGL ES 2.0 renderer.
 *
 * Everything the UI draws goes through these six primitives. Shapes use a
 * signed-distance rounded box in the fragment shader, so cards, pills, bars,
 * circles and chart lines all come from one shader with no texture atlas and
 * no geometry tessellation on the CPU.
 */
#ifndef STONE_RENDER_H
#define STONE_RENDER_H

typedef struct { float r, g, b, a; } Color;
typedef struct { float x, y, w, h; } Rect;

static inline Color color_rgba(float r, float g, float b, float a)
{ Color c; c.r = r; c.g = g; c.b = b; c.a = a; return c; }

static inline Color color_hex(unsigned int hex, float a)
{
    Color c;
    c.r = (float)((hex >> 16) & 0xFF) / 255.0f;
    c.g = (float)((hex >> 8) & 0xFF) / 255.0f;
    c.b = (float)(hex & 0xFF) / 255.0f;
    c.a = a;
    return c;
}

static inline Color color_alpha(Color c, float a) { c.a *= a; return c; }

static inline Color color_mix(Color a, Color b, float t)
{
    Color c;
    c.r = a.r + (b.r - a.r) * t;
    c.g = a.g + (b.g - a.g) * t;
    c.b = a.b + (b.b - a.b) * t;
    c.a = a.a + (b.a - a.a) * t;
    return c;
}

static inline Rect rect_make(float x, float y, float w, float h)
{ Rect r; r.x = x; r.y = y; r.w = w; r.h = h; return r; }

static inline Rect rect_inset(Rect r, float dx, float dy)
{ r.x += dx; r.y += dy; r.w -= dx * 2.0f; r.h -= dy * 2.0f; return r; }

static inline int rect_contains(Rect r, float x, float y)
{ return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h; }

/* --- lifecycle ------------------------------------------------------------ */
int  render_init(void);              /* creates GL objects; 0 on failure */
void render_shutdown(void);          /* called when the GL context is lost */
void render_begin(int width, int height, Color clear);
void render_end(void);
int  render_width(void);
int  render_height(void);
float render_scale(void);            /* density scale, 1.0 at 420 px wide */
void render_set_density(float dp_scale);

/* --- clipping ------------------------------------------------------------- */
void render_push_clip(Rect r);
void render_pop_clip(void);

/* --- page transform --------------------------------------------------------
   A single global offset + alpha fade applied to every primitive drawn while
   it is active. Used to slide/fade whole screens in during page navigation
   without every widget having to know about the animation. Reset to the
   identity (0, 0, 1) at the start of every render_begin(). */
void render_set_page_transform(float dx, float dy, float alpha);
void render_clear_page_transform(void);

/* --- primitives ----------------------------------------------------------- */
void render_rect(Rect r, Color c, float radius);
void render_rect_gradient(Rect r, Color top, Color bottom, float radius);
void render_rect_outline(Rect r, Color c, float radius, float thickness);
void render_circle(float cx, float cy, float radius, Color c);
void render_ring(float cx, float cy, float radius, float thickness, Color c);
void render_line(float x0, float y0, float x1, float y1, float w, Color c);
/* Progress arc drawn as a segmented ring: t in [0,1]. */
void render_arc(float cx, float cy, float radius, float thickness,
                float t, Color c);

/* --- images ----------------------------------------------------------------
   Textures come from the mip-mapped pack in assets/stone_pack.stpk (see
   platform/assets.h). Lookup is by name; the level uploaded is chosen from
   the destination rectangle, so a 1024 px master costs a 256 px texture on a
   small phone. `tint` multiplies the texel, so pass white for a plain draw
   and a theme colour to recolour a monochrome mark. Every call is a silent
   no-op when the pack is absent, which is what lets the UI fall back to its
   vector drawing without a branch at every call site.

   `radius` rounds the destination corners in the fragment shader, exactly
   the way the rounded-box shapes do, so artwork can sit inside a card
   without square corners poking out of it. */
int  render_image_available(const char *name);
void render_image(const char *name, Rect dst, Color tint, float radius);
/* Sub-rectangle of the atlas, in 0..1 texture coordinates. */
void render_image_sub(const char *name, Rect dst,
                      float u0, float v0, float u1, float v1,
                      Color tint, float radius);
/* Aspect-preserving cover fit: fills `dst` completely, centre-cropping the
   overflow, the way a CSS background-size:cover would. */
void render_image_cover(const char *name, Rect dst, Color tint, float radius);

/* --- text ----------------------------------------------------------------- */
#define TEXT_LEFT   0
#define TEXT_CENTER 1
#define TEXT_RIGHT  2

void  render_text(const char *s, float x, float y, float size, Color c, int bold);
void  render_text_aligned(const char *s, Rect box, float size, Color c,
                          int bold, int align);
float render_text_width(const char *s, float size, int bold);
float render_line_height(float size);
/* Word-wraps into `box.w`; returns the total height used. Pass draw=0 to
   measure only. */
float render_text_wrapped(const char *s, Rect box, float size, Color c,
                          int bold, int draw);
/* Copies at most `max` bytes of `s`, appending "..." when it does not fit. */
void  render_text_ellipsis(const char *s, float max_w, float size, int bold,
                           char *out, int out_len);

#endif /* STONE_RENDER_H */
