/* page_diet.c - daily calorie target, food logging and the offline food list. */
#include "ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../core/calc.h"

#define DIET_MAX_RESULTS 64

void page_diet(Rect area)
{
    StoneApp *app = stone_app();
    UiState *U = ui_state();
    const StoneTheme *t = theme();
    Rect content, cur, card;
    float pad = ui_dp(16.0f);
    float target_kcal = stone_target_calories(&app->profile);
    float kcal = 0, prot = 0, carb = 0, fat = 0, tp, tc, tf;
    int indices[DIET_MAX_RESULTS];
    int n, i, logged_today = 0;
    char buf[96];

    stone_day_nutrition(app->today, &kcal, &prot, &carb, &fat);
    stone_macro_targets(&app->profile, target_kcal, &tp, &tc, &tf);
    n = stone_food_search(U->food_query, indices, DIET_MAX_RESULTS);
    for (i = 0; i < app->food_log_count; ++i)
        if (strcmp(app->food_log[i].date, app->today) == 0) logged_today++;

    content = ui_scroll_begin(PAGE_DIET, area,
                              ui_dp(560.0f) + (float)(n + logged_today) * ui_dp(64.0f));
    cur = rect_inset(content, pad, 0.0f);
    cur.y += ui_dp(4.0f);

    /* ---- summary -------------------------------------------------------- */
    card = ui_row(&cur, ui_dp(150.0f), ui_dp(14.0f));
    ui_card(card);
    {
        float left = target_kcal - kcal;
        snprintf(buf, sizeof(buf), "%.0f", kcal);
        render_text(buf, card.x + pad, card.y + ui_dp(18.0f), ui_dp(32.0f), t->text, 1);
        snprintf(buf, sizeof(buf), "of %.0f kcal", target_kcal);
        render_text(buf, card.x + pad + render_text_width("0000", ui_dp(32.0f), 1) + ui_dp(10.0f),
                    card.y + ui_dp(34.0f), ui_dp(12.5f), t->text_dim, 0);

        snprintf(buf, sizeof(buf), left >= 0 ? "%.0f kcal remaining" : "%.0f kcal over target",
                 fabsf(left));
        render_text(buf, card.x + pad, card.y + ui_dp(62.0f), ui_dp(12.0f),
                    left >= 0 ? t->text_faint : t->danger, 0);
        ui_progress_bar(rect_make(card.x + pad, card.y + ui_dp(84.0f),
                                  card.w - pad * 2.0f, ui_dp(9.0f)),
                        target_kcal > 0 ? kcal / target_kcal : 0.0f,
                        kcal > target_kcal * 1.05f ? t->danger : t->accent);

        snprintf(buf, sizeof(buf), "P %.0f/%.0f g    C %.0f/%.0f g    F %.0f/%.0f g",
                 prot, tp, carb, tc, fat, tf);
        render_text(buf, card.x + pad, card.y + ui_dp(106.0f), ui_dp(12.0f), t->text_dim, 0);
        snprintf(buf, sizeof(buf), "%d item(s) logged today", logged_today);
        render_text(buf, card.x + pad, card.y + ui_dp(126.0f), ui_dp(11.0f), t->text_faint, 0);
    }

    /* ---- meal selector + search ----------------------------------------- */
    {
        Rect row = ui_row(&cur, ui_dp(34.0f), ui_dp(10.0f));
        float gap = ui_dp(8.0f);
        float cw = (row.w - gap * 3.0f) / 4.0f;
        for (i = 0; i < MEAL_COUNT; ++i) {
            Rect chip = rect_make(row.x + (cw + gap) * (float)i, row.y, cw, row.h);
            if (ui_chip(200 + i, chip, stone_meal_name(i), U->selected_meal == i))
                U->selected_meal = i;
        }
    }
    {
        Rect row = ui_row(&cur, ui_dp(44.0f), ui_dp(10.0f));
        Rect search = rect_make(row.x, row.y, row.w - ui_dp(112.0f), row.h);
        Rect add = rect_make(row.x + row.w - ui_dp(104.0f), row.y, ui_dp(104.0f), row.h);

        if (ui_touch_area(210, search))
            ui_keyboard_open(KB_TEXT, "Search food", U->food_query, KB_TARGET_FOOD_SEARCH, NULL);
        render_rect(search, t->surface_alt, ui_dp(14.0f));
        render_rect_outline(search, t->border, ui_dp(14.0f), 1.0f);
        render_ring(search.x + ui_dp(20.0f), search.y + search.h * 0.5f, ui_dp(6.0f),
                    ui_dp(1.6f), t->text_faint);
        render_text(U->food_query[0] ? U->food_query : "Search food...",
                    search.x + ui_dp(36.0f),
                    search.y + (search.h - render_line_height(ui_dp(13.5f))) * 0.5f,
                    ui_dp(13.5f), U->food_query[0] ? t->text : t->text_faint, 0);
        if (U->food_query[0]) {
            Rect clear = rect_make(search.x + search.w - ui_dp(38.0f), search.y,
                                   ui_dp(38.0f), search.h);
            if (ui_touch_area(211, clear)) U->food_query[0] = '\0';
            render_text_aligned("x", clear, ui_dp(15.0f), t->text_faint, 1, TEXT_CENTER);
        }
        if (ui_button(212, add, "+ Custom", 1)) ui_navigate(PAGE_ADD_FOOD);
    }

    /* ---- today's log ---------------------------------------------------- */
    if (logged_today > 0) {
        ui_section_title(&cur, "LOGGED TODAY");
        for (i = app->food_log_count - 1; i >= 0; --i) {
            Rect row, del;
            if (strcmp(app->food_log[i].date, app->today) != 0) continue;
            row = ui_row(&cur, ui_dp(58.0f), ui_dp(8.0f));
            render_rect(row, t->surface, ui_dp(14.0f));
            render_rect(rect_make(row.x, row.y + ui_dp(14.0f), ui_dp(3.0f), row.h - ui_dp(28.0f)),
                        t->accent, ui_dp(2.0f));

            {
                char clipped[64];
                render_text_ellipsis(app->food_log[i].food_name, row.w - ui_dp(150.0f),
                                     ui_dp(13.5f), 1, clipped, sizeof(clipped));
                render_text(clipped, row.x + ui_dp(16.0f), row.y + ui_dp(12.0f),
                            ui_dp(13.5f), t->text, 1);
            }
            snprintf(buf, sizeof(buf), "%s  -  %.2gx porsi  -  P%.0f C%.0f F%.0f",
                     stone_meal_name(app->food_log[i].meal), app->food_log[i].portions,
                     app->food_log[i].protein_g, app->food_log[i].carbs_g,
                     app->food_log[i].fat_g);
            render_text(buf, row.x + ui_dp(16.0f), row.y + ui_dp(32.0f),
                        ui_dp(11.0f), t->text_faint, 0);

            snprintf(buf, sizeof(buf), "%.0f kcal", app->food_log[i].kcal);
            render_text_aligned(buf, rect_make(row.x, row.y + ui_dp(10.0f),
                                               row.w - ui_dp(50.0f), ui_dp(20.0f)),
                                ui_dp(13.0f), t->accent, 1, TEXT_RIGHT);

            del = rect_make(row.x + row.w - ui_dp(46.0f), row.y + ui_dp(12.0f),
                            ui_dp(34.0f), ui_dp(34.0f));
            if (ui_touch_area(1000 + i, del)) {
                stone_remove_food_log(i);
                stone_app_toast("Entry removed");
                break;                       /* indices shifted; redraw next frame */
            }
            render_rect(del, t->surface_alt, ui_dp(10.0f));
            render_text_aligned("x", del, ui_dp(14.0f), t->text_faint, 1, TEXT_CENTER);
        }
    }

    /* ---- food database -------------------------------------------------- */
    ui_section_title(&cur, U->food_query[0] ? "SEARCH RESULTS" : "FOOD DATABASE");
    if (n == 0) {
        ui_empty_state(ui_row(&cur, ui_dp(90.0f), ui_dp(8.0f)), "No food matched",
                       "Try another keyword or add a custom food");
    }
    for (i = 0; i < n; ++i) {
        const StoneFood *f = &app->foods[indices[i]];
        Rect row = ui_row(&cur, ui_dp(62.0f), ui_dp(8.0f));
        Rect log_btn = rect_make(row.x + row.w - ui_dp(74.0f), row.y + ui_dp(13.0f),
                                 ui_dp(62.0f), ui_dp(36.0f));
        char clipped[64];

        render_rect(row, t->surface, ui_dp(14.0f));
        render_text_ellipsis(f->name, row.w - ui_dp(110.0f), ui_dp(13.5f), 1,
                             clipped, sizeof(clipped));
        render_text(clipped, row.x + ui_dp(16.0f), row.y + ui_dp(12.0f),
                    ui_dp(13.5f), t->text, 1);
        snprintf(buf, sizeof(buf), "%.0f kcal / %.0f g  -  P%.0f C%.0f F%.0f",
                 f->kcal, f->serving_g, f->protein_g, f->carbs_g, f->fat_g);
        render_text(buf, row.x + ui_dp(16.0f), row.y + ui_dp(34.0f),
                    ui_dp(11.0f), t->text_faint, 0);
        if (f->custom)
            render_rect(rect_make(row.x + ui_dp(16.0f), row.y + ui_dp(52.0f),
                                  ui_dp(4.0f), ui_dp(4.0f)), t->primary, ui_dp(2.0f));

        if (ui_button(1200 + i, log_btn, "Log", 0)) {
            char init[16];
            U->selected_food = f->id;
            snprintf(init, sizeof(init), "1");
            ui_keyboard_open(KB_DECIMAL, "How many servings?", init, KB_TARGET_PORTIONS, NULL);
        }
        if (f->custom) {
            Rect del = rect_make(row.x + row.w - ui_dp(74.0f) - ui_dp(40.0f),
                                 row.y + ui_dp(13.0f), ui_dp(34.0f), ui_dp(36.0f));
            if (ui_touch_area(1400 + i, del)) {
                stone_delete_food(f->id);
                stone_app_toast("Custom food deleted");
                break;
            }
            render_text_aligned("x", del, ui_dp(14.0f), t->text_faint, 1, TEXT_CENTER);
        }
    }

    ui_disclaimer(&cur, cur.w);
    cur.y += ui_dp(10.0f);
    ui_scroll_set_content(PAGE_DIET, cur.y - content.y);
    ui_scroll_end();
}

/* -------------------------------------------------------- add custom food */

void page_add_food(Rect area)
{
    UiState *U = ui_state();
    const StoneTheme *t = theme();
    Rect content, cur, card;
    float pad = ui_dp(16.0f);
    char buf[64];
    int i;

    struct { const char *label; float *value; int kind; } fields[5] = {
        {"Serving size (g/ml)", &U->form_serving, KB_TARGET_FORM_SERVING},
        {"Calories (kcal)",     &U->form_kcal,    KB_TARGET_FORM_KCAL},
        {"Protein (g)",         &U->form_protein, KB_TARGET_FORM_PROTEIN},
        {"Carbs (g)",           &U->form_carbs,   KB_TARGET_FORM_CARBS},
        {"Fat (g)",             &U->form_fat,     KB_TARGET_FORM_FAT}
    };

    content = ui_scroll_begin(PAGE_ADD_FOOD, area, ui_dp(560.0f));
    cur = rect_inset(content, pad, 0.0f);
    cur.y += ui_dp(4.0f);

    card = ui_row(&cur, ui_dp(74.0f), ui_dp(12.0f));
    ui_card(card);
    render_text("Name", card.x + pad, card.y + ui_dp(14.0f), ui_dp(11.5f), t->text_faint, 0);
    if (ui_touch_area(300, card))
        ui_keyboard_open(KB_TEXT, "Food name", U->form_name, KB_TARGET_FORM_NAME, NULL);
    render_text(U->form_name[0] ? U->form_name : "Tap to type",
                card.x + pad, card.y + ui_dp(36.0f), ui_dp(15.0f),
                U->form_name[0] ? t->text : t->text_faint, 1);

    for (i = 0; i < 5; ++i) {
        Rect row = ui_row(&cur, ui_dp(64.0f), ui_dp(10.0f));
        ui_card(row);
        render_text(fields[i].label, row.x + pad, row.y + ui_dp(12.0f),
                    ui_dp(11.5f), t->text_faint, 0);
        snprintf(buf, sizeof(buf), "%.1f", *fields[i].value);
        render_text(buf, row.x + pad, row.y + ui_dp(32.0f), ui_dp(15.0f), t->text, 1);
        if (ui_touch_area(301 + i, row)) {
            char init[24];
            snprintf(init, sizeof(init), "%.1f", *fields[i].value);
            ui_keyboard_open(KB_DECIMAL, fields[i].label, init, fields[i].kind, NULL);
        }
        render_text_aligned("edit", rect_inset(row, pad, 0), ui_dp(12.0f),
                            t->primary, 1, TEXT_RIGHT);
    }

    {
        Rect row = ui_row(&cur, ui_dp(48.0f), ui_dp(12.0f));
        float gap = ui_dp(10.0f);
        float bw = (row.w - gap) * 0.5f;
        if (ui_ghost_button(310, rect_make(row.x, row.y, bw, row.h), "Cancel"))
            ui_back();
        if (ui_button(311, rect_make(row.x + bw + gap, row.y, bw, row.h), "Save food", 1)) {
            if (!U->form_name[0]) {
                stone_app_toast("Please enter a name first");
            } else if (stone_add_custom_food(U->form_name, U->form_serving, U->form_kcal,
                                             U->form_protein, U->form_carbs, U->form_fat)) {
                stone_app_toast("Food added to your database");
                U->form_name[0] = '\0';
                U->form_serving = 100.0f;
                U->form_kcal = U->form_protein = U->form_carbs = U->form_fat = 0.0f;
                ui_back();
            } else {
                stone_app_toast("Could not save the food");
            }
        }
    }

    ui_disclaimer(&cur, cur.w);
    ui_scroll_set_content(PAGE_ADD_FOOD, cur.y - content.y + ui_dp(20.0f));
    ui_scroll_end();
}
