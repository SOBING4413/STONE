#include "render.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <GLES2/gl2.h>

#include "font.h"
#include "../platform/assets.h"

#ifdef __ANDROID__
#include <android/log.h>
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "STONE", __VA_ARGS__)
#else
#include <stdio.h>
#define LOGE(...) fprintf(stderr, __VA_ARGS__)
#endif

#define MAX_CLIPS      8
#define MAX_TEXT_QUADS 512

/* --------------------------------------------------------------- shaders */

static const char *k_shape_vs =
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_local;\n"
    "attribute float a_t;\n"
    "uniform vec2 u_viewport;\n"
    "varying vec2 v_local;\n"
    "varying float v_t;\n"
    "void main() {\n"
    "    v_local = a_local;\n"
    "    v_t = a_t;\n"
    "    vec2 ndc = vec2(a_pos.x / u_viewport.x * 2.0 - 1.0,\n"
    "                    1.0 - a_pos.y / u_viewport.y * 2.0);\n"
    "    gl_Position = vec4(ndc, 0.0, 1.0);\n"
    "}\n";

/* Rounded-box signed distance: negative inside, positive outside. */
static const char *k_shape_fs =
    "precision mediump float;\n"
    "uniform vec4 u_color0;\n"
    "uniform vec4 u_color1;\n"
    "uniform vec2 u_half;\n"
    "uniform float u_radius;\n"
    "uniform float u_thickness;\n"
    "varying vec2 v_local;\n"
    "varying float v_t;\n"
    "void main() {\n"
    "    vec2 d = abs(v_local) - (u_half - vec2(u_radius));\n"
    "    float dist = length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - u_radius;\n"
    "    float a = 1.0 - smoothstep(-0.75, 0.75, dist);\n"
    "    if (u_thickness > 0.0) {\n"
    "        a *= smoothstep(-0.75, 0.75, dist + u_thickness);\n"
    "    }\n"
    "    vec4 col = mix(u_color0, u_color1, v_t);\n"
    "    gl_FragColor = vec4(col.rgb, col.a * a);\n"
    "    if (gl_FragColor.a < 0.002) discard;\n"
    "}\n";

static const char *k_text_vs =
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_uv;\n"
    "uniform vec2 u_viewport;\n"
    "varying vec2 v_uv;\n"
    "void main() {\n"
    "    v_uv = a_uv;\n"
    "    vec2 ndc = vec2(a_pos.x / u_viewport.x * 2.0 - 1.0,\n"
    "                    1.0 - a_pos.y / u_viewport.y * 2.0);\n"
    "    gl_Position = vec4(ndc, 0.0, 1.0);\n"
    "}\n";

static const char *k_text_fs =
    "precision mediump float;\n"
    "uniform sampler2D u_tex;\n"
    "uniform vec4 u_color;\n"
    "varying vec2 v_uv;\n"
    "void main() {\n"
    "    float a = texture2D(u_tex, v_uv).a;\n"
    "    gl_FragColor = vec4(u_color.rgb, u_color.a * a);\n"
    "}\n";

/* Images carry the same local-space coordinate the shape shader uses, so one
   program covers "draw the artwork", "recolour this monochrome mark" and
   "round the corners to match the card behind it" without a stencil pass or
   a second texture. */
static const char *k_image_vs =
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_uv;\n"
    "attribute vec2 a_local;\n"
    "uniform vec2 u_viewport;\n"
    "varying vec2 v_uv;\n"
    "varying vec2 v_local;\n"
    "void main() {\n"
    "    v_uv = a_uv;\n"
    "    v_local = a_local;\n"
    "    vec2 ndc = vec2(a_pos.x / u_viewport.x * 2.0 - 1.0,\n"
    "                    1.0 - a_pos.y / u_viewport.y * 2.0);\n"
    "    gl_Position = vec4(ndc, 0.0, 1.0);\n"
    "}\n";

static const char *k_image_fs =
    "precision mediump float;\n"
    "uniform sampler2D u_tex;\n"
    "uniform vec4 u_color;\n"
    "uniform vec2 u_half;\n"
    "uniform float u_radius;\n"
    "varying vec2 v_uv;\n"
    "varying vec2 v_local;\n"
    "void main() {\n"
    "    vec4 t = texture2D(u_tex, v_uv);\n"
    "    float a = 1.0;\n"
    "    if (u_radius > 0.0) {\n"
    "        vec2 d = abs(v_local) - (u_half - vec2(u_radius));\n"
    "        float dist = length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - u_radius;\n"
    "        a = 1.0 - smoothstep(-0.75, 0.75, dist);\n"
    "    }\n"
    "    gl_FragColor = vec4(t.rgb * u_color.rgb, t.a * u_color.a * a);\n"
    "    if (gl_FragColor.a < 0.002) discard;\n"
    "}\n";

/* ----------------------------------------------------------------- state */

typedef struct {
    GLuint tex;
    const StoneFontData *fd;
} FontFace;

/* A tiny fixed cache is the right shape here: the pack holds six textures and
   at most three of them are on screen at once, so there is nothing to evict
   and no allocator to run. */
#define MAX_IMAGES 8

typedef struct {
    char   name[32];
    GLuint tex;
    int    width, height;    /* dimensions of the mip that is resident */
    int    level;
    int    failed;           /* looked up once and not found; stop retrying */
} ImageSlot;

static struct {
    int ready;
    int width, height;
    float density;

    GLuint shape_prog, text_prog, image_prog;
    GLint  s_pos, s_local, s_t, s_viewport, s_color0, s_color1, s_half, s_radius, s_thick;
    GLint  t_pos, t_uv, t_viewport, t_color, t_tex;
    GLint  i_pos, i_uv, i_local, i_viewport, i_color, i_tex, i_half, i_radius;

    ImageSlot images[MAX_IMAGES];
    int       image_count;

    FontFace regular, bold;

    Rect clips[MAX_CLIPS];
    int  clip_count;

    float tr_dx, tr_dy, tr_alpha;           /* active page transform */

    float verts[6 * 5];                     /* one shape: 6 verts x (x,y,lx,ly,t) */
    float text_buf[MAX_TEXT_QUADS * 6 * 4]; /* glyph quads: x,y,u,v               */
} R;

/* ------------------------------------------------------------- gl helpers */

static GLuint compile_shader(GLenum type, const char *src)
{
    GLuint s = glCreateShader(type);
    GLint ok = 0;
    if (!s) return 0;
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, sizeof(log), NULL, log);
        LOGE("shader compile failed: %s", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static GLuint link_program(const char *vs_src, const char *fs_src)
{
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_src);
    GLuint p;
    GLint ok = 0;

    if (!vs || !fs) { if (vs) glDeleteShader(vs); if (fs) glDeleteShader(fs); return 0; }
    p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(p, sizeof(log), NULL, log);
        LOGE("program link failed: %s", log);
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

static int upload_face(FontFace *face, const StoneFontData *fd)
{
    int w = 0, h = 0;
    unsigned char *pixels = stone_font_decode(fd, &w, &h);
    if (!pixels) return 0;

    glGenTextures(1, &face->tex);
    glBindTexture(GL_TEXTURE_2D, face->tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, w, h, 0, GL_ALPHA, GL_UNSIGNED_BYTE, pixels);
    /* NPOT-safe settings for GLES2: clamp + linear, no mipmaps. */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    free(pixels);

    face->fd = fd;
    return face->tex != 0;
}

int render_init(void)
{
    memset(&R, 0, sizeof(R));
    R.density = 1.0f;

    R.shape_prog = link_program(k_shape_vs, k_shape_fs);
    R.text_prog  = link_program(k_text_vs, k_text_fs);
    R.image_prog = link_program(k_image_vs, k_image_fs);
    if (!R.shape_prog || !R.text_prog || !R.image_prog) return 0;

    R.s_pos      = glGetAttribLocation(R.shape_prog, "a_pos");
    R.s_local    = glGetAttribLocation(R.shape_prog, "a_local");
    R.s_t        = glGetAttribLocation(R.shape_prog, "a_t");
    R.s_viewport = glGetUniformLocation(R.shape_prog, "u_viewport");
    R.s_color0   = glGetUniformLocation(R.shape_prog, "u_color0");
    R.s_color1   = glGetUniformLocation(R.shape_prog, "u_color1");
    R.s_half     = glGetUniformLocation(R.shape_prog, "u_half");
    R.s_radius   = glGetUniformLocation(R.shape_prog, "u_radius");
    R.s_thick    = glGetUniformLocation(R.shape_prog, "u_thickness");

    R.t_pos      = glGetAttribLocation(R.text_prog, "a_pos");
    R.t_uv       = glGetAttribLocation(R.text_prog, "a_uv");
    R.t_viewport = glGetUniformLocation(R.text_prog, "u_viewport");
    R.t_color    = glGetUniformLocation(R.text_prog, "u_color");
    R.t_tex      = glGetUniformLocation(R.text_prog, "u_tex");

    R.i_pos      = glGetAttribLocation(R.image_prog, "a_pos");
    R.i_uv       = glGetAttribLocation(R.image_prog, "a_uv");
    R.i_local    = glGetAttribLocation(R.image_prog, "a_local");
    R.i_viewport = glGetUniformLocation(R.image_prog, "u_viewport");
    R.i_color    = glGetUniformLocation(R.image_prog, "u_color");
    R.i_tex      = glGetUniformLocation(R.image_prog, "u_tex");
    R.i_half     = glGetUniformLocation(R.image_prog, "u_half");
    R.i_radius   = glGetUniformLocation(R.image_prog, "u_radius");

    if (!upload_face(&R.regular, &k_stone_font_regular)) return 0;
    if (!upload_face(&R.bold, &k_stone_font_bold)) return 0;

    R.ready = 1;
    return 1;
}

void render_shutdown(void)
{
    int i;
    for (i = 0; i < R.image_count; ++i)
        if (R.images[i].tex) glDeleteTextures(1, &R.images[i].tex);
    if (R.regular.tex) glDeleteTextures(1, &R.regular.tex);
    if (R.bold.tex)    glDeleteTextures(1, &R.bold.tex);
    if (R.shape_prog)  glDeleteProgram(R.shape_prog);
    if (R.text_prog)   glDeleteProgram(R.text_prog);
    if (R.image_prog)  glDeleteProgram(R.image_prog);
    memset(&R, 0, sizeof(R));
}

void render_begin(int width, int height, Color clear)
{
    R.width = width > 0 ? width : 1;
    R.height = height > 0 ? height : 1;
    R.clip_count = 0;
    R.tr_dx = 0.0f;
    R.tr_dy = 0.0f;
    R.tr_alpha = 1.0f;

    glViewport(0, 0, R.width, R.height);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(clear.r, clear.g, clear.b, clear.a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void render_end(void)
{
    glDisable(GL_SCISSOR_TEST);
}

int   render_width(void)  { return R.width; }
int   render_height(void) { return R.height; }
float render_scale(void)  { return R.density; }
void  render_set_density(float s) { R.density = (s > 0.3f && s < 6.0f) ? s : 1.0f; }

/* --------------------------------------------------------------- clipping */

static void apply_clip(void)
{
    if (R.clip_count == 0) { glDisable(GL_SCISSOR_TEST); return; }
    {
        Rect r = R.clips[R.clip_count - 1];
        int x = (int)r.x, y = (int)(R.height - (r.y + r.h));
        int w = (int)r.w, h = (int)r.h;
        if (w < 0) w = 0;
        if (h < 0) h = 0;
        glEnable(GL_SCISSOR_TEST);
        glScissor(x, y, w, h);
    }
}

void render_push_clip(Rect r)
{
    if (R.clip_count >= MAX_CLIPS) return;
    if (R.clip_count > 0) {
        /* Intersect with the parent so nested scroll areas behave. */
        Rect p = R.clips[R.clip_count - 1];
        float x0 = r.x > p.x ? r.x : p.x;
        float y0 = r.y > p.y ? r.y : p.y;
        float x1 = (r.x + r.w < p.x + p.w) ? r.x + r.w : p.x + p.w;
        float y1 = (r.y + r.h < p.y + p.h) ? r.y + r.h : p.y + p.h;
        r = rect_make(x0, y0, x1 - x0 > 0 ? x1 - x0 : 0, y1 - y0 > 0 ? y1 - y0 : 0);
    }
    R.clips[R.clip_count++] = r;
    apply_clip();
}

void render_pop_clip(void)
{
    if (R.clip_count > 0) R.clip_count--;
    apply_clip();
}

/* --------------------------------------------------------- page transform */

void render_set_page_transform(float dx, float dy, float alpha)
{
    R.tr_dx = dx;
    R.tr_dy = dy;
    R.tr_alpha = alpha;
}

void render_clear_page_transform(void)
{
    R.tr_dx = 0.0f;
    R.tr_dy = 0.0f;
    R.tr_alpha = 1.0f;
}

/* ------------------------------------------------------------- primitives */

static void shape_quad(float cx, float cy, float hx, float hy, float cos_a, float sin_a,
                       Color c0, Color c1, float radius, float thickness)
{
    static const float lx[6] = {-1, 1, 1, -1, -1, 1};
    static const float ly[6] = {-1, -1, 1, 1, -1, 1};
    /* triangle list: (0,1,2) (0,2,3) expressed through the table below */
    static const int idx[6] = {0, 1, 2, 0, 2, 3};
    float local[4][2];
    float world[4][2];
    int i;

    if (!R.ready) return;
    if (hx <= 0.0f || hy <= 0.0f) return;

    cx += R.tr_dx;
    cy += R.tr_dy;
    c0.a *= R.tr_alpha;
    c1.a *= R.tr_alpha;
    if (c0.a <= 0.0f && c1.a <= 0.0f) return;

    for (i = 0; i < 4; ++i) {
        float px = (i == 0 || i == 3) ? -hx : hx;
        float py = (i < 2) ? -hy : hy;
        local[i][0] = px;
        local[i][1] = py;
        world[i][0] = cx + px * cos_a - py * sin_a;
        world[i][1] = cy + px * sin_a + py * cos_a;
    }
    (void)lx; (void)ly;

    for (i = 0; i < 6; ++i) {
        int k = idx[i];
        R.verts[i * 5 + 0] = world[k][0];
        R.verts[i * 5 + 1] = world[k][1];
        R.verts[i * 5 + 2] = local[k][0];
        R.verts[i * 5 + 3] = local[k][1];
        R.verts[i * 5 + 4] = (local[k][1] + hy) / (hy * 2.0f); /* gradient t */
    }

    glUseProgram(R.shape_prog);
    glUniform2f(R.s_viewport, (float)R.width, (float)R.height);
    glUniform4f(R.s_color0, c0.r, c0.g, c0.b, c0.a);
    glUniform4f(R.s_color1, c1.r, c1.g, c1.b, c1.a);
    glUniform2f(R.s_half, hx, hy);
    glUniform1f(R.s_radius, radius);
    glUniform1f(R.s_thick, thickness);

    glEnableVertexAttribArray((GLuint)R.s_pos);
    glVertexAttribPointer((GLuint)R.s_pos, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), R.verts);
    glEnableVertexAttribArray((GLuint)R.s_local);
    glVertexAttribPointer((GLuint)R.s_local, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), R.verts + 2);
    glEnableVertexAttribArray((GLuint)R.s_t);
    glVertexAttribPointer((GLuint)R.s_t, 1, GL_FLOAT, GL_FALSE, 5 * sizeof(float), R.verts + 4);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glDisableVertexAttribArray((GLuint)R.s_pos);
    glDisableVertexAttribArray((GLuint)R.s_local);
    glDisableVertexAttribArray((GLuint)R.s_t);
}

static float clamp_radius(Rect r, float radius)
{
    float m = (r.w < r.h ? r.w : r.h) * 0.5f;
    if (radius < 0.0f) radius = 0.0f;
    return radius > m ? m : radius;
}

void render_rect(Rect r, Color c, float radius)
{
    if (r.w <= 0.0f || r.h <= 0.0f || c.a <= 0.0f) return;
    shape_quad(r.x + r.w * 0.5f, r.y + r.h * 0.5f, r.w * 0.5f + 1.0f, r.h * 0.5f + 1.0f,
               1.0f, 0.0f, c, c, clamp_radius(r, radius), 0.0f);
}

void render_rect_gradient(Rect r, Color top, Color bottom, float radius)
{
    if (r.w <= 0.0f || r.h <= 0.0f) return;
    shape_quad(r.x + r.w * 0.5f, r.y + r.h * 0.5f, r.w * 0.5f + 1.0f, r.h * 0.5f + 1.0f,
               1.0f, 0.0f, top, bottom, clamp_radius(r, radius), 0.0f);
}

void render_rect_outline(Rect r, Color c, float radius, float thickness)
{
    if (r.w <= 0.0f || r.h <= 0.0f || thickness <= 0.0f) return;
    shape_quad(r.x + r.w * 0.5f, r.y + r.h * 0.5f, r.w * 0.5f + 1.0f, r.h * 0.5f + 1.0f,
               1.0f, 0.0f, c, c, clamp_radius(r, radius), thickness);
}

void render_circle(float cx, float cy, float radius, Color c)
{
    if (radius <= 0.0f) return;
    shape_quad(cx, cy, radius + 1.0f, radius + 1.0f, 1.0f, 0.0f, c, c, radius, 0.0f);
}

void render_ring(float cx, float cy, float radius, float thickness, Color c)
{
    if (radius <= 0.0f || thickness <= 0.0f) return;
    shape_quad(cx, cy, radius + 1.0f, radius + 1.0f, 1.0f, 0.0f, c, c, radius, thickness);
}

void render_line(float x0, float y0, float x1, float y1, float w, Color c)
{
    float dx = x1 - x0, dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    float ca, sa;

    if (len < 0.0001f || w <= 0.0f) return;
    ca = dx / len;
    sa = dy / len;
    shape_quad((x0 + x1) * 0.5f, (y0 + y1) * 0.5f, len * 0.5f + w * 0.5f, w * 0.5f + 0.75f,
               ca, sa, c, c, w * 0.5f, 0.0f);
}

void render_arc(float cx, float cy, float radius, float thickness, float t, Color c)
{
    const int segments = 48;
    int i, n;
    float prev_x, prev_y;

    if (t <= 0.0f) return;
    if (t > 1.0f) t = 1.0f;
    n = (int)((float)segments * t + 0.5f);
    if (n < 1) n = 1;

    prev_x = cx;
    prev_y = cy - radius;
    for (i = 1; i <= n; ++i) {
        float a = -1.5707963f + 6.2831853f * ((float)i / (float)segments);
        float x = cx + cosf(a) * radius;
        float y = cy + sinf(a) * radius;
        render_line(prev_x, prev_y, x, y, thickness, c);
        prev_x = x;
        prev_y = y;
    }
}

/* ------------------------------------------------------------------- text */

static const FontFace *face_for(int bold)
{
    return bold ? &R.bold : &R.regular;
}

static const StoneGlyph *glyph_for(const StoneFontData *fd, unsigned char ch)
{
    int idx;
    if (ch < STONE_FONT_FIRST || ch > 126) ch = '?';
    idx = ch - STONE_FONT_FIRST;
    if (idx < 0 || idx >= STONE_FONT_GLYPHS) return NULL;
    return &fd->glyphs[idx];
}

float render_text_width(const char *s, float size, int bold)
{
    const FontFace *f = face_for(bold);
    float scale, w = 0.0f;

    if (!s || !R.ready || !f->fd) return 0.0f;
    scale = size / f->fd->px;
    for (; *s; ++s) {
        const StoneGlyph *g = glyph_for(f->fd, (unsigned char)*s);
        if (g) w += g->advance * scale;
    }
    return w;
}

float render_line_height(float size)
{
    const StoneFontData *fd = R.regular.fd;
    if (!fd) return size * 1.2f;
    return size * (fd->line_h / fd->px);
}

void render_text(const char *s, float x, float y, float size, Color c, int bold)
{
    const FontFace *f = face_for(bold);
    float scale, pen;
    int quads = 0;
    float *vb = R.text_buf;

    c.a *= R.tr_alpha;
    if (!s || !*s || !R.ready || !f->fd || c.a <= 0.0f) return;
    scale = size / f->fd->px;
    x += R.tr_dx;
    y += R.tr_dy;
    pen = x;

    glUseProgram(R.text_prog);
    glUniform2f(R.t_viewport, (float)R.width, (float)R.height);
    glUniform4f(R.t_color, c.r, c.g, c.b, c.a);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, f->tex);
    glUniform1i(R.t_tex, 0);

    glEnableVertexAttribArray((GLuint)R.t_pos);
    glEnableVertexAttribArray((GLuint)R.t_uv);

    for (; *s; ++s) {
        const StoneGlyph *g = glyph_for(f->fd, (unsigned char)*s);
        float gx, gy, gw, gh, u0, v0, u1, v1;
        int i;
        static const float cx[6] = {0, 1, 1, 0, 1, 0};
        static const float cy[6] = {0, 0, 1, 0, 1, 1};

        if (!g) continue;
        if (g->w > 0 && g->h > 0) {
            gx = pen + (float)g->bx * scale;
            gy = y + (float)g->by * scale;
            gw = (float)g->w * scale;
            gh = (float)g->h * scale;
            u0 = (float)g->u / (float)f->fd->atlas_w;
            v0 = (float)g->v / (float)f->fd->atlas_h;
            u1 = (float)(g->u + g->w) / (float)f->fd->atlas_w;
            v1 = (float)(g->v + g->h) / (float)f->fd->atlas_h;

            for (i = 0; i < 6; ++i) {
                float *v = vb + (quads * 6 + i) * 4;
                v[0] = gx + cx[i] * gw;
                v[1] = gy + cy[i] * gh;
                v[2] = u0 + cx[i] * (u1 - u0);
                v[3] = v0 + cy[i] * (v1 - v0);
            }
            quads++;

            if (quads >= MAX_TEXT_QUADS) {
                glVertexAttribPointer((GLuint)R.t_pos, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), vb);
                glVertexAttribPointer((GLuint)R.t_uv, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), vb + 2);
                glDrawArrays(GL_TRIANGLES, 0, quads * 6);
                quads = 0;
            }
        }
        pen += g->advance * scale;
    }

    if (quads > 0) {
        glVertexAttribPointer((GLuint)R.t_pos, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), vb);
        glVertexAttribPointer((GLuint)R.t_uv, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), vb + 2);
        glDrawArrays(GL_TRIANGLES, 0, quads * 6);
    }

    glDisableVertexAttribArray((GLuint)R.t_pos);
    glDisableVertexAttribArray((GLuint)R.t_uv);
}

void render_text_aligned(const char *s, Rect box, float size, Color c, int bold, int align)
{
    float w, x;
    float y = box.y + (box.h - render_line_height(size)) * 0.5f;

    if (!s) return;
    w = render_text_width(s, size, bold);
    if (align == TEXT_CENTER)      x = box.x + (box.w - w) * 0.5f;
    else if (align == TEXT_RIGHT)  x = box.x + box.w - w;
    else                           x = box.x;
    render_text(s, x, y, size, c, bold);
}

void render_text_ellipsis(const char *s, float max_w, float size, int bold,
                          char *out, int out_len)
{
    int i = 0;
    float w = 0.0f;
    const FontFace *f = face_for(bold);
    float scale, dots;

    if (!out || out_len <= 0) return;
    out[0] = '\0';
    if (!s || !R.ready || !f->fd) return;

    scale = size / f->fd->px;
    dots = render_text_width("...", size, bold);

    for (; s[i] && i < out_len - 4; ++i) {
        const StoneGlyph *g = glyph_for(f->fd, (unsigned char)s[i]);
        float adv = g ? g->advance * scale : 0.0f;
        if (w + adv > max_w - (s[i + 1] ? dots : 0.0f)) {
            memcpy(out, s, (size_t)i);
            out[i] = '\0';
            if (i + 4 < out_len) strcat(out, "...");
            return;
        }
        w += adv;
    }
    memcpy(out, s, (size_t)i);
    out[i] = '\0';
}

float render_text_wrapped(const char *s, Rect box, float size, Color c, int bold, int draw)
{
    char line[256];
    int len = 0;
    float y = box.y;
    float lh = render_line_height(size) * 0.98f;
    const char *p = s;

    if (!s || !*s) return 0.0f;

    while (*p) {
        const char *word = p;
        const char *scan = p;
        char test[256];
        int wlen;

        while (*scan && *scan != ' ' && *scan != '\n') scan++;
        wlen = (int)(scan - word);
        if (wlen > 200) wlen = 200;

        /* Build "line + word" and see whether it still fits. */
        if (len > 0) {
            memcpy(test, line, (size_t)len);
            test[len] = ' ';
            memcpy(test + len + 1, word, (size_t)wlen);
            test[len + 1 + wlen] = '\0';
        } else {
            memcpy(test, word, (size_t)wlen);
            test[wlen] = '\0';
        }

        if (render_text_width(test, size, bold) > box.w && len > 0) {
            line[len] = '\0';
            if (draw) render_text(line, box.x, y, size, c, bold);
            y += lh;
            memcpy(line, word, (size_t)wlen);
            len = wlen;
        } else {
            len = (int)strlen(test);
            if (len > (int)sizeof(line) - 1) len = (int)sizeof(line) - 1;
            memcpy(line, test, (size_t)len);
        }
        line[len] = '\0';

        if (*scan == '\n') {
            if (draw) render_text(line, box.x, y, size, c, bold);
            y += lh;
            len = 0;
            line[0] = '\0';
            scan++;
        } else if (*scan == ' ') {
            scan++;
        }
        p = scan;

        if (y > box.y + 4000.0f) break;   /* pathological input guard */
    }

    if (len > 0) {
        if (draw) render_text(line, box.x, y, size, c, bold);
        y += lh;
    }
    return y - box.y;
}

/* ----------------------------------------------------------------- images */

static void slot_set_name(ImageSlot *slot, const char *name)
{
    size_t n = strlen(name);
    if (n > sizeof(slot->name) - 1) n = sizeof(slot->name) - 1;
    memcpy(slot->name, name, n);
    slot->name[n] = '\0';
}

static ImageSlot *slot_find(const char *name)
{
    int i;
    for (i = 0; i < R.image_count; ++i)
        if (strcmp(R.images[i].name, name) == 0) return &R.images[i];
    return NULL;
}

/* Returns a resident texture for `name`, uploading (or re-uploading at a
   sharper mip) if the caller needs more pixels than the level currently in
   VRAM. NULL means "draw nothing": either the pack is missing or the name is
   not in it, and every caller treats that as a clean no-op. */
static ImageSlot *image_slot(const char *name, float want_px)
{
    StoneAssetImage img;
    ImageSlot *slot;
    const unsigned char *px;
    int level, best, w = 0, h = 0;

    if (!R.ready || !name || !name[0]) return NULL;

    slot = slot_find(name);
    if (slot) {
        if (slot->failed) return NULL;
        /* Already sharp enough, or already at the master level. */
        if (slot->tex && (slot->level == 0 || want_px <= (float)slot->width * 1.35f))
            return slot;
    }

    if (!stone_assets_image(name, &img)) {
        if (!slot && R.image_count < MAX_IMAGES) {
            slot = &R.images[R.image_count++];
            memset(slot, 0, sizeof(*slot));
            slot_set_name(slot, name);
        }
        if (slot) slot->failed = 1;
        return NULL;
    }

    /* Smallest mip that still covers the requested size. Walking down from
       the 1x1 end means the first level that is big enough is also the
       cheapest one that is big enough. */
    best = 0;
    for (level = img.mip_count - 1; level >= 0; --level) {
        int lw = img.width >> level;
        if (lw < 1) lw = 1;
        best = level;
        if ((float)lw >= want_px) break;
    }

    px = stone_assets_mip(&img, best, &w, &h);
    if (!px || w <= 0 || h <= 0) return NULL;

    if (!slot) {
        if (R.image_count >= MAX_IMAGES) return NULL;
        slot = &R.images[R.image_count++];
        memset(slot, 0, sizeof(*slot));
        slot_set_name(slot, name);
    }

    if (!slot->tex) {
        glGenTextures(1, &slot->tex);
        if (!slot->tex) { slot->failed = 1; return NULL; }
    }
    glBindTexture(GL_TEXTURE_2D, slot->tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    slot->width  = w;
    slot->height = h;
    slot->level  = best;
    return slot;
}

int render_image_available(const char *name)
{
    StoneAssetImage img;
    ImageSlot *slot;

    if (!name || !name[0]) return 0;
    slot = slot_find(name);
    if (slot) return !slot->failed;
    return stone_assets_image(name, &img);
}

void render_image_sub(const char *name, Rect dst,
                      float u0, float v0, float u1, float v1,
                      Color tint, float radius)
{
    ImageSlot *slot;
    float verts[6 * 6];
    static const float cx[6] = {0, 1, 1, 0, 1, 0};
    static const float cy[6] = {0, 0, 1, 0, 1, 1};
    float span, x, y, hx, hy, max_r;
    int i;

    if (dst.w <= 0.0f || dst.h <= 0.0f) return;

    tint.a *= R.tr_alpha;
    if (tint.a <= 0.0f) return;

    /* How many texels of the source the destination is stretching over: that,
       not the destination width, is what decides the mip level for an atlas
       tile. */
    span = u1 - u0;
    if (span < 0.0f) span = -span;
    if (span < 0.0001f) span = 1.0f;

    slot = image_slot(name, dst.w / span);
    if (!slot || !slot->tex) return;

    x  = dst.x + R.tr_dx;
    y  = dst.y + R.tr_dy;
    hx = dst.w * 0.5f;
    hy = dst.h * 0.5f;

    max_r = hx < hy ? hx : hy;
    if (radius < 0.0f)   radius = 0.0f;
    if (radius > max_r)  radius = max_r;

    for (i = 0; i < 6; ++i) {
        float *v = verts + i * 6;
        v[0] = x + cx[i] * dst.w;
        v[1] = y + cy[i] * dst.h;
        v[2] = u0 + cx[i] * (u1 - u0);
        v[3] = v0 + cy[i] * (v1 - v0);
        v[4] = (cx[i] * 2.0f - 1.0f) * hx;
        v[5] = (cy[i] * 2.0f - 1.0f) * hy;
    }

    glUseProgram(R.image_prog);
    glUniform2f(R.i_viewport, (float)R.width, (float)R.height);
    glUniform4f(R.i_color, tint.r, tint.g, tint.b, tint.a);
    glUniform2f(R.i_half, hx, hy);
    glUniform1f(R.i_radius, radius);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, slot->tex);
    glUniform1i(R.i_tex, 0);

    glEnableVertexAttribArray((GLuint)R.i_pos);
    glVertexAttribPointer((GLuint)R.i_pos, 2, GL_FLOAT, GL_FALSE,
                          6 * sizeof(float), verts);
    glEnableVertexAttribArray((GLuint)R.i_uv);
    glVertexAttribPointer((GLuint)R.i_uv, 2, GL_FLOAT, GL_FALSE,
                          6 * sizeof(float), verts + 2);
    glEnableVertexAttribArray((GLuint)R.i_local);
    glVertexAttribPointer((GLuint)R.i_local, 2, GL_FLOAT, GL_FALSE,
                          6 * sizeof(float), verts + 4);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glDisableVertexAttribArray((GLuint)R.i_pos);
    glDisableVertexAttribArray((GLuint)R.i_uv);
    glDisableVertexAttribArray((GLuint)R.i_local);
}

void render_image(const char *name, Rect dst, Color tint, float radius)
{
    render_image_sub(name, dst, 0.0f, 0.0f, 1.0f, 1.0f, tint, radius);
}

void render_image_cover(const char *name, Rect dst, Color tint, float radius)
{
    ImageSlot *slot;
    float src_aspect, dst_aspect, half;

    if (dst.w <= 0.0f || dst.h <= 0.0f) return;

    slot = image_slot(name, dst.w);
    if (!slot || !slot->tex || slot->height <= 0) return;

    src_aspect = (float)slot->width / (float)slot->height;
    dst_aspect = dst.w / dst.h;

    if (dst_aspect > src_aspect) {
        /* Destination is wider: keep the full width, crop top and bottom. */
        half = (src_aspect / dst_aspect) * 0.5f;
        render_image_sub(name, dst, 0.0f, 0.5f - half, 1.0f, 0.5f + half,
                         tint, radius);
    } else {
        half = (dst_aspect / src_aspect) * 0.5f;
        render_image_sub(name, dst, 0.5f - half, 0.0f, 0.5f + half, 1.0f,
                         tint, radius);
    }
}
