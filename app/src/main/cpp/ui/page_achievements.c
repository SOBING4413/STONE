/* page_achievements.c - the badge wall.
 *
 * Nothing here is stored: every badge is recomputed from the logs on each
 * frame (see core/achievements.c), which is cheap at this data size and means
 * the grid can never disagree with the numbers on the other pages.
 */
#include "ui.h"

#include <stdio.h>
#include <string.h>

#include "../core/achievements.h"
#include "../core/calc.h"

#define GRID_COLS 4

void page_achievements(Rect area)
{
    UiState *U = ui_state();
    const StoneTheme *t = theme();
    Rect content, cur, card;
    float pad = ui_dp(16.0f);
    int unlocked = stone_achievements_unlocked_count();
    int next = stone_achievement_next();
    float cell_w, cell_h;
    char buf[128];
    int i, rows;

    rows = (STONE_ACHIEVEMENT_COUNT + GRID_COLS - 1) / GRID_COLS;

    content = ui_scroll_begin(PAGE_ACHIEVEMENTS, area, ui_dp(880.0f));
    cur = rect_inset(content, pad, 0.0f);
    cur.y += ui_dp(4.0f);

    /* ---- summary -------------------------------------------------------- */
    card = ui_row(&cur, ui_dp(152.0f), ui_dp(14.0f));
    ui_hero_band(card, PAGE_PROFILE, 0.85f);
    render_rect_outline(card, t->border, ui_dp(18.0f), 1.0f);
    {
        float cx = card.x + card.w - ui_dp(62.0f);
        float cy = card.y + card.h * 0.5f;
        snprintf(buf, sizeof(buf), "%d/%d", unlocked, STONE_ACHIEVEMENT_COUNT);
        ui_ring_stat(cx, cy, ui_dp(42.0f), stone_achievements_ratio(),
                     buf, "unlocked", t->primary);

        render_text("Achievements", card.x + pad, card.y + ui_dp(26.0f),
                    ui_dp(19.0f), t->text, 1);
        render_text("Earned from what you have already logged",
                    card.x + pad, card.y + ui_dp(52.0f), ui_dp(11.5f), t->text_dim, 0);

        if (next >= 0) {
            const StoneAchievement *a = stone_achievement(next);
            snprintf(buf, sizeof(buf), "Next up: %s", a->name);
            render_text(buf, card.x + pad, card.y + ui_dp(84.0f),
                        ui_dp(12.5f), t->primary, 1);
            snprintf(buf, sizeof(buf), "%d of %d %s",
                     stone_achievement_value(next), a->goal, a->unit);
            render_text(buf, card.x + pad, card.y + ui_dp(104.0f),
                        ui_dp(11.0f), t->text_faint, 0);
            ui_progress_bar(rect_make(card.x + pad, card.y + ui_dp(124.0f),
                                      card.w - ui_dp(150.0f), ui_dp(7.0f)),
                            a->goal > 0 ? (float)stone_achievement_value(next) /
                                          (float)a->goal : 0.0f,
                            t->primary);
        } else {
            render_text("Every badge unlocked. Outstanding.",
                        card.x + pad, card.y + ui_dp(92.0f), ui_dp(12.5f), t->primary, 1);
        }
    }

    /* ---- grid ----------------------------------------------------------- */
    ui_section_title(&cur, "BADGES");

    cell_w = (cur.w - ui_dp(10.0f) * (float)(GRID_COLS - 1)) / (float)GRID_COLS;
    cell_h = cell_w + ui_dp(26.0f);

    for (i = 0; i < STONE_ACHIEVEMENT_COUNT; ++i) {
        const StoneAchievement *a = stone_achievement(i);
        int col = i % GRID_COLS;
        int row = i / GRID_COLS;
        int got = stone_achievement_unlocked(i);
        Rect tile = rect_make(cur.x + (cell_w + ui_dp(10.0f)) * (float)col,
                              cur.y + (cell_h + ui_dp(10.0f)) * (float)row,
                              cell_w, cell_h);
        char clipped[32];

        if (ui_touch_area(1600 + i, tile))
            U->achievement_focus = (U->achievement_focus == i) ? -1 : i;

        if (U->achievement_focus == i)
            render_rect(rect_inset(tile, ui_dp(-2.0f), ui_dp(-2.0f)),
                        color_alpha(t->primary, 0.14f), ui_dp(16.0f));

        ui_badge(rect_make(tile.x, tile.y, cell_w, cell_w), a->tile, got);
        render_text_ellipsis(a->name, cell_w, ui_dp(10.0f), got,
                             clipped, sizeof(clipped));
        render_text_aligned(clipped,
                            rect_make(tile.x, tile.y + cell_w + ui_dp(2.0f),
                                      cell_w, ui_dp(18.0f)),
                            ui_dp(10.0f), got ? t->text : t->text_faint, got,
                            TEXT_CENTER);
    }

    cur.y += (cell_h + ui_dp(10.0f)) * (float)rows + ui_dp(6.0f);
    cur.h -= (cell_h + ui_dp(10.0f)) * (float)rows + ui_dp(6.0f);

    /* ---- detail of the selected badge ------------------------------------ */
    if (U->achievement_focus >= 0) {
        const StoneAchievement *a = stone_achievement(U->achievement_focus);
        int got = stone_achievement_unlocked(U->achievement_focus);
        int val = stone_achievement_value(U->achievement_focus);
        Rect box = ui_row(&cur, ui_dp(124.0f), ui_dp(12.0f));

        ui_card(box);
        ui_badge(rect_make(box.x + pad, box.y + ui_dp(18.0f),
                           ui_dp(64.0f), ui_dp(64.0f)), a->tile, got);
        render_text(a->name, box.x + ui_dp(94.0f), box.y + ui_dp(20.0f),
                    ui_dp(16.0f), t->text, 1);
        render_text_wrapped(a->detail,
                            rect_make(box.x + ui_dp(94.0f), box.y + ui_dp(44.0f),
                                      box.w - ui_dp(110.0f), ui_dp(34.0f)),
                            ui_dp(11.5f), t->text_dim, 0, 1);
        if (got) snprintf(buf, sizeof(buf), "Unlocked  -  %d %s", val, a->unit);
        else     snprintf(buf, sizeof(buf), "Progress  -  %d of %d %s",
                          val, a->goal, a->unit);
        render_text(buf, box.x + ui_dp(94.0f), box.y + ui_dp(84.0f),
                    ui_dp(11.5f), got ? t->primary : t->text_faint, 1);
        ui_progress_bar(rect_make(box.x + pad, box.y + box.h - ui_dp(16.0f),
                                  box.w - pad * 2.0f, ui_dp(6.0f)),
                        a->goal > 0 ? (float)val / (float)a->goal : 0.0f,
                        got ? t->primary : t->accent);
    } else {
        Rect hint = ui_row(&cur, ui_dp(46.0f), ui_dp(12.0f));
        render_text_aligned("Tap a badge to see how it is earned", hint,
                            ui_dp(11.5f), t->text_faint, 0, TEXT_CENTER);
    }

    cur.y += ui_dp(12.0f);
    ui_scroll_set_content(PAGE_ACHIEVEMENTS, cur.y - content.y);
    ui_scroll_end();
}
