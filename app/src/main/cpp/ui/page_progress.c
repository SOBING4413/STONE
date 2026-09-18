/* page_progress.c - charts built from whatever the user has logged so far. */
#include "ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../core/calc.h"
#include "../renderer/chart.h"

#define SERIES_CAP 32

void page_progress(Rect area)
{
    StoneApp *app = stone_app();
    UiState *U = ui_state();
    const StoneTheme *t = theme();
    Rect content, cur, card;
    float pad = ui_dp(16.0f);
    char buf[96];
    ChartStyle style;
    int i;

    content = ui_scroll_begin(PAGE_PROGRESS, area, ui_dp(760.0f));
    cur = rect_inset(content, pad, 0.0f);
    cur.y += ui_dp(4.0f);

    /* ---- headline stats -------------------------------------------------- */
    {
        float gap = ui_dp(12.0f);
        float tw = (cur.w - gap) * 0.5f;
        float th = ui_dp(84.0f);
        Rect row = ui_row(&cur, th, gap);
        float w = stone_latest_weight();
        float d = w - app->profile.target_weight_kg;

        snprintf(buf, sizeof(buf), "%.1f kg", w);
        ui_stat_tile(rect_make(row.x, row.y, tw, th), "CURRENT", buf, "latest entry", t->primary);
        snprintf(buf, sizeof(buf), "%.1f kg", fabsf(d));
        ui_stat_tile(rect_make(row.x + tw + gap, row.y, tw, th), "TO TARGET", buf,
                     d > 0 ? "to lose" : (d < 0 ? "to gain" : "target reached"), t->accent);

        row = ui_row(&cur, th, gap);
        snprintf(buf, sizeof(buf), "%d", stone_active_streak());
        ui_stat_tile(rect_make(row.x, row.y, tw, th), "DAY STREAK", buf, "keep it alive", t->warn);
        snprintf(buf, sizeof(buf), "%d", app->workout_log_count);
        ui_stat_tile(rect_make(row.x + tw + gap, row.y, tw, th), "TOTAL WORKOUTS", buf,
                     "all time", t->primary);
    }

    /* ---- tab selector ---------------------------------------------------- */
    {
        static const char *tabs[3] = {"Weight", "Calories", "Workouts"};
        Rect row = ui_row(&cur, ui_dp(36.0f), ui_dp(12.0f));
        float gap = ui_dp(8.0f);
        float cw = (row.w - gap * 2.0f) / 3.0f;
        for (i = 0; i < 3; ++i) {
            Rect c = rect_make(row.x + (cw + gap) * (float)i, row.y, cw, row.h);
            if (ui_chip(600 + i, c, tabs[i], U->progress_tab == i)) U->progress_tab = i;
        }
    }

    memset(&style, 0, sizeof(style));
    style.grid = color_alpha(t->border, 0.8f);
    style.label = t->text_faint;
    style.label_size = ui_dp(10.0f);
    style.show_points = 1;

    card = ui_row(&cur, ui_dp(250.0f), ui_dp(14.0f));
    ui_card(card);

    if (U->progress_tab == 0) {
        float kg[SERIES_CAP];
        char dates[SERIES_CAP][STONE_DATE_LEN];
        char shorts[SERIES_CAP][10];
        const char *labels[SERIES_CAP];
        int n = stone_weight_series(90, kg, dates, SERIES_CAP);

        render_text("WEIGHT (kg)", card.x + pad, card.y + ui_dp(14.0f),
                    ui_dp(11.0f), t->text_faint, 1);
        if (n < 2) {
            ui_empty_state(rect_inset(card, ui_dp(16.0f), ui_dp(34.0f)),
                           "Log your weight", "At least two entries are needed for a chart");
        } else {
            for (i = 0; i < n; ++i) {
                stone_date_short(dates[i], shorts[i], sizeof(shorts[i]));
                labels[i] = shorts[i];
            }
            style.line = t->primary;
            style.fill_top = color_alpha(t->primary, 0.22f);
            style.target = app->profile.target_weight_kg;
            chart_line(rect_make(card.x + pad, card.y + ui_dp(40.0f),
                                 card.w - pad * 2.0f, card.h - ui_dp(58.0f)),
                       kg, n, labels, &style);
        }
    } else if (U->progress_tab == 1) {
        float kcal[14];
        char dates[14][STONE_DATE_LEN];
        char shorts[14][10];
        const char *labels[14];
        int n = stone_calorie_series(14, kcal, dates, 14);

        render_text("CALORIES IN (last 14 days)", card.x + pad, card.y + ui_dp(14.0f),
                    ui_dp(11.0f), t->text_faint, 1);
        for (i = 0; i < n; ++i) {
            stone_date_short(dates[i], shorts[i], sizeof(shorts[i]));
            labels[i] = shorts[i];
        }
        style.line = t->accent;
        style.fill_top = color_alpha(t->accent, 0.20f);
        style.target = stone_target_calories(&app->profile);
        chart_bars(rect_make(card.x + pad, card.y + ui_dp(40.0f),
                             card.w - pad * 2.0f, card.h - ui_dp(58.0f)),
                   kcal, n, labels, &style);
    } else {
        int counts[8];
        float vals[8];
        char lbl[8][8];
        const char *labels[8];
        int n = stone_workout_week_series(8, counts, 8);

        render_text("WORKOUTS PER WEEK", card.x + pad, card.y + ui_dp(14.0f),
                    ui_dp(11.0f), t->text_faint, 1);
        for (i = 0; i < n; ++i) {
            vals[i] = (float)counts[i];
            /* n is bounded by the array size, so the label always fits;
               the mask keeps static analysers from assuming otherwise. */
            snprintf(lbl[i], sizeof(lbl[i]), "W-%d", (n - 1 - i) & 0xFF);
            labels[i] = lbl[i];
        }
        style.line = t->primary;
        chart_bars(rect_make(card.x + pad, card.y + ui_dp(40.0f),
                             card.w - pad * 2.0f, card.h - ui_dp(58.0f)),
                   vals, n, labels, &style);
    }

    /* ---- achievements ---------------------------------------------------- */
    ui_section_title(&cur, "ACHIEVEMENTS");
    {
        struct { const char *name; const char *hint; int done; } ach[5];
        int streak = stone_active_streak();
        int total = app->workout_log_count;

        ach[0].name = "First workout";      ach[0].hint = "Complete one session";       ach[0].done = total >= 1;
        ach[1].name = "Weekly target";      ach[1].hint = "Hit your weekly workouts";   ach[1].done = stone_workouts_this_week() >= app->profile.weekly_workout_target;
        ach[2].name = "7 day streak";       ach[2].hint = "Stay active a full week";    ach[2].done = streak >= 7;
        ach[3].name = "10 workouts";        ach[3].hint = "Reach 10 sessions total";    ach[3].done = total >= 10;
        ach[4].name = "Tracking habit";     ach[4].hint = "Log food on 5 days";         ach[4].done = stone_active_days_total() >= 5;

        for (i = 0; i < 5; ++i) {
            Rect row = ui_row(&cur, ui_dp(58.0f), ui_dp(8.0f));
            Color c = ach[i].done ? t->primary : t->text_faint;
            render_rect(row, t->surface, ui_dp(14.0f));
            render_ring(row.x + ui_dp(28.0f), row.y + row.h * 0.5f, ui_dp(13.0f),
                        ui_dp(2.0f), c);
            if (ach[i].done)
                render_circle(row.x + ui_dp(28.0f), row.y + row.h * 0.5f, ui_dp(7.0f), c);
            render_text(ach[i].name, row.x + ui_dp(52.0f), row.y + ui_dp(12.0f),
                        ui_dp(13.5f), ach[i].done ? t->text : t->text_dim, 1);
            render_text(ach[i].hint, row.x + ui_dp(52.0f), row.y + ui_dp(32.0f),
                        ui_dp(11.0f), t->text_faint, 0);
            render_text_aligned(ach[i].done ? "done" : "locked",
                                rect_inset(row, ui_dp(14.0f), 0), ui_dp(11.0f), c, 1, TEXT_RIGHT);
        }
    }

    ui_disclaimer(&cur, cur.w);
    cur.y += ui_dp(10.0f);
    ui_scroll_set_content(PAGE_PROGRESS, cur.y - content.y);
    ui_scroll_end();
}
