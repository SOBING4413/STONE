/* page_about.c - credits, version metadata and the offline-by-design notice. */
#include "ui.h"

#include <stdio.h>

void page_about(Rect area)
{
    StoneApp *app = stone_app();
    const StoneTheme *t = theme();
    Rect content, cur, card;
    float pad = ui_dp(16.0f);
    char buf[128];

    content = ui_scroll_begin(PAGE_ABOUT, area, ui_dp(720.0f));
    cur = rect_inset(content, pad, 0.0f);
    cur.y += ui_dp(4.0f);

    /* ---- logo card ------------------------------------------------------ */
    card = ui_row(&cur, ui_dp(176.0f), ui_dp(14.0f));
    ui_card_shadowed(card);
    {
        float cx = card.x + card.w * 0.5f;
        float cy = card.y + ui_dp(58.0f);
        render_circle(cx, cy, ui_dp(34.0f), color_alpha(t->primary, 0.16f));
        render_ring(cx, cy, ui_dp(34.0f), ui_dp(2.0f), t->primary);
        render_rect(rect_make(cx - ui_dp(12.0f), cy - ui_dp(12.0f),
                              ui_dp(24.0f), ui_dp(24.0f)), t->primary, ui_dp(7.0f));

        render_text_aligned(STONE_APP_NAME,
                            rect_make(card.x, card.y + ui_dp(100.0f), card.w, ui_dp(34.0f)),
                            ui_dp(26.0f), t->text, 1, TEXT_CENTER);
        snprintf(buf, sizeof(buf), "Version %s  -  offline fitness companion",
                 STONE_APP_VERSION);
        render_text_aligned(buf,
                            rect_make(card.x, card.y + ui_dp(136.0f), card.w, ui_dp(20.0f)),
                            ui_dp(11.5f), t->text_faint, 0, TEXT_CENTER);
    }

    /* ---- credits -------------------------------------------------------- */
    ui_section_title(&cur, "CREDITS");
    card = ui_row(&cur, ui_dp(104.0f), ui_dp(12.0f));
    ui_card(card);
    {
        Rect inner = rect_inset(card, pad, ui_dp(14.0f));
        render_text(STONE_AUTHOR, inner.x, inner.y, ui_dp(16.0f), t->text, 1);
        render_text("Design, native C engine and app logic",
                    inner.x, inner.y + ui_dp(24.0f), ui_dp(11.5f), t->text_faint, 0);
        render_text(STONE_STUDIO, inner.x, inner.y + ui_dp(50.0f),
                    ui_dp(15.0f), t->primary, 1);
    }

    /* ---- build info ----------------------------------------------------- */
    ui_section_title(&cur, "BUILD");
    card = ui_row(&cur, ui_dp(146.0f), ui_dp(12.0f));
    ui_card(card);
    {
        Rect inner = rect_inset(card, pad, ui_dp(12.0f));
        float y = inner.y;
        ui_kv_row(rect_make(inner.x, y, inner.w, ui_dp(24.0f)), "Package", "com.stone.app");
        y += ui_dp(26.0f);
        ui_kv_row(rect_make(inner.x, y, inner.w, ui_dp(24.0f)), "Language", "C99 / Android NDK");
        y += ui_dp(26.0f);
        ui_kv_row(rect_make(inner.x, y, inner.w, ui_dp(24.0f)), "Renderer", "OpenGL ES 2.0");
        y += ui_dp(26.0f);
        snprintf(buf, sizeof(buf), "%d foods / %d workouts",
                 app->food_count, app->workout_count);
        ui_kv_row(rect_make(inner.x, y, inner.w, ui_dp(24.0f)), "Offline data", buf);
        y += ui_dp(26.0f);
        ui_kv_row(rect_make(inner.x, y, inner.w, ui_dp(24.0f)), "Storage",
                  app->storage_warning ? "Unavailable" : "Local JSON files");
    }

    /* ---- privacy -------------------------------------------------------- */
    ui_section_title(&cur, "PRIVACY");
    {
        Rect box = ui_row(&cur, ui_dp(96.0f), ui_dp(12.0f));
        render_rect(box, t->surface_alt, ui_dp(14.0f));
        render_text_wrapped("STONE works entirely offline. There is no account, no login, "
                            "no server, no analytics and no advertising. Your profile, "
                            "meals, workouts and progress never leave this device.",
                            rect_inset(box, ui_dp(14.0f), ui_dp(12.0f)),
                            ui_dp(12.0f), t->text_dim, 0, 1);
    }

    /* ---- font licence --------------------------------------------------- */
    {
        Rect box = ui_row(&cur, ui_dp(60.0f), ui_dp(10.0f));
        render_text_wrapped("Typeface: Liberation Sans, licensed under the SIL Open Font "
                            "License 1.1, rasterised into an embedded atlas.",
                            box, ui_dp(10.5f), t->text_faint, 0, 1);
    }

    {
        Rect row = ui_row(&cur, ui_dp(46.0f), ui_dp(10.0f));
        if (ui_ghost_button(500, row, "Back")) ui_back();
    }

    ui_disclaimer(&cur, cur.w);
    cur.y += ui_dp(10.0f);
    ui_scroll_set_content(PAGE_ABOUT, cur.y - content.y);
    ui_scroll_end();
}
