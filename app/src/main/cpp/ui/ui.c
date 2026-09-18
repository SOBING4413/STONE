#include "ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "../core/calc.h"
#include "../core/achievements.h"
#include "../platform/window.h"

static UiState U;
static StoneTheme g_theme;
static int g_theme_dark = -1;

/* Total time the launch splash stays on screen, in seconds; the last and
   first FADE_EDGE seconds of that are used for the fade in/out. */
#define SPLASH_DURATION 1.35f
#define SPLASH_FADE_EDGE 0.32f

UiState *ui_state(void) { return &U; }

/* ------------------------------------------------------------------ theme */

void theme_refresh(int dark)
{
    if (g_theme_dark == dark) return;
    g_theme_dark = dark;

    if (dark) {
        g_theme.bg          = color_hex(0x0E1116, 1.0f);
        g_theme.surface     = color_hex(0x171C24, 1.0f);
        g_theme.surface_alt = color_hex(0x1F2630, 1.0f);
        g_theme.border      = color_hex(0x2B3340, 1.0f);
        g_theme.primary     = color_hex(0x27D9A3, 1.0f);
        g_theme.primary_soft= color_hex(0x14584A, 1.0f);
        g_theme.accent      = color_hex(0xF98A3C, 1.0f);
        g_theme.warn        = color_hex(0xFACC15, 1.0f);
        g_theme.danger      = color_hex(0xF2555A, 1.0f);
        g_theme.text        = color_hex(0xF3F6FA, 1.0f);
        g_theme.text_dim    = color_hex(0x9AA6B6, 1.0f);
        g_theme.text_faint  = color_hex(0x6A7686, 1.0f);
        g_theme.nav_bg      = color_hex(0x121721, 1.0f);
        g_theme.shadow      = color_rgba(0.0f, 0.0f, 0.0f, 0.35f);
        g_theme.overlay     = color_rgba(0.0f, 0.0f, 0.0f, 0.62f);
    } else {
        g_theme.bg          = color_hex(0xF4F6FA, 1.0f);
        g_theme.surface     = color_hex(0xFFFFFF, 1.0f);
        g_theme.surface_alt = color_hex(0xEDF1F7, 1.0f);
        g_theme.border      = color_hex(0xD9E0EA, 1.0f);
        g_theme.primary     = color_hex(0x0FA97F, 1.0f);
        g_theme.primary_soft= color_hex(0xCCF0E4, 1.0f);
        g_theme.accent      = color_hex(0xE2702A, 1.0f);
        g_theme.warn        = color_hex(0xC98A06, 1.0f);
        g_theme.danger      = color_hex(0xD93B41, 1.0f);
        g_theme.text        = color_hex(0x121820, 1.0f);
        g_theme.text_dim    = color_hex(0x5A6675, 1.0f);
        g_theme.text_faint  = color_hex(0x8A94A3, 1.0f);
        g_theme.nav_bg      = color_hex(0xFFFFFF, 1.0f);
        g_theme.shadow      = color_rgba(0.1f, 0.15f, 0.25f, 0.12f);
        g_theme.overlay     = color_rgba(0.1f, 0.12f, 0.16f, 0.45f);
    }
}

const StoneTheme *theme(void) { return &g_theme; }

/* ----------------------------------------------------------------- layout */

float ui_dp(float v) { return v * U.scale; }

Rect ui_row(Rect *cursor, float height, float gap)
{
    Rect r = rect_make(cursor->x, cursor->y, cursor->w, height);
    cursor->y += height + gap;
    cursor->h -= height + gap;
    return r;
}

Rect ui_split_left(Rect r, float w, float gap)
{
    (void)gap;
    r.w = w;
    return r;
}

Rect ui_split_right(Rect r, float w, float gap)
{
    Rect o = r;
    o.x = r.x + r.w - w;
    o.w = w;
    (void)gap;
    return o;
}

/* ------------------------------------------------------------------ input */

void ui_pointer_down(float x, float y)
{
    U.input.x = U.input.down_x = x;
    U.input.y = U.input.down_y = y;
    U.input.down = 1;
    U.input.pressed = 1;
    U.input.dragged = 0;
    U.input.hold_time = 0.0f;
    U.input.move_dx = U.input.move_dy = 0.0f;
}

void ui_pointer_move(float x, float y)
{
    U.input.move_dx += x - U.input.x;
    U.input.move_dy += y - U.input.y;
    U.input.x = x;
    U.input.y = y;
    if (fabsf(x - U.input.down_x) > ui_dp(8.0f) ||
        fabsf(y - U.input.down_y) > ui_dp(8.0f))
        U.input.dragged = 1;
}

void ui_pointer_up(float x, float y)
{
    U.input.x = x;
    U.input.y = y;
    U.input.down = 0;
    U.input.released = 1;
}

void ui_pointer_cancel(void)
{
    U.input.down = 0;
    U.input.released = 0;
    U.input.dragged = 1;
    U.active_id = 0;
}

/* --------------------------------------------------------------- widgets */

static int hit(Rect r) { return rect_contains(r, U.input.x, U.input.y); }

int ui_touch_area(int id, Rect r)
{
    int clicked = 0;
    if (U.keyboard.active) return 0;

    if (U.input.pressed && hit(r)) U.active_id = id;
    if (U.input.released && U.active_id == id) {
        if (hit(r) && !U.input.dragged) clicked = 1;
        U.active_id = 0;
    }
    return clicked;
}

static int is_pressed(int id, Rect r)
{
    return U.active_id == id && U.input.down && hit(r) && !U.input.dragged;
}

void ui_card(Rect r)
{
    const StoneTheme *t = theme();
    render_rect(r, t->surface, ui_dp(18.0f));
    render_rect_outline(r, t->border, ui_dp(18.0f), 1.0f);
}

void ui_card_shadowed(Rect r)
{
    const StoneTheme *t = theme();
    Rect s = r;
    s.y += ui_dp(3.0f);
    render_rect(s, t->shadow, ui_dp(18.0f));
    ui_card(r);
}

void ui_section_title(Rect *cursor, const char *title)
{
    const StoneTheme *t = theme();
    Rect r = ui_row(cursor, ui_dp(26.0f), ui_dp(6.0f));
    render_text(title, r.x, r.y, ui_dp(13.0f), t->text_faint, 1);
}

static int button_base(int id, Rect r, const char *label, Color bg, Color fg,
                       Color border, float border_w)
{
    int clicked = ui_touch_area(id, r);
    float radius = ui_dp(14.0f);
    Rect draw = r;

    if (is_pressed(id, r)) {
        draw = rect_inset(r, ui_dp(1.5f), ui_dp(1.5f));
        bg = color_alpha(bg, 0.82f);
    }
    if (bg.a > 0.0f) render_rect(draw, bg, radius);
    if (border_w > 0.0f) render_rect_outline(draw, border, radius, border_w);
    render_text_aligned(label, draw, ui_dp(15.0f), fg, 1, TEXT_CENTER);
    return clicked;
}

static int button_gradient(int id, Rect r, const char *label, Color top, Color bottom, Color fg)
{
    int clicked = ui_touch_area(id, r);
    float radius = ui_dp(14.0f);
    Rect draw = r;

    if (is_pressed(id, r)) {
        draw = rect_inset(r, ui_dp(1.5f), ui_dp(1.5f));
        top = color_alpha(top, 0.86f);
        bottom = color_alpha(bottom, 0.86f);
    }
    render_rect_gradient(draw, top, bottom, radius);
    render_text_aligned(label, draw, ui_dp(15.0f), fg, 1, TEXT_CENTER);
    return clicked;
}

int ui_button(int id, Rect r, const char *label, int primary)
{
    const StoneTheme *t = theme();
    if (primary) {
        /* A subtle top-to-bottom gradient instead of a flat fill reads as
           more modern and gives primary actions a touch more depth. */
        Color top = color_mix(t->primary, color_hex(0xFFFFFF, 1.0f), 0.16f);
        return button_gradient(id, r, label, top, t->primary, color_hex(0x06231C, 1.0f));
    }
    return button_base(id, r, label, t->surface_alt, t->text, t->border, 1.0f);
}

int ui_ghost_button(int id, Rect r, const char *label)
{
    const StoneTheme *t = theme();
    return button_base(id, r, label, color_rgba(0, 0, 0, 0), t->text_dim, t->border, 1.0f);
}

int ui_danger_button(int id, Rect r, const char *label)
{
    const StoneTheme *t = theme();
    return button_base(id, r, label, color_alpha(t->danger, 0.14f), t->danger, t->danger, 1.0f);
}

int ui_chip(int id, Rect r, const char *label, int selected)
{
    const StoneTheme *t = theme();
    Color bg = selected ? t->primary : t->surface_alt;
    Color fg = selected ? color_hex(0x06231C, 1.0f) : t->text_dim;
    int clicked = ui_touch_area(id, r);

    render_rect(r, bg, r.h * 0.5f);
    if (!selected) render_rect_outline(r, t->border, r.h * 0.5f, 1.0f);
    render_text_aligned(label, r, ui_dp(12.5f), fg, selected, TEXT_CENTER);
    return clicked;
}

void ui_progress_bar(Rect r, float t, Color fill)
{
    const StoneTheme *th = theme();
    Rect inner = r;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    render_rect(r, th->surface_alt, r.h * 0.5f);
    inner.w = r.w * t;
    if (inner.w > 2.0f)
        render_rect_gradient(inner, color_alpha(fill, 0.85f), fill, r.h * 0.5f);
}

void ui_stat_tile(Rect r, const char *label, const char *value, const char *sub, Color accent)
{
    const StoneTheme *t = theme();
    float pad = ui_dp(12.0f);

    render_rect(r, t->surface_alt, ui_dp(16.0f));
    render_rect(rect_make(r.x, r.y + pad, ui_dp(3.0f), r.h - pad * 2.0f), accent, ui_dp(2.0f));
    render_text(label, r.x + pad, r.y + pad * 0.75f, ui_dp(11.0f), t->text_faint, 0);
    render_text(value, r.x + pad, r.y + pad * 0.75f + ui_dp(16.0f), ui_dp(21.0f), t->text, 1);
    if (sub && sub[0])
        render_text(sub, r.x + pad, r.y + r.h - pad - ui_dp(11.0f), ui_dp(11.0f), t->text_dim, 0);
}

void ui_ring_stat(float cx, float cy, float radius, float t, const char *value,
                  const char *label, Color c)
{
    const StoneTheme *th = theme();
    float thickness = ui_dp(9.0f);
    float glow = 0.5f + 0.5f * sinf(U.time * 1.8f);

    render_ring(cx, cy, radius, thickness, th->surface_alt);
    /* Soft breathing glow behind the arc keeps the ring feeling alive even
       when the underlying stat is not currently changing. */
    render_ring(cx, cy, radius + ui_dp(2.0f), ui_dp(2.0f), color_alpha(c, 0.10f + 0.10f * glow));
    render_arc(cx, cy, radius, thickness, t, c);
    if (value) {
        float w = render_text_width(value, ui_dp(24.0f), 1);
        render_text(value, cx - w * 0.5f, cy - ui_dp(20.0f), ui_dp(24.0f), th->text, 1);
    }
    if (label) {
        float w = render_text_width(label, ui_dp(11.0f), 0);
        render_text(label, cx - w * 0.5f, cy + ui_dp(6.0f), ui_dp(11.0f), th->text_dim, 0);
    }
}

void ui_kv_row(Rect r, const char *key, const char *value)
{
    const StoneTheme *t = theme();
    render_text(key, r.x, r.y + (r.h - render_line_height(ui_dp(13.0f))) * 0.5f,
                ui_dp(13.0f), t->text_dim, 0);
    render_text_aligned(value, r, ui_dp(13.5f), t->text, 1, TEXT_RIGHT);
}

void ui_empty_state(Rect r, const char *title, const char *hint)
{
    const StoneTheme *t = theme();
    Rect box = r;
    render_rect(r, t->surface_alt, ui_dp(16.0f));
    box.y = r.y + r.h * 0.5f - ui_dp(22.0f);
    box.h = ui_dp(22.0f);
    render_text_aligned(title, box, ui_dp(14.0f), t->text_dim, 1, TEXT_CENTER);
    box.y += ui_dp(22.0f);
    render_text_aligned(hint, box, ui_dp(12.0f), t->text_faint, 0, TEXT_CENTER);
}

void ui_disclaimer(Rect *cursor, float width)
{
    const StoneTheme *t = theme();
    const char *msg = "All numbers here are general fitness estimates, not a "
                      "medical diagnosis or medical advice.";
    Rect box = rect_make(cursor->x + ui_dp(14.0f), cursor->y + ui_dp(12.0f),
                         width - ui_dp(28.0f), 0.0f);
    float h = render_text_wrapped(msg, box, ui_dp(11.5f), t->text_faint, 0, 0);
    Rect card = rect_make(cursor->x, cursor->y, width, h + ui_dp(24.0f));

    render_rect(card, color_alpha(t->warn, 0.10f), ui_dp(14.0f));
    render_rect_outline(card, color_alpha(t->warn, 0.35f), ui_dp(14.0f), 1.0f);
    render_text_wrapped(msg, box, ui_dp(11.5f), t->text_dim, 0, 1);

    cursor->y += card.h + ui_dp(12.0f);
    cursor->h -= card.h + ui_dp(12.0f);
}

void ui_toast(void)
{
    StoneApp *app = stone_app();
    const StoneTheme *t = theme();
    float a, w;
    Rect r;

    if (app->status_timer <= 0.0f || !app->status[0]) return;
    a = app->status_timer > 0.4f ? 1.0f : app->status_timer / 0.4f;
    w = render_text_width(app->status, ui_dp(13.0f), 0) + ui_dp(32.0f);
    if (w > (float)render_width() - ui_dp(32.0f)) w = (float)render_width() - ui_dp(32.0f);

    r = rect_make(((float)render_width() - w) * 0.5f,
                  U.safe.y + U.safe.h - ui_dp(74.0f), w, ui_dp(40.0f));
    render_rect(r, color_alpha(color_hex(0x0B0F14, 1.0f), 0.92f * a), ui_dp(12.0f));
    render_rect_outline(r, color_alpha(t->primary, 0.5f * a), ui_dp(12.0f), 1.0f);
    render_text_aligned(app->status, r, ui_dp(13.0f), color_alpha(t->text, a), 0, TEXT_CENTER);
}

/* -------------------------------------------------------------- scrolling */

static UiScroll *g_active_scroll = NULL;

Rect ui_scroll_begin(StonePage page, Rect area, float content_h)
{
    UiScroll *s = &U.scroll[page];
    float max_off;

    /* `content_h` is the caller's estimate; once the page has measured its
       real height (ui_scroll_set_content) that value wins. */
    if (s->content_h <= 1.0f) s->content_h = content_h;
    content_h = s->content_h;
    s->view_h = area.h;
    max_off = content_h - area.h;
    if (max_off < 0.0f) max_off = 0.0f;

    if (!U.keyboard.active) {
        /* Any button, card or list row under the finger claims active_id on
           press (see ui_touch_area), before it is known whether the touch
           will turn into a tap or a scroll. Requiring active_id == 0 here
           meant a swipe that started on top of one of those widgets could
           never be recognised as a scroll - only swipes starting on truly
           empty space worked, which is exactly the "sometimes I can scroll,
           sometimes I can't" bug. Once the move has crossed the drag
           threshold inside this area, always take over as a scroll and
           cancel whatever widget was pressed underneath it. */
        if (U.input.down && U.input.dragged && hit(area)) {
            s->grabbing = 1;
            s->offset -= U.input.move_dy;
            s->velocity = -U.input.move_dy / (U.dt > 0.0001f ? U.dt : 0.016f);
            U.active_id = 0;                    /* cancel any pressed widget */
        }
        if (!U.input.down) s->grabbing = 0;
    }

    if (!s->grabbing) {
        s->offset += s->velocity * U.dt;
        s->velocity *= 0.90f;
        if (fabsf(s->velocity) < 6.0f) s->velocity = 0.0f;
    }

    /* Rubber-band back into range. */
    if (s->offset < 0.0f) {
        s->offset += (0.0f - s->offset) * (s->grabbing ? 0.5f : 0.25f);
        if (fabsf(s->offset) < 0.5f) s->offset = 0.0f;
        s->velocity = 0.0f;
    } else if (s->offset > max_off) {
        s->offset += (max_off - s->offset) * (s->grabbing ? 0.5f : 0.25f);
        if (fabsf(s->offset - max_off) < 0.5f) s->offset = max_off;
        s->velocity = 0.0f;
    }

    render_push_clip(area);
    g_active_scroll = s;

    /* Scrollbar hint. */
    if (max_off > 1.0f) {
        const StoneTheme *t = theme();
        float frac = area.h / content_h;
        float bar_h = area.h * frac;
        float bar_y = area.y + (area.h - bar_h) * (s->offset / max_off);
        if (bar_h < ui_dp(28.0f)) bar_h = ui_dp(28.0f);
        render_rect(rect_make(area.x + area.w - ui_dp(4.0f), bar_y, ui_dp(3.0f), bar_h),
                    color_alpha(t->text_faint, 0.35f), ui_dp(1.5f));
    }

    return rect_make(area.x, area.y - s->offset, area.w, content_h);
}

void ui_scroll_set_content(StonePage page, float height)
{
    if (page < 0 || page >= PAGE_COUNT) return;
    if (height > 1.0f) U.scroll[page].content_h = height;
}

void ui_scroll_end(void)
{
    render_pop_clip();
    g_active_scroll = NULL;
}

int ui_scroll_is_dragging(void)
{
    return g_active_scroll ? g_active_scroll->grabbing : 0;
}

/* -------------------------------------------------------------- keyboard */

static char g_kb_commit[64];
static int  g_kb_commit_ready = 0;
static int  g_kb_commit_kind = 0;
static void *g_kb_commit_target = NULL;

void ui_keyboard_open(KeyboardMode mode, const char *title, const char *initial,
                      int target_kind, void *target)
{
    U.keyboard.mode = mode;
    U.keyboard.active = 1;
    U.keyboard.shift = 1;
    U.keyboard.symbols = 0;
    U.keyboard.anim = 0.0f;
    U.keyboard.just_opened = 1;
    U.keyboard.target_kind = target_kind;
    U.keyboard.target = target;
    stone_strlcpy(U.keyboard.title, title ? title : "Input", sizeof(U.keyboard.title));
    stone_strlcpy(U.keyboard.buffer, initial ? initial : "", sizeof(U.keyboard.buffer));
    U.active_id = 0;

    /* Swallow the rest of the tap that opened us. The field was hit on
       pointer-up, and the keyboard panel is drawn later in this very frame -
       so without this the panel's scrim (and, on a tall screen, whichever key
       happens to sit under the finger) would see the same release event and
       act on it. One tap is one action; this is where that is enforced. */
    U.input.pressed = 0;
    U.input.released = 0;
}

int ui_keyboard_active(void) { return U.keyboard.active; }

int ui_keyboard_take(int *out_kind, void **out_target, char *out_text, int out_len)
{
    if (!g_kb_commit_ready) return 0;
    g_kb_commit_ready = 0;
    if (out_kind) *out_kind = g_kb_commit_kind;
    if (out_target) *out_target = g_kb_commit_target;
    if (out_text) stone_strlcpy(out_text, g_kb_commit, (size_t)out_len);
    return 1;
}

static void kb_append(char c)
{
    size_t n = strlen(U.keyboard.buffer);
    if (n + 1 >= sizeof(U.keyboard.buffer)) return;
    if (c == '.' && strchr(U.keyboard.buffer, '.')) return;
    U.keyboard.buffer[n] = c;
    U.keyboard.buffer[n + 1] = '\0';
}

static void kb_backspace(void)
{
    size_t n = strlen(U.keyboard.buffer);
    if (n > 0) U.keyboard.buffer[n - 1] = '\0';
}

static int kb_key(int id, Rect r, const char *label, int accent)
{
    const StoneTheme *t = theme();
    int clicked = 0;
    Color bg = accent ? t->primary : t->surface_alt;
    Color fg = accent ? color_hex(0x06231C, 1.0f) : t->text;
    float size = ui_dp(16.0f);

    /* Keys bypass ui_touch_area's keyboard guard, but never react on the
       frame the panel opened in (see ui_keyboard_open). */
    if (!U.keyboard.just_opened) {
        if (U.input.pressed && hit(r)) U.active_id = id;
        if (U.input.released && U.active_id == id) {
            if (hit(r) && !U.input.dragged) clicked = 1;
            U.active_id = 0;
        }
        if (U.active_id == id && U.input.down && hit(r)) bg = color_alpha(bg, 0.75f);
    }

    render_rect(r, bg, ui_dp(10.0f));
    render_rect_outline(r, color_alpha(t->border, 0.7f), ui_dp(10.0f), 1.0f);
    /* Word labels ("space", "clear") need to shrink or they clip the key. */
    if (render_text_width(label, size, 1) > r.w - ui_dp(8.0f)) size = ui_dp(12.0f);
    render_text_aligned(label, r, size, fg, 1, TEXT_CENTER);
    return clicked;
}

void ui_keyboard_draw(void)
{
    const StoneTheme *t = theme();
    /* A digits row sits above the letters at all times: names, notes and
       search terms routinely contain numbers, and a keyboard that cannot type
       "Protein 90" is a keyboard that is in the way. */
    static const char *rows_text[4] = {"1234567890", "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"};
    float w = (float)render_width();
    float h = (float)render_height();
    float bottom = h - U.inset_bottom;
    Rect panel, field, key;
    float pad = ui_dp(8.0f);
    float panel_h = ui_dp(360.0f);
    int numeric = (U.keyboard.mode != KB_TEXT);
    int interactive = !U.keyboard.just_opened;
    float top, gw, kh;
    int i, r;

    if (!U.keyboard.active) return;

    U.keyboard.anim += (1.0f - U.keyboard.anim) * 0.35f;
    if (U.keyboard.anim > 0.999f) U.keyboard.anim = 1.0f;

    panel = rect_make(0.0f, bottom - panel_h * U.keyboard.anim, w,
                      panel_h + U.inset_bottom + ui_dp(4.0f));

    /* Scrim: tapping above the panel cancels. Never on the opening frame. */
    render_rect(rect_make(0, 0, w, h), color_alpha(t->overlay, U.keyboard.anim), 0.0f);
    if (interactive && U.input.released && !U.input.dragged &&
        U.input.y < panel.y) {
        U.keyboard.active = 0;
        U.active_id = 0;
        return;
    }

    render_rect(panel, t->nav_bg, ui_dp(22.0f));
    render_rect_outline(panel, t->border, ui_dp(22.0f), 1.0f);

    render_text(U.keyboard.title, panel.x + ui_dp(18.0f), panel.y + ui_dp(13.0f),
                ui_dp(13.0f), t->text_dim, 1);

    field = rect_make(panel.x + ui_dp(16.0f), panel.y + ui_dp(34.0f),
                      panel.w - ui_dp(32.0f), ui_dp(44.0f));
    render_rect(field, t->surface_alt, ui_dp(12.0f));
    render_rect_outline(field, color_alpha(t->primary, 0.6f), ui_dp(12.0f), 1.5f);
    {
        char shown[80];
        int empty = (U.keyboard.buffer[0] == '\0');
        snprintf(shown, sizeof(shown), "%s%s", U.keyboard.buffer,
                 (fmodf(U.time, 1.0f) < 0.5f) ? "|" : "");
        render_text(empty && fmodf(U.time, 1.0f) >= 0.5f ? "Type a value" : shown,
                    field.x + ui_dp(14.0f),
                    field.y + (field.h - render_line_height(ui_dp(17.0f))) * 0.5f,
                    ui_dp(17.0f), empty ? t->text_faint : t->text, 0);
    }

    top = field.y + field.h + ui_dp(12.0f);

    if (numeric) {
        static const char *pad_keys[12] = {"1","2","3","4","5","6","7","8","9",".","0","<"};
        kh = ui_dp(42.0f);
        gw = (panel.w - ui_dp(32.0f) - pad * 2.0f) / 3.0f;
        for (i = 0; i < 12; ++i) {
            int col = i % 3, row = i / 3;
            const char *lbl = pad_keys[i];
            /* An integer field must not be able to produce "1.5". */
            if (lbl[0] == '.' && U.keyboard.mode == KB_NUMERIC) continue;
            key = rect_make(panel.x + ui_dp(16.0f) + (gw + pad) * (float)col,
                            top + (kh + ui_dp(7.0f)) * (float)row, gw, kh);
            if (kb_key(3000 + i, key, lbl, 0)) {
                if (lbl[0] == '<') kb_backspace();
                else kb_append(lbl[0]);
            }
        }
    } else {
        kh = ui_dp(36.0f);
        for (r = 0; r < 4; ++r) {
            int n = (int)strlen(rows_text[r]);
            float cell = (panel.w - ui_dp(20.0f) - ui_dp(4.0f) * 9.0f) / 10.0f;
            float row_w = (float)n * cell + ui_dp(4.0f) * (float)(n - 1);
            float x0 = (panel.w - row_w) * 0.5f;
            for (i = 0; i < n; ++i) {
                char lbl[2];
                char c = rows_text[r][i];
                lbl[0] = (r == 0 || U.keyboard.shift) ? c
                                                      : (char)(c - 'A' + 'a');
                lbl[1] = '\0';
                key = rect_make(x0 + (cell + ui_dp(4.0f)) * (float)i,
                                top + (kh + ui_dp(5.0f)) * (float)r, cell, kh);
                if (kb_key(3100 + r * 20 + i, key, lbl, 0)) kb_append(lbl[0]);
            }
        }
        {
            float y = top + (kh + ui_dp(5.0f)) * 4.0f;
            float bw = (panel.w - ui_dp(32.0f) - pad * 2.0f) / 3.0f;
            if (kb_key(3200, rect_make(panel.x + ui_dp(16.0f), y, bw, kh),
                       U.keyboard.shift ? "ABC" : "abc", 0))
                U.keyboard.shift = !U.keyboard.shift;
            if (kb_key(3201, rect_make(panel.x + ui_dp(16.0f) + bw + pad, y, bw, kh),
                       "space", 0))
                kb_append(' ');
            if (kb_key(3202, rect_make(panel.x + ui_dp(16.0f) + (bw + pad) * 2.0f, y,
                                       bw, kh), "<", 0))
                kb_backspace();
        }
    }

    /* Clear / Cancel / Save */
    {
        float bw = (panel.w - ui_dp(32.0f) - pad * 2.0f) / 3.0f;
        float by = panel.y + panel_h - ui_dp(56.0f);
        float bh = ui_dp(44.0f);

        if (kb_key(3299, rect_make(panel.x + ui_dp(16.0f), by, bw, bh), "clear", 0)) {
            U.keyboard.buffer[0] = '\0';
        }
        if (kb_key(3300, rect_make(panel.x + ui_dp(16.0f) + bw + pad, by, bw, bh),
                   "Cancel", 0)) {
            U.keyboard.active = 0;
            U.active_id = 0;
            return;
        }
        if (kb_key(3301, rect_make(panel.x + ui_dp(16.0f) + (bw + pad) * 2.0f, by,
                                   bw, bh), "Save", 1)) {
            stone_strlcpy(g_kb_commit, U.keyboard.buffer, sizeof(g_kb_commit));
            g_kb_commit_kind = U.keyboard.target_kind;
            g_kb_commit_target = U.keyboard.target;
            g_kb_commit_ready = 1;
            U.keyboard.active = 0;
            U.active_id = 0;
        }
    }
}

/* ------------------------------------------------------------- navigation */

void ui_navigate(StonePage page)
{
    if (page < 0 || page >= PAGE_COUNT || page == U.page) return;
    if (U.nav_depth < UI_NAV_DEPTH) U.nav_stack[U.nav_depth++] = U.page;
    U.page = page;
    U.page_anim = 0.0f;
    U.input.dragged = 1;      /* swallow the tap that caused the navigation */
    U.active_id = 0;
}

int ui_back(void)
{
    if (U.keyboard.active) { U.keyboard.active = 0; return 1; }
    if (U.page == PAGE_ONBOARDING) {
        /* Back walks the wizard, and on the first card it does nothing rather
           than dropping a brand new user out of the app mid-setup. */
        if (U.onboard_step > 0) { U.onboard_step--; U.onboard_anim = 0.0f; }
        return 1;
    }
    if (U.session_active && U.page == PAGE_WORKOUT_SESSION) {
        U.session_active = 0;
        U.page = PAGE_WORKOUT_DETAIL;
        U.page_anim = 0.0f;
        return 1;
    }
    if (U.nav_depth > 0) {
        U.page = U.nav_stack[--U.nav_depth];
        U.page_anim = 0.0f;
        return 1;
    }
    if (U.page != PAGE_DASHBOARD) {
        U.page = PAGE_DASHBOARD;
        U.page_anim = 0.0f;
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ chrome */

static void draw_nav_icon(Rect r, int index, Color c)
{
    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;
    float s = ui_dp(9.0f);

    switch (index) {
    case PAGE_DASHBOARD:  /* four squares */
        render_rect(rect_make(cx - s, cy - s, s * 0.85f, s * 0.85f), c, ui_dp(2.0f));
        render_rect(rect_make(cx + s * 0.15f, cy - s, s * 0.85f, s * 0.85f), c, ui_dp(2.0f));
        render_rect(rect_make(cx - s, cy + s * 0.15f, s * 0.85f, s * 0.85f), c, ui_dp(2.0f));
        render_rect(rect_make(cx + s * 0.15f, cy + s * 0.15f, s * 0.85f, s * 0.85f), c, ui_dp(2.0f));
        break;
    case PAGE_DIET:       /* plate */
        render_ring(cx, cy, s, ui_dp(2.0f), c);
        render_circle(cx, cy, s * 0.35f, c);
        break;
    case PAGE_WORKOUT:    /* dumbbell */
        render_rect(rect_make(cx - s, cy - s * 0.55f, s * 0.35f, s * 1.1f), c, ui_dp(2.0f));
        render_rect(rect_make(cx + s * 0.65f, cy - s * 0.55f, s * 0.35f, s * 1.1f), c, ui_dp(2.0f));
        render_rect(rect_make(cx - s * 0.6f, cy - s * 0.15f, s * 1.2f, s * 0.3f), c, ui_dp(1.0f));
        break;
    case PAGE_PROGRESS:   /* bars */
        render_rect(rect_make(cx - s, cy + s * 0.1f, s * 0.45f, s * 0.8f), c, ui_dp(2.0f));
        render_rect(rect_make(cx - s * 0.25f, cy - s * 0.4f, s * 0.45f, s * 1.3f), c, ui_dp(2.0f));
        render_rect(rect_make(cx + s * 0.5f, cy - s, s * 0.45f, s * 1.9f), c, ui_dp(2.0f));
        break;
    default:              /* person */
        render_circle(cx, cy - s * 0.45f, s * 0.4f, c);
        render_rect(rect_make(cx - s * 0.65f, cy + s * 0.1f, s * 1.3f, s * 0.8f), c, ui_dp(5.0f));
        break;
    }
}

static int nav_active_tab(void)
{
    if (U.page == PAGE_WORKOUT_DETAIL || U.page == PAGE_WORKOUT_SESSION) return PAGE_WORKOUT;
    if (U.page == PAGE_ADD_FOOD) return PAGE_DIET;
    if (U.page == PAGE_ABOUT || U.page == PAGE_HISTORY ||
        U.page == PAGE_ACHIEVEMENTS) return PAGE_PROFILE;
    if ((int)U.page < UI_NAV_TABS) return (int)U.page;
    return PAGE_DASHBOARD;
}

static void draw_nav_bar(Rect bar)
{
    const StoneTheme *t = theme();
    static const char *labels[UI_NAV_TABS] = {"Home", "Diet", "Workout", "Progress", "Profile"};
    float tab_w = bar.w / (float)UI_NAV_TABS;
    float target_x = bar.x + tab_w * ((float)nav_active_tab() + 0.5f);
    float pill_w = tab_w * 0.64f;
    Rect pill;
    int i;

    render_rect(rect_make(bar.x, bar.y - ui_dp(1.0f), bar.w, ui_dp(1.0f)), t->border, 0.0f);
    render_rect(bar, t->nav_bg, 0.0f);

    /* When the system navigation bar is on screen after all, the strip it
       occupies is painted in the same colour and the tab bar sits above it,
       so the two never overlap and the result still reads as one surface
       rather than a gap. */
    if (U.inset_bottom > 0.5f)
        render_rect(rect_make(0.0f, bar.y + bar.h, (float)render_width(),
                              U.inset_bottom + 2.0f), t->nav_bg, 0.0f);

    /* The highlight glides between tabs instead of jumping, so switching
       tabs reads as one continuous motion rather than five separate pages. */
    if (U.nav_indicator_x < 0.0f) U.nav_indicator_x = target_x;
    else U.nav_indicator_x += (target_x - U.nav_indicator_x) * 0.28f;

    pill = rect_make(U.nav_indicator_x - pill_w * 0.5f, bar.y + ui_dp(5.0f),
                     pill_w, ui_dp(32.0f));
    render_rect(pill, color_alpha(t->primary, 0.13f), ui_dp(16.0f));
    render_rect(rect_make(pill.x + pill.w * 0.5f - ui_dp(13.0f), bar.y + ui_dp(2.0f),
                          ui_dp(26.0f), ui_dp(3.0f)), t->primary, ui_dp(2.0f));

    for (i = 0; i < UI_NAV_TABS; ++i) {
        Rect tab = rect_make(bar.x + tab_w * (float)i, bar.y, tab_w, bar.h);
        int active = (i == nav_active_tab());
        Color c = active ? t->primary : t->text_faint;

        if (ui_touch_area(900 + i, tab)) {
            U.nav_depth = 0;
            U.page = (StonePage)i;
            U.page_anim = 0.0f;
        }

        draw_nav_icon(rect_make(tab.x, tab.y + ui_dp(8.0f), tab.w, ui_dp(26.0f)), i, c);
        render_text_aligned(labels[i],
                            rect_make(tab.x, tab.y + ui_dp(34.0f), tab.w, ui_dp(16.0f)),
                            ui_dp(10.5f), c, active, TEXT_CENTER);
    }
}

static const char *page_title(StonePage p)
{
    switch (p) {
    case PAGE_DASHBOARD:       return "Dashboard";
    case PAGE_DIET:            return "Diet";
    case PAGE_WORKOUT:         return "Workout";
    case PAGE_PROGRESS:        return "Progress";
    case PAGE_PROFILE:         return "Profile";
    case PAGE_HISTORY:         return "History";
    case PAGE_WORKOUT_DETAIL:  return "Program";
    case PAGE_WORKOUT_SESSION: return "Session";
    case PAGE_ADD_FOOD:        return "Add Food";
    case PAGE_ABOUT:           return "About";
    case PAGE_ACHIEVEMENTS:    return "Achievements";
    case PAGE_ONBOARDING:      return "Welcome";
    default:                   return STONE_APP_NAME;
    }
}

static void draw_header(Rect header)
{
    const StoneTheme *t = theme();
    StoneApp *app = stone_app();
    int has_back = (U.nav_depth > 0) || (U.page >= PAGE_HISTORY);
    float x = header.x + ui_dp(18.0f);

    if (has_back) {
        Rect b = rect_make(header.x + ui_dp(10.0f), header.y + ui_dp(6.0f),
                           ui_dp(38.0f), ui_dp(38.0f));
        if (ui_touch_area(901, b)) ui_back();
        render_rect(b, t->surface_alt, ui_dp(12.0f));
        {
            float cx = b.x + b.w * 0.5f, cy = b.y + b.h * 0.5f, s = ui_dp(5.0f);
            render_line(cx + s * 0.4f, cy - s, cx - s * 0.5f, cy, ui_dp(2.0f), t->text);
            render_line(cx - s * 0.5f, cy, cx + s * 0.4f, cy + s, ui_dp(2.0f), t->text);
        }
        x = b.x + b.w + ui_dp(12.0f);
    }

    render_text(page_title(U.page), x, header.y + ui_dp(8.0f), ui_dp(20.0f), t->text, 1);
    if (U.page == PAGE_DASHBOARD) {
        char sub[64];
        snprintf(sub, sizeof(sub), "%s  -  %s", stone_weekday(app->today), app->today);
        render_text(sub, x, header.y + ui_dp(31.0f), ui_dp(11.5f), t->text_faint, 0);
    } else {
        render_text(STONE_APP_NAME, x, header.y + ui_dp(31.0f), ui_dp(11.5f), t->text_faint, 0);
    }

    if (stone_app()->storage_warning) {
        Rect w = rect_make(header.x + header.w - ui_dp(120.0f), header.y + ui_dp(12.0f),
                           ui_dp(106.0f), ui_dp(26.0f));
        render_rect(w, color_alpha(t->danger, 0.15f), ui_dp(8.0f));
        render_text_aligned("storage error", w, ui_dp(10.5f), t->danger, 1, TEXT_CENTER);
    } else {
        /* Real-time clock, always live: read fresh off the system clock every
           frame so it ticks in place instead of being a static timestamp. */
        time_t raw = time(NULL);
        struct tm *lt = localtime(&raw);
        char clock_str[12];
        Rect w = rect_make(header.x + header.w - ui_dp(94.0f), header.y + ui_dp(9.0f),
                           ui_dp(80.0f), ui_dp(30.0f));

        if (lt) strftime(clock_str, sizeof(clock_str), "%H:%M:%S", lt);
        else    memcpy(clock_str, "--:--:--", 9);

        render_rect(w, t->surface_alt, ui_dp(11.0f));
        render_rect_outline(w, t->border, ui_dp(11.0f), 1.0f);
        render_text_aligned(clock_str, w, ui_dp(12.5f), t->text, 1, TEXT_CENTER);
    }
}

/* ------------------------------------------------------------------ frame */

void ui_init(void)
{
    memset(&U, 0, sizeof(U));
    U.page = PAGE_DASHBOARD;
    U.scale = 1.0f;
    U.selected_level = -1;
    U.selected_category = -1;
    U.selected_workout = 1;
    U.food_portions = 1.0f;
    U.selected_meal = MEAL_BREAKFAST;
    U.selected_food = -1;
    U.form_serving = 100.0f;
    U.splash_timer = SPLASH_DURATION;
    U.nav_indicator_x = -1.0f;    /* sentinel: snap to place on first frame */
    U.achievement_focus = -1;

    /* A profile that was never saved means this is a first run, so the setup
       wizard takes over instead of dropping the user on a dashboard full of
       placeholder numbers. */
    if (!stone_app()->profile.configured) {
        U.page = PAGE_ONBOARDING;
        U.onboard_step = 0;
    }

    theme_refresh(stone_app()->settings.dark_theme);
}

/* -------------------------------------------------------------- splash */

static void draw_splash(int width, int height, const StoneTheme *t)
{
    float elapsed  = SPLASH_DURATION - U.splash_timer;
    float fade_in  = elapsed / SPLASH_FADE_EDGE;
    float fade_out = U.splash_timer / SPLASH_FADE_EDGE;
    float alpha, pop;
    float cx = (float)width * 0.5f;
    float cy = (float)height * 0.5f;
    float logo_r = ui_dp(42.0f);
    const char *tagline = "Offline fitness & diet tracker";
    char mark[2] = { STONE_APP_NAME[0], '\0' };
    int i;

    if (fade_in > 1.0f) fade_in = 1.0f;
    if (fade_in < 0.0f) fade_in = 0.0f;
    if (fade_out > 1.0f) fade_out = 1.0f;
    if (fade_out < 0.0f) fade_out = 0.0f;
    alpha = fade_in < fade_out ? fade_in : fade_out;
    pop = 0.82f + 0.18f * fade_in;

    /* The splash is always the brand's dark canvas, light theme or not: it is
       a logo lockup, not a screen of content. */
    render_rect(rect_make(0, 0, (float)width, (float)height),
                color_hex(0x0E1116, 1.0f), 0.0f);

    if (render_image_available("splash")) {
        float side = (float)width * 1.02f;
        float max_side = (float)height * 0.78f;
        Rect box;
        if (side > max_side) side = max_side;
        box = rect_make(cx - side * 0.5f, cy - ui_dp(46.0f) - side * 0.5f, side, side);
        render_image("splash", box, color_rgba(1.0f, 1.0f, 1.0f, alpha), 0.0f);
        cy = box.y + box.h * 0.5f + ui_dp(16.0f);
    } else {
        /* Vector fallback: the app has to launch cleanly even if the asset
           pack was stripped out of the APK. */
        render_circle(cx, cy - ui_dp(30.0f), logo_r * pop, color_alpha(t->primary, alpha));
        render_ring(cx, cy - ui_dp(30.0f), logo_r * pop + ui_dp(8.0f), ui_dp(2.0f),
                    color_alpha(t->primary, alpha * (0.25f + 0.2f *
                                (0.5f + 0.5f * sinf(U.time * 2.4f)))));
        render_text_aligned(mark,
                            rect_make(cx - logo_r * pop, cy - ui_dp(30.0f) - logo_r * pop,
                                      logo_r * 2.0f * pop, logo_r * 2.0f * pop),
                            logo_r * 1.05f * pop,
                            color_alpha(color_hex(0x06231C, 1.0f), alpha), 1, TEXT_CENTER);
    }

    {
        float w = render_text_width(STONE_APP_NAME, ui_dp(27.0f), 1);
        render_text(STONE_APP_NAME, cx - w * 0.5f, cy + ui_dp(30.0f), ui_dp(27.0f),
                   color_alpha(color_hex(0xF3F6FA, 1.0f), alpha), 1);
    }
    {
        float w = render_text_width(tagline, ui_dp(12.0f), 0);
        render_text(tagline, cx - w * 0.5f, cy + ui_dp(62.0f), ui_dp(12.0f),
                   color_alpha(color_hex(0x6A7686, 1.0f), alpha), 0);
    }

    for (i = 0; i < 3; ++i) {
        float phase = U.time * 4.2f - (float)i * 0.55f;
        float sv = 0.5f + 0.5f * sinf(phase);
        float dr = ui_dp(4.0f) + ui_dp(2.4f) * sv;
        float dx = cx + ((float)i - 1.0f) * ui_dp(20.0f);
        float dy = cy + ui_dp(100.0f);
        render_circle(dx, dy, dr, color_alpha(t->primary, alpha * (0.45f + 0.55f * sv)));
    }
}

void ui_frame(float dt, int width, int height)
{
    const StoneTheme *t;
    Rect header, content, nav;
    float base;

    U.dt = dt > 0.05f ? 0.05f : dt;    /* clamp after a long stall */
    U.time += U.dt;
    U.page_anim += (1.0f - U.page_anim) * 0.22f;

    /* dp scale: a 400 dp wide design stretched to the real screen, clamped so
       tablets do not get comically large widgets. */
    base = (float)width / 400.0f;
    if (base < 0.7f) base = 0.7f;
    if (base > 2.6f) base = 2.6f;
    U.scale = base;
    render_set_density(base);

    /* Safe area, refreshed every frame because the system can put a bar back
       on screen between any two of them. Zero on every edge is the normal,
       fully immersive case. */
    {
        StoneInsets in = stone_window_insets();
        U.inset_left   = in.left;
        U.inset_top    = in.top;
        U.inset_right  = in.right;
        U.inset_bottom = in.bottom;
        if (U.inset_left + U.inset_right > (float)width * 0.4f)
            U.inset_left = U.inset_right = 0.0f;
        if (U.inset_top + U.inset_bottom > (float)height * 0.4f)
            U.inset_top = U.inset_bottom = 0.0f;
    }

    theme_refresh(stone_app()->settings.dark_theme);
    t = theme();

    render_begin(width, height, t->bg);

    if (U.splash_timer > 0.0f) {
        /* First frames after launch: show a branded splash instead of the
           real UI while data finishes loading, so the app never appears to
           freeze on a blank screen. */
        draw_splash(width, height, t);
        U.splash_timer -= U.dt;
    } else {
        /* The setup wizard owns the whole screen: no header, no tab bar, and
           nothing to navigate away to until it is finished. */
        int chrome = (U.page != PAGE_ONBOARDING);
        float avail_w = (float)width - U.inset_left - U.inset_right;

        if (chrome) {
            header  = rect_make(U.inset_left, U.inset_top + ui_dp(12.0f),
                                avail_w, ui_dp(52.0f));
            nav     = rect_make(0.0f, (float)height - U.inset_bottom - ui_dp(58.0f),
                                (float)width, ui_dp(58.0f));
            content = rect_make(U.inset_left, header.y + header.h + ui_dp(4.0f),
                                avail_w,
                                nav.y - (header.y + header.h) - ui_dp(8.0f));
        } else {
            header  = rect_make(0.0f, 0.0f, 0.0f, 0.0f);
            nav     = rect_make(0.0f, (float)height - U.inset_bottom, (float)width, 0.0f);
            content = rect_make(U.inset_left, U.inset_top, avail_w,
                                (float)height - U.inset_top - U.inset_bottom);
        }
        if (content.h < ui_dp(80.0f)) content.h = ui_dp(80.0f);
        U.safe = content;

        if (chrome) draw_header(header);

        /* Every page enter (tab switch, drill-in, back) restarts page_anim
           at 0.0 and eases it to 1.0; turn that into a gentle slide-up +
           fade so navigating never feels like an instant, jarring cut. */
        {
            float pe = stone_app()->settings.animations_enabled ? U.page_anim : 1.0f;
            float dy = (1.0f - pe) * ui_dp(16.0f);
            render_set_page_transform(0.0f, dy, pe);
        }

        switch (U.page) {
        case PAGE_DASHBOARD:       page_dashboard(content); break;
        case PAGE_DIET:            page_diet(content); break;
        case PAGE_WORKOUT:         page_workout(content); break;
        case PAGE_WORKOUT_DETAIL:  page_workout_detail(content); break;
        case PAGE_WORKOUT_SESSION: page_workout_session(content); break;
        case PAGE_PROGRESS:        page_progress(content); break;
        case PAGE_PROFILE:         page_profile(content); break;
        case PAGE_HISTORY:         page_history(content); break;
        case PAGE_ADD_FOOD:        page_add_food(content); break;
        case PAGE_ABOUT:           page_about(content); break;
        case PAGE_ACHIEVEMENTS:    page_achievements(content); break;
        case PAGE_ONBOARDING:      page_onboarding(content); break;
        default:                   page_dashboard(content); break;
        }

        render_clear_page_transform();

        if (chrome) draw_nav_bar(nav);
        ui_keyboard_draw();
        ui_toast();

        /* Deliver any committed keyboard value to the page layer. */
        {
            int kind;
            void *target;
            char text[64];
            if (ui_keyboard_take(&kind, &target, text, sizeof(text)))
                ui_handle_input_commit(kind, target, text);
        }
    }

    render_end();

    /* Consume one-shot input flags at the very end of the frame. */
    U.input.pressed = 0;
    U.input.released = 0;
    U.input.move_dx = U.input.move_dy = 0.0f;
    U.keyboard.just_opened = 0;
    if (U.input.down) U.input.hold_time += U.dt;
    else { U.input.hold_time = 0.0f; U.input.dragged = 0; }
}

/* ------------------------------------------------------------- artwork --
 * Both helpers degrade to vector drawing when the asset pack is absent, so
 * neither the achievements grid nor a page header can end up as a hole in
 * the layout on a build without assets/stone_pack.stpk.
 */

void ui_badge(Rect r, int tile, int unlocked)
{
    const StoneTheme *t = theme();
    float u0, v0;
    Color tint;

    if (tile < 0) tile = 0;
    if (tile > 15) tile = 15;

    if (render_image_available("badges")) {
        u0 = (float)(tile % 4) * 0.25f;
        v0 = (float)(tile / 4) * 0.25f;
        tint = unlocked ? color_rgba(1.0f, 1.0f, 1.0f, 1.0f)
                        : color_rgba(0.42f, 0.46f, 0.52f, 0.40f);
        render_image_sub("badges", r, u0, v0, u0 + 0.25f, v0 + 0.25f, tint, 0.0f);
        return;
    }

    {
        float cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
        float rad = (r.w < r.h ? r.w : r.h) * 0.42f;
        Color c = unlocked ? t->primary : t->text_faint;
        render_circle(cx, cy, rad, color_alpha(c, unlocked ? 0.18f : 0.08f));
        render_ring(cx, cy, rad, ui_dp(2.0f), color_alpha(c, unlocked ? 0.9f : 0.35f));
        render_circle(cx, cy, rad * 0.34f, color_alpha(c, unlocked ? 1.0f : 0.3f));
    }
}

void ui_hero_band(Rect r, int tab, float alpha)
{
    const StoneTheme *t = theme();
    /* The hero atlas is five stacked 1024x204 bands, one per tab. */
    const float band = 204.0f / 1024.0f;
    float v0;

    if (r.w <= 0.0f || r.h <= 0.0f || alpha <= 0.0f) return;
    if (tab < 0) tab = 0;
    if (tab > 4) tab = 4;
    v0 = (float)tab * band;

    if (render_image_available("hero")) {
        render_image_sub("hero", r, 0.0f, v0, 1.0f, v0 + band,
                         color_rgba(1.0f, 1.0f, 1.0f, alpha), ui_dp(20.0f));
    } else {
        render_rect_gradient(r, color_alpha(t->primary, 0.18f * alpha),
                             color_alpha(t->surface, alpha), ui_dp(18.0f));
    }
}
