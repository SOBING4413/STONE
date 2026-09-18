/* page_history.c - a single day's food, workouts and weight, with day paging. */
#include "ui.h"

#include <stdio.h>
#include <string.h>

#include "../core/calc.h"

void page_history(Rect area)
{
    StoneApp *app = stone_app();
    UiState *U = ui_state();
    const StoneTheme *t = theme();
    Rect content, cur, card;
    float pad = ui_dp(16.0f);
    char date[STONE_DATE_LEN];
    char buf[96];
    float kcal = 0, prot = 0, carb = 0, fat = 0;
    int i, food_rows = 0, workout_rows = 0;

    if (U->history_day_offset < 0) U->history_day_offset = 0;
    if (U->history_day_offset > 365) U->history_day_offset = 365;
    stone_days_to_date(stone_date_to_days(app->today) - U->history_day_offset,
                       date, sizeof(date));
    stone_day_nutrition(date, &kcal, &prot, &carb, &fat);

    for (i = 0; i < app->food_log_count; ++i)
        if (strcmp(app->food_log[i].date, date) == 0) food_rows++;
    for (i = 0; i < app->workout_log_count; ++i)
        if (strcmp(app->workout_log[i].date, date) == 0) workout_rows++;

    content = ui_scroll_begin(PAGE_HISTORY, area,
                              ui_dp(320.0f) + (float)(food_rows + workout_rows) * ui_dp(64.0f));
    cur = rect_inset(content, pad, 0.0f);
    cur.y += ui_dp(4.0f);

    /* ---- day picker ------------------------------------------------------ */
    card = ui_row(&cur, ui_dp(64.0f), ui_dp(12.0f));
    ui_card(card);
    {
        Rect prev = rect_make(card.x + ui_dp(10.0f), card.y + ui_dp(12.0f),
                              ui_dp(40.0f), ui_dp(40.0f));
        Rect next = rect_make(card.x + card.w - ui_dp(50.0f), card.y + ui_dp(12.0f),
                              ui_dp(40.0f), ui_dp(40.0f));
        if (ui_touch_area(700, prev)) U->history_day_offset++;
        if (ui_touch_area(701, next) && U->history_day_offset > 0) U->history_day_offset--;

        render_rect(prev, t->surface_alt, ui_dp(12.0f));
        render_text_aligned("<", prev, ui_dp(16.0f), t->text, 1, TEXT_CENTER);
        render_rect(next, t->surface_alt, ui_dp(12.0f));
        render_text_aligned(">", next, ui_dp(16.0f),
                            U->history_day_offset > 0 ? t->text : t->text_faint, 1, TEXT_CENTER);

        snprintf(buf, sizeof(buf), "%s, %s", stone_weekday(date), date);
        render_text_aligned(buf, rect_make(card.x, card.y + ui_dp(14.0f), card.w, ui_dp(20.0f)),
                            ui_dp(15.0f), t->text, 1, TEXT_CENTER);
        snprintf(buf, sizeof(buf), U->history_day_offset == 0 ? "today" : "%d day(s) ago",
                 U->history_day_offset);
        render_text_aligned(buf, rect_make(card.x, card.y + ui_dp(36.0f), card.w, ui_dp(18.0f)),
                            ui_dp(11.0f), t->text_faint, 0, TEXT_CENTER);
    }

    /* ---- day summary ----------------------------------------------------- */
    card = ui_row(&cur, ui_dp(86.0f), ui_dp(14.0f));
    ui_card(card);
    {
        float cw = (card.w - pad * 2.0f) / 3.0f;
        const char *k[3] = {"CALORIES IN", "BURNED", "WORKOUTS"};
        char v[3][24];
        snprintf(v[0], sizeof(v[0]), "%.0f", kcal);
        snprintf(v[1], sizeof(v[1]), "%d", stone_kcal_burned_on(date));
        snprintf(v[2], sizeof(v[2]), "%d", workout_rows);
        for (i = 0; i < 3; ++i) {
            float x = card.x + pad + cw * (float)i;
            render_text(k[i], x, card.y + ui_dp(18.0f), ui_dp(10.5f), t->text_faint, 1);
            render_text(v[i], x, card.y + ui_dp(38.0f), ui_dp(20.0f), t->text, 1);
        }
    }

    /* ---- weight for the day --------------------------------------------- */
    for (i = 0; i < app->weight_count; ++i) {
        if (strcmp(app->weights[i].date, date) != 0) continue;
        card = ui_row(&cur, ui_dp(52.0f), ui_dp(12.0f));
        render_rect(card, color_alpha(t->primary, 0.12f), ui_dp(14.0f));
        snprintf(buf, sizeof(buf), "Weight logged: %.1f kg", app->weights[i].weight_kg);
        render_text(buf, card.x + pad, card.y + ui_dp(16.0f), ui_dp(13.5f), t->primary, 1);
        break;
    }

    /* ---- workouts -------------------------------------------------------- */
    ui_section_title(&cur, "WORKOUTS");
    if (workout_rows == 0) {
        ui_empty_state(ui_row(&cur, ui_dp(72.0f), ui_dp(8.0f)), "No workout on this day", "");
    } else {
        for (i = app->workout_log_count - 1; i >= 0; --i) {
            Rect row, del;
            if (strcmp(app->workout_log[i].date, date) != 0) continue;
            row = ui_row(&cur, ui_dp(56.0f), ui_dp(8.0f));
            render_rect(row, t->surface, ui_dp(14.0f));
            render_text(app->workout_log[i].workout_name, row.x + ui_dp(16.0f),
                        row.y + ui_dp(11.0f), ui_dp(13.5f), t->text, 1);
            snprintf(buf, sizeof(buf), "%d min  -  %d kcal",
                     app->workout_log[i].duration_s / 60, app->workout_log[i].kcal_burned);
            render_text(buf, row.x + ui_dp(16.0f), row.y + ui_dp(31.0f),
                        ui_dp(11.0f), t->text_faint, 0);
            del = rect_make(row.x + row.w - ui_dp(46.0f), row.y + ui_dp(11.0f),
                            ui_dp(34.0f), ui_dp(34.0f));
            if (ui_touch_area(1800 + i, del)) {
                stone_remove_workout_log(i);
                stone_app_toast("Workout entry removed");
                break;
            }
            render_text_aligned("x", del, ui_dp(14.0f), t->text_faint, 1, TEXT_CENTER);
        }
    }

    /* ---- meals ----------------------------------------------------------- */
    ui_section_title(&cur, "MEALS");
    if (food_rows == 0) {
        ui_empty_state(ui_row(&cur, ui_dp(72.0f), ui_dp(8.0f)), "Nothing logged", "");
    } else {
        for (i = app->food_log_count - 1; i >= 0; --i) {
            Rect row;
            char clipped[64];
            if (strcmp(app->food_log[i].date, date) != 0) continue;
            row = ui_row(&cur, ui_dp(56.0f), ui_dp(8.0f));
            render_rect(row, t->surface, ui_dp(14.0f));
            render_text_ellipsis(app->food_log[i].food_name, row.w - ui_dp(130.0f),
                                 ui_dp(13.5f), 1, clipped, sizeof(clipped));
            render_text(clipped, row.x + ui_dp(16.0f), row.y + ui_dp(11.0f),
                        ui_dp(13.5f), t->text, 1);
            snprintf(buf, sizeof(buf), "%s  -  P%.0f C%.0f F%.0f",
                     stone_meal_name(app->food_log[i].meal), app->food_log[i].protein_g,
                     app->food_log[i].carbs_g, app->food_log[i].fat_g);
            render_text(buf, row.x + ui_dp(16.0f), row.y + ui_dp(31.0f),
                        ui_dp(11.0f), t->text_faint, 0);
            snprintf(buf, sizeof(buf), "%.0f kcal", app->food_log[i].kcal);
            render_text_aligned(buf, rect_inset(row, ui_dp(16.0f), 0), ui_dp(12.5f),
                                t->accent, 1, TEXT_RIGHT);
        }
    }

    cur.y += ui_dp(10.0f);
    ui_scroll_set_content(PAGE_HISTORY, cur.y - content.y);
    ui_scroll_end();
}
