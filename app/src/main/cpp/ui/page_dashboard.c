/* page_dashboard.c - the landing screen: today's numbers at a glance. */
#include "ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../core/calc.h"
#include "../renderer/chart.h"

static float weight_progress(const StoneProfile *p, float current)
{
    float start, span;
    if (!p) return 0.0f;
    start = p->goal == GOAL_GAIN ? p->target_weight_kg - 8.0f
                                 : p->target_weight_kg + 8.0f;
    span = p->target_weight_kg - start;
    if (fabsf(span) < 0.01f) return 1.0f;
    {
        float t = (current - start) / span;
        return stone_clampf(t, 0.0f, 1.0f);
    }
}

void page_dashboard(Rect area)
{
    StoneApp *app = stone_app();
    UiState *U = ui_state();
    const StoneTheme *t = theme();
    const StoneProfile *p = &app->profile;
    Rect content, cur, card;
    float pad = ui_dp(16.0f);
    float weight = stone_latest_weight();
    float bmi = stone_bmi(weight, p->height_cm);
    float target_kcal = stone_target_calories(p);
    float kcal = 0, prot = 0, carb = 0, fat = 0;
    float tp = 0, tc = 0, tf = 0;
    int burned = stone_kcal_burned_on(app->today);
    int week = stone_workouts_this_week();
    char buf[96], sub[64];

    stone_day_nutrition(app->today, &kcal, &prot, &carb, &fat);
    stone_macro_targets(p, target_kcal, &tp, &tc, &tf);

    content = ui_scroll_begin(PAGE_DASHBOARD, area, ui_dp(1090.0f));
    cur = rect_inset(content, pad, 0.0f);
    cur.y += ui_dp(4.0f);

    /* ---- hero ---------------------------------------------------------- */
    card = ui_row(&cur, ui_dp(178.0f), ui_dp(14.0f));
    render_rect_gradient(card, color_mix(t->surface, t->primary, 0.10f), t->surface, ui_dp(22.0f));
    /* Artwork band behind the numbers, knocked back by a scrim so the text on
       top keeps its contrast in both themes. */
    ui_hero_band(card, PAGE_DASHBOARD, t->bg.r < 0.5f ? 0.55f : 0.30f);
    render_rect(card, color_alpha(t->bg, t->bg.r < 0.5f ? 0.34f : 0.55f), ui_dp(22.0f));
    render_rect_outline(card, t->border, ui_dp(22.0f), 1.0f);

    snprintf(buf, sizeof(buf), "Hi, %s", p->name[0] ? p->name : "Athlete");
    render_text(buf, card.x + pad, card.y + ui_dp(16.0f), ui_dp(13.0f), t->text_dim, 0);

    snprintf(buf, sizeof(buf), "%.1f", weight);
    render_text(buf, card.x + pad, card.y + ui_dp(38.0f), ui_dp(40.0f), t->text, 1);
    render_text("kg", card.x + pad + render_text_width(buf, ui_dp(40.0f), 1) + ui_dp(6.0f),
                card.y + ui_dp(58.0f), ui_dp(14.0f), t->text_dim, 0);

    {
        float d = stone_weight_change_7d();
        Color c = (p->goal == GOAL_GAIN) ? (d >= 0 ? t->primary : t->accent)
                                         : (d <= 0 ? t->primary : t->accent);
        snprintf(buf, sizeof(buf), "%s%.1f kg / 7 days", d > 0 ? "+" : "", d);
        render_text(buf, card.x + pad, card.y + ui_dp(90.0f), ui_dp(12.5f),
                    fabsf(d) < 0.05f ? t->text_faint : c, 0);
    }

    snprintf(buf, sizeof(buf), "Target %.1f kg  -  %s", p->target_weight_kg,
             stone_goal_name(p->goal));
    render_text(buf, card.x + pad, card.y + ui_dp(112.0f), ui_dp(12.0f), t->text_faint, 0);

    ui_progress_bar(rect_make(card.x + pad, card.y + ui_dp(136.0f),
                              card.w - pad * 2.0f - ui_dp(96.0f), ui_dp(8.0f)),
                    weight_progress(p, weight), t->primary);

    ui_ring_stat(card.x + card.w - ui_dp(58.0f), card.y + ui_dp(74.0f), ui_dp(40.0f),
                 weight_progress(p, weight), NULL, NULL, t->primary);
    snprintf(buf, sizeof(buf), "%.0f%%", weight_progress(p, weight) * 100.0f);
    render_text_aligned(buf, rect_make(card.x + card.w - ui_dp(108.0f),
                                       card.y + ui_dp(60.0f), ui_dp(100.0f), ui_dp(28.0f)),
                        ui_dp(18.0f), t->text, 1, TEXT_CENTER);

    /* ---- stat tiles ----------------------------------------------------- */
    {
        float gap = ui_dp(12.0f);
        float tw = (cur.w - gap) * 0.5f;
        float th = ui_dp(84.0f);
        Rect row = ui_row(&cur, th, gap);

        snprintf(buf, sizeof(buf), "%.1f", bmi);
        ui_stat_tile(rect_make(row.x, row.y, tw, th), "BMI", buf,
                     stone_bmi_category(bmi), t->primary);

        snprintf(buf, sizeof(buf), "%.0f", target_kcal);
        snprintf(sub, sizeof(sub), "kcal/day estimate");
        ui_stat_tile(rect_make(row.x + tw + gap, row.y, tw, th), "DAILY TARGET", buf,
                     sub, t->accent);

        row = ui_row(&cur, th, gap);
        snprintf(buf, sizeof(buf), "%d/%d", week, p->weekly_workout_target);
        ui_stat_tile(rect_make(row.x, row.y, tw, th), "WORKOUTS THIS WEEK", buf,
                     week >= p->weekly_workout_target ? "target reached" : "keep going",
                     t->primary);

        snprintf(buf, sizeof(buf), "%d", stone_active_streak());
        snprintf(sub, sizeof(sub), "%d active days total", stone_active_days_total());
        ui_stat_tile(rect_make(row.x + tw + gap, row.y, tw, th), "STREAK", buf, sub, t->warn);
    }

    /* ---- today's calories ---------------------------------------------- */
    ui_section_title(&cur, "TODAY");
    card = ui_row(&cur, ui_dp(168.0f), ui_dp(14.0f));
    ui_card(card);
    {
        float bx = card.x + pad, bw = card.w - pad * 2.0f;
        float eaten_t = target_kcal > 0 ? kcal / target_kcal : 0.0f;
        float my = card.y + ui_dp(84.0f);
        const char *names[3] = {"Protein", "Carbs", "Fat"};
        float got[3], goal[3];
        int i;

        got[0] = prot; got[1] = carb; got[2] = fat;
        goal[0] = tp;  goal[1] = tc;  goal[2] = tf;

        snprintf(buf, sizeof(buf), "%.0f / %.0f kcal", kcal, target_kcal);
        render_text(buf, bx, card.y + ui_dp(16.0f), ui_dp(18.0f), t->text, 1);
        snprintf(buf, sizeof(buf), "%.0f kcal burned  -  %.0f kcal left",
                 (float)burned, target_kcal - kcal + (float)burned);
        render_text(buf, bx, card.y + ui_dp(40.0f), ui_dp(11.5f), t->text_faint, 0);
        ui_progress_bar(rect_make(bx, card.y + ui_dp(60.0f), bw, ui_dp(9.0f)),
                        eaten_t, eaten_t > 1.05f ? t->danger : t->accent);

        for (i = 0; i < 3; ++i) {
            float cw = (bw - ui_dp(20.0f)) / 3.0f;
            float cx = bx + (cw + ui_dp(10.0f)) * (float)i;
            Color c = i == 0 ? t->primary : (i == 1 ? t->warn : t->accent);
            snprintf(buf, sizeof(buf), "%.0f / %.0f g", got[i], goal[i]);
            render_text(names[i], cx, my, ui_dp(11.5f), t->text_dim, 0);
            render_text(buf, cx, my + ui_dp(17.0f), ui_dp(12.5f), t->text, 1);
            ui_progress_bar(rect_make(cx, my + ui_dp(38.0f), cw, ui_dp(6.0f)),
                            goal[i] > 0 ? got[i] / goal[i] : 0.0f, c);
        }
    }

    /* ---- quick actions -------------------------------------------------- */
    ui_section_title(&cur, "QUICK ACTIONS");
    {
        float gap = ui_dp(10.0f);
        float bw = (cur.w - gap * 2.0f) / 3.0f;
        Rect row = ui_row(&cur, ui_dp(46.0f), ui_dp(16.0f));

        if (ui_button(101, rect_make(row.x, row.y, bw, row.h), "Log weight", 1)) {
            char init[16];
            snprintf(init, sizeof(init), "%.1f", weight);
            ui_keyboard_open(KB_DECIMAL, "Weight today (kg)", init, KB_TARGET_WEIGHT, NULL);
        }
        if (ui_button(102, rect_make(row.x + bw + gap, row.y, bw, row.h), "Log food", 0))
            ui_navigate(PAGE_DIET);
        if (ui_button(103, rect_make(row.x + (bw + gap) * 2.0f, row.y, bw, row.h), "Workout", 0))
            ui_navigate(PAGE_WORKOUT);
    }

    /* ---- weight trend --------------------------------------------------- */
    ui_section_title(&cur, "WEIGHT TREND");
    card = ui_row(&cur, ui_dp(190.0f), ui_dp(14.0f));
    ui_card(card);
    {
        float kg[32];
        char dates[32][STONE_DATE_LEN];
        char shorts[32][10];
        const char *labels[32];
        int n = stone_weight_series(60, kg, dates, 32);
        int i;

        if (n < 2) {
            ui_empty_state(rect_inset(card, ui_dp(14.0f), ui_dp(14.0f)),
                           "Not enough data yet", "Log your weight for a few days");
        } else {
            ChartStyle s;
            for (i = 0; i < n; ++i) {
                stone_date_short(dates[i], shorts[i], sizeof(shorts[i]));
                labels[i] = shorts[i];
            }
            memset(&s, 0, sizeof(s));
            s.line = t->primary;
            s.grid = color_alpha(t->border, 0.8f);
            s.label = t->text_faint;
            s.fill_top = color_alpha(t->primary, 0.22f);
            s.target = p->target_weight_kg;
            s.show_points = 1;
            s.label_size = ui_dp(10.0f);
            chart_line(rect_inset(card, ui_dp(16.0f), ui_dp(18.0f)), kg, n, labels, &s);
        }
    }

    /* ---- recent activity ------------------------------------------------ */
    ui_section_title(&cur, "RECENT ACTIVITY");
    {
        int shown = 0, i;
        for (i = app->workout_log_count - 1; i >= 0 && shown < 3; --i, ++shown) {
            Rect row = ui_row(&cur, ui_dp(56.0f), ui_dp(8.0f));
            char when[16];
            render_rect(row, t->surface, ui_dp(14.0f));
            render_circle(row.x + ui_dp(26.0f), row.y + row.h * 0.5f, ui_dp(12.0f),
                          color_alpha(t->primary, 0.18f));
            render_rect(rect_make(row.x + ui_dp(21.0f), row.y + row.h * 0.5f - ui_dp(1.5f),
                                  ui_dp(10.0f), ui_dp(3.0f)), t->primary, ui_dp(1.5f));
            render_text(app->workout_log[i].workout_name, row.x + ui_dp(48.0f),
                        row.y + ui_dp(12.0f), ui_dp(13.5f), t->text, 1);
            snprintf(buf, sizeof(buf), "%d min  -  %d kcal",
                     app->workout_log[i].duration_s / 60, app->workout_log[i].kcal_burned);
            render_text(buf, row.x + ui_dp(48.0f), row.y + ui_dp(30.0f),
                        ui_dp(11.0f), t->text_faint, 0);
            stone_date_short(app->workout_log[i].date, when, sizeof(when));
            render_text_aligned(when, rect_inset(row, ui_dp(14.0f), 0), ui_dp(11.0f),
                                t->text_faint, 0, TEXT_RIGHT);
        }
        for (i = app->food_log_count - 1; i >= 0 && shown < 6; --i, ++shown) {
            Rect row = ui_row(&cur, ui_dp(56.0f), ui_dp(8.0f));
            char when[16];
            render_rect(row, t->surface, ui_dp(14.0f));
            render_circle(row.x + ui_dp(26.0f), row.y + row.h * 0.5f, ui_dp(12.0f),
                          color_alpha(t->accent, 0.18f));
            render_ring(row.x + ui_dp(26.0f), row.y + row.h * 0.5f, ui_dp(6.0f),
                        ui_dp(2.0f), t->accent);
            render_text(app->food_log[i].food_name, row.x + ui_dp(48.0f),
                        row.y + ui_dp(12.0f), ui_dp(13.5f), t->text, 1);
            snprintf(buf, sizeof(buf), "%s  -  %.0f kcal",
                     stone_meal_name(app->food_log[i].meal), app->food_log[i].kcal);
            render_text(buf, row.x + ui_dp(48.0f), row.y + ui_dp(30.0f),
                        ui_dp(11.0f), t->text_faint, 0);
            stone_date_short(app->food_log[i].date, when, sizeof(when));
            render_text_aligned(when, rect_inset(row, ui_dp(14.0f), 0), ui_dp(11.0f),
                                t->text_faint, 0, TEXT_RIGHT);
        }
        if (shown == 0)
            ui_empty_state(ui_row(&cur, ui_dp(90.0f), ui_dp(8.0f)), "No activity yet",
                           "Log a meal or finish a workout to start");
        if (ui_ghost_button(104, ui_row(&cur, ui_dp(42.0f), ui_dp(12.0f)), "Open history"))
            ui_navigate(PAGE_HISTORY);
    }

    ui_disclaimer(&cur, cur.w);
    cur.y += ui_dp(10.0f);

    /* Keep the declared content height honest for the scroller. */
    ui_scroll_set_content(PAGE_DASHBOARD, cur.y - content.y);
    (void)U;

    ui_scroll_end();
}
