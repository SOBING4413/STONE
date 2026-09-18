/* page_workout.c - program list, program detail and the guided session timer. */
#include "ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../core/calc.h"

static Color level_color(int level)
{
    const StoneTheme *t = theme();
    if (level == LEVEL_BEGINNER) return t->primary;
    if (level == LEVEL_INTERMEDIATE) return t->warn;
    return t->accent;
}

void page_workout(Rect area)
{
    StoneApp *app = stone_app();
    UiState *U = ui_state();
    const StoneTheme *t = theme();
    Rect content, cur;
    float pad = ui_dp(16.0f);
    int i, shown = 0;
    char buf[96];

    content = ui_scroll_begin(PAGE_WORKOUT, area,
                              ui_dp(220.0f) + (float)app->workout_count * ui_dp(104.0f));
    cur = rect_inset(content, pad, 0.0f);
    cur.y += ui_dp(4.0f);

    /* ---- weekly target -------------------------------------------------- */
    {
        Rect card = ui_row(&cur, ui_dp(92.0f), ui_dp(14.0f));
        int week = stone_workouts_this_week();
        int target = app->profile.weekly_workout_target;
        ui_card(card);
        render_text("WEEKLY TARGET", card.x + pad, card.y + ui_dp(14.0f),
                    ui_dp(11.0f), t->text_faint, 1);
        snprintf(buf, sizeof(buf), "%d of %d workouts done", week, target);
        render_text(buf, card.x + pad, card.y + ui_dp(32.0f), ui_dp(17.0f), t->text, 1);
        ui_progress_bar(rect_make(card.x + pad, card.y + ui_dp(62.0f),
                                  card.w - pad * 2.0f, ui_dp(8.0f)),
                        target > 0 ? (float)week / (float)target : 0.0f, t->primary);
    }

    /* ---- filters -------------------------------------------------------- */
    {
        Rect row = ui_row(&cur, ui_dp(32.0f), ui_dp(8.0f));
        float gap = ui_dp(8.0f);
        float cw = (row.w - gap * 3.0f) / 4.0f;
        if (ui_chip(400, rect_make(row.x, row.y, cw, row.h), "All", U->selected_level < 0))
            U->selected_level = -1;
        for (i = 0; i < LEVEL_COUNT; ++i) {
            Rect c = rect_make(row.x + (cw + gap) * (float)(i + 1), row.y, cw, row.h);
            if (ui_chip(401 + i, c, stone_level_name(i), U->selected_level == i))
                U->selected_level = (U->selected_level == i) ? -1 : i;
        }

        row = ui_row(&cur, ui_dp(32.0f), ui_dp(12.0f));
        cw = (row.w - gap * 3.0f) / 4.0f;
        {
            static const int cats[4] = {CAT_FULL_BODY, CAT_UPPER_BODY, CAT_LOWER_BODY, CAT_CORE};
            for (i = 0; i < 4; ++i) {
                Rect c = rect_make(row.x + (cw + gap) * (float)i, row.y, cw, row.h);
                if (ui_chip(410 + i, c, stone_category_name(cats[i]),
                            U->selected_category == cats[i]))
                    U->selected_category = (U->selected_category == cats[i]) ? -1 : cats[i];
            }
        }
        row = ui_row(&cur, ui_dp(32.0f), ui_dp(14.0f));
        cw = (row.w - gap * 3.0f) / 4.0f;
        {
            static const int cats[2] = {CAT_STRENGTH, CAT_CARDIO};
            for (i = 0; i < 2; ++i) {
                Rect c = rect_make(row.x + (cw + gap) * (float)i, row.y, cw, row.h);
                if (ui_chip(420 + i, c, stone_category_name(cats[i]),
                            U->selected_category == cats[i]))
                    U->selected_category = (U->selected_category == cats[i]) ? -1 : cats[i];
            }
        }
    }

    /* ---- program cards -------------------------------------------------- */
    for (i = 0; i < app->workout_count; ++i) {
        const StoneWorkout *w = &app->workouts[i];
        Rect card;
        char clipped[64];

        if (U->selected_level >= 0 && w->level != U->selected_level) continue;
        if (U->selected_category >= 0 && w->category != U->selected_category) continue;

        card = ui_row(&cur, ui_dp(96.0f), ui_dp(10.0f));
        shown++;

        if (ui_touch_area(1600 + i, card)) {
            U->selected_workout = w->id;
            ui_navigate(PAGE_WORKOUT_DETAIL);
        }
        ui_card(card);
        render_rect(rect_make(card.x, card.y + ui_dp(16.0f), ui_dp(4.0f), card.h - ui_dp(32.0f)),
                    level_color(w->level), ui_dp(2.0f));

        render_text_ellipsis(w->name, card.w - ui_dp(120.0f), ui_dp(15.0f), 1,
                             clipped, sizeof(clipped));
        render_text(clipped, card.x + ui_dp(18.0f), card.y + ui_dp(14.0f),
                    ui_dp(15.0f), t->text, 1);

        render_text_ellipsis(w->description, card.w - ui_dp(46.0f), ui_dp(11.5f), 0,
                             clipped, sizeof(clipped));
        render_text(clipped, card.x + ui_dp(18.0f), card.y + ui_dp(36.0f),
                    ui_dp(11.5f), t->text_faint, 0);

        snprintf(buf, sizeof(buf), "%s  -  %s  -  %d min  -  %d moves",
                 stone_level_name(w->level), stone_category_name(w->category),
                 w->est_minutes, w->exercise_count);
        render_text(buf, card.x + ui_dp(18.0f), card.y + ui_dp(62.0f),
                    ui_dp(11.0f), t->text_dim, 0);

        {
            Rect pill = rect_make(card.x + card.w - ui_dp(92.0f), card.y + ui_dp(14.0f),
                                  ui_dp(78.0f), ui_dp(24.0f));
            render_rect(pill, color_alpha(level_color(w->level), 0.16f), pill.h * 0.5f);
            render_text_aligned(stone_level_name(w->level), pill, ui_dp(10.5f),
                                level_color(w->level), 1, TEXT_CENTER);
        }
    }

    if (shown == 0)
        ui_empty_state(ui_row(&cur, ui_dp(100.0f), ui_dp(10.0f)), "No program matches",
                       "Clear a filter to see more programs");

    cur.y += ui_dp(10.0f);
    ui_scroll_set_content(PAGE_WORKOUT, cur.y - content.y);
    ui_scroll_end();
}

/* ------------------------------------------------------------- detail page */

void page_workout_detail(Rect area)
{
    UiState *U = ui_state();
    const StoneTheme *t = theme();
    const StoneWorkout *w = stone_find_workout(U->selected_workout);
    Rect content, cur, card;
    float pad = ui_dp(16.0f);
    int i;
    char buf[96];

    if (!w) {
        ui_empty_state(rect_inset(area, ui_dp(20.0f), ui_dp(20.0f)),
                       "Program not found", "Go back and pick another program");
        return;
    }

    content = ui_scroll_begin(PAGE_WORKOUT_DETAIL, area,
                              ui_dp(300.0f) + (float)w->exercise_count * ui_dp(120.0f));
    cur = rect_inset(content, pad, 0.0f);
    cur.y += ui_dp(4.0f);

    card = ui_row(&cur, ui_dp(150.0f), ui_dp(14.0f));
    render_rect_gradient(card, color_mix(t->surface, level_color(w->level), 0.14f),
                         t->surface, ui_dp(20.0f));
    render_rect_outline(card, t->border, ui_dp(20.0f), 1.0f);
    render_text(w->name, card.x + pad, card.y + ui_dp(16.0f), ui_dp(20.0f), t->text, 1);
    render_text_wrapped(w->description,
                        rect_make(card.x + pad, card.y + ui_dp(44.0f),
                                  card.w - pad * 2.0f, ui_dp(40.0f)),
                        ui_dp(12.0f), t->text_dim, 0, 1);
    snprintf(buf, sizeof(buf), "%s  -  %s  -  approx %d min  -  ~%.0f kcal",
             stone_level_name(w->level), stone_category_name(w->category),
             w->est_minutes, w->kcal_per_min * (float)w->est_minutes);
    render_text(buf, card.x + pad, card.y + ui_dp(112.0f), ui_dp(11.5f), t->text_faint, 0);

    {
        Rect row = ui_row(&cur, ui_dp(50.0f), ui_dp(14.0f));
        float gap = ui_dp(10.0f);
        float bw = (row.w - gap) * 0.5f;
        if (ui_button(500, rect_make(row.x, row.y, bw, row.h), "Start session", 1)) {
            U->session_active = 1;
            U->session_exercise = 0;
            U->session_set = 1;
            U->session_resting = 0;
            U->session_elapsed = 0.0f;
            U->session_timer = (float)w->exercises[0].duration_s;
            ui_navigate(PAGE_WORKOUT_SESSION);
        }
        if (ui_button(501, rect_make(row.x + bw + gap, row.y, bw, row.h), "Log as done", 0)) {
            if (stone_log_workout(w->id, w->est_minutes * 60))
                stone_app_toast("Workout logged");
        }
    }

    ui_section_title(&cur, "EXERCISES");
    for (i = 0; i < w->exercise_count; ++i) {
        const StoneExercise *x = &w->exercises[i];
        Rect row;
        float h;

        h = render_text_wrapped(x->instruction,
                                rect_make(0, 0, cur.w - ui_dp(76.0f), 0),
                                ui_dp(11.5f), t->text_faint, 0, 0);
        row = ui_row(&cur, ui_dp(74.0f) + h, ui_dp(10.0f));
        ui_card(row);

        render_circle(row.x + ui_dp(28.0f), row.y + ui_dp(30.0f), ui_dp(14.0f),
                      color_alpha(level_color(w->level), 0.18f));
        snprintf(buf, sizeof(buf), "%d", i + 1);
        render_text_aligned(buf, rect_make(row.x + ui_dp(14.0f), row.y + ui_dp(19.0f),
                                           ui_dp(28.0f), ui_dp(22.0f)),
                            ui_dp(13.0f), level_color(w->level), 1, TEXT_CENTER);

        render_text(x->name, row.x + ui_dp(52.0f), row.y + ui_dp(14.0f),
                    ui_dp(14.0f), t->text, 1);
        if (x->duration_s > 0)
            snprintf(buf, sizeof(buf), "%d set  -  %d s  -  rest %d s",
                     x->sets, x->duration_s, x->rest_s);
        else
            snprintf(buf, sizeof(buf), "%d set  -  %d reps  -  rest %d s",
                     x->sets, x->reps, x->rest_s);
        render_text(buf, row.x + ui_dp(52.0f), row.y + ui_dp(34.0f),
                    ui_dp(11.5f), t->primary, 0);
        render_text_wrapped(x->instruction,
                            rect_make(row.x + ui_dp(52.0f), row.y + ui_dp(54.0f),
                                      row.w - ui_dp(76.0f), h),
                            ui_dp(11.5f), t->text_faint, 0, 1);
    }

    ui_disclaimer(&cur, cur.w);
    cur.y += ui_dp(10.0f);
    ui_scroll_set_content(PAGE_WORKOUT_DETAIL, cur.y - content.y);
    ui_scroll_end();
}

/* ------------------------------------------------------------ session page */

void page_workout_session(Rect area)
{
    UiState *U = ui_state();
    const StoneTheme *t = theme();
    const StoneWorkout *w = stone_find_workout(U->selected_workout);
    const StoneExercise *x;
    Rect cur = rect_inset(area, ui_dp(16.0f), 0.0f);
    Rect card;
    char buf[96];
    float total_sets, done_sets;
    int i;

    if (!w || w->exercise_count == 0) {
        ui_empty_state(rect_inset(area, ui_dp(20.0f), ui_dp(20.0f)),
                       "Session unavailable", "Pick a program first");
        return;
    }

    if (U->session_exercise >= w->exercise_count) U->session_exercise = w->exercise_count - 1;
    x = &w->exercises[U->session_exercise];

    U->session_elapsed += U->dt;
    if (U->session_timer > 0.0f) {
        U->session_timer -= U->dt;
        if (U->session_timer <= 0.0f) {
            U->session_timer = 0.0f;
            stone_app_toast(U->session_resting ? "Rest over - next set" : "Time!");
        }
    }

    total_sets = 0.0f;
    for (i = 0; i < w->exercise_count; ++i) total_sets += (float)w->exercises[i].sets;
    done_sets = 0.0f;
    for (i = 0; i < U->session_exercise; ++i) done_sets += (float)w->exercises[i].sets;
    done_sets += (float)(U->session_set - 1);

    cur.y += ui_dp(4.0f);

    /* progress header */
    card = ui_row(&cur, ui_dp(70.0f), ui_dp(12.0f));
    ui_card(card);
    snprintf(buf, sizeof(buf), "Exercise %d of %d  -  set %d of %d",
             U->session_exercise + 1, w->exercise_count, U->session_set, x->sets);
    render_text(buf, card.x + ui_dp(16.0f), card.y + ui_dp(14.0f),
                ui_dp(12.5f), t->text_dim, 0);
    ui_progress_bar(rect_make(card.x + ui_dp(16.0f), card.y + ui_dp(40.0f),
                              card.w - ui_dp(32.0f), ui_dp(8.0f)),
                    total_sets > 0 ? done_sets / total_sets : 0.0f, t->primary);

    /* timer ring */
    card = ui_row(&cur, ui_dp(250.0f), ui_dp(12.0f));
    ui_card(card);
    {
        float cx = card.x + card.w * 0.5f;
        float cy = card.y + ui_dp(112.0f);
        float radius = ui_dp(82.0f);
        float span = U->session_resting ? (float)x->rest_s
                                        : (x->duration_s > 0 ? (float)x->duration_s : 1.0f);
        float tprog = span > 0 ? 1.0f - (U->session_timer / span) : 0.0f;
        Color ring = U->session_resting ? t->warn : t->primary;

        render_ring(cx, cy, radius, ui_dp(11.0f), t->surface_alt);
        if (U->session_timer > 0.0f) render_arc(cx, cy, radius, ui_dp(11.0f), tprog, ring);

        if (U->session_resting || x->duration_s > 0) {
            int secs = (int)(U->session_timer + 0.5f);
            snprintf(buf, sizeof(buf), "%02d:%02d", secs / 60, secs % 60);
        } else {
            snprintf(buf, sizeof(buf), "%d", x->reps);
        }
        render_text_aligned(buf, rect_make(cx - ui_dp(90.0f), cy - ui_dp(24.0f),
                                           ui_dp(180.0f), ui_dp(40.0f)),
                            ui_dp(38.0f), t->text, 1, TEXT_CENTER);
        render_text_aligned(U->session_resting ? "REST" :
                            (x->duration_s > 0 ? "SECONDS" : "REPS"),
                            rect_make(cx - ui_dp(90.0f), cy + ui_dp(22.0f),
                                      ui_dp(180.0f), ui_dp(18.0f)),
                            ui_dp(11.0f), t->text_faint, 1, TEXT_CENTER);

        render_text_aligned(x->name, rect_make(card.x, card.y + ui_dp(206.0f),
                                               card.w, ui_dp(22.0f)),
                            ui_dp(16.0f), t->text, 1, TEXT_CENTER);
    }

    /* instruction */
    {
        float h = render_text_wrapped(x->instruction,
                                      rect_make(0, 0, cur.w - ui_dp(32.0f), 0),
                                      ui_dp(12.0f), t->text_faint, 0, 0);
        card = ui_row(&cur, h + ui_dp(28.0f), ui_dp(12.0f));
        render_rect(card, t->surface_alt, ui_dp(14.0f));
        render_text_wrapped(x->instruction,
                            rect_make(card.x + ui_dp(16.0f), card.y + ui_dp(14.0f),
                                      card.w - ui_dp(32.0f), h),
                            ui_dp(12.0f), t->text_dim, 0, 1);
    }

    /* controls */
    {
        Rect row = ui_row(&cur, ui_dp(50.0f), ui_dp(10.0f));
        float gap = ui_dp(10.0f);
        float bw = (row.w - gap * 2.0f) / 3.0f;

        if (ui_button(520, rect_make(row.x, row.y, bw, row.h), "Prev", 0)) {
            if (U->session_set > 1) U->session_set--;
            else if (U->session_exercise > 0) {
                U->session_exercise--;
                U->session_set = w->exercises[U->session_exercise].sets;
            }
            U->session_resting = 0;
            U->session_timer = (float)w->exercises[U->session_exercise].duration_s;
        }
        if (ui_button(521, rect_make(row.x + bw + gap, row.y, bw, row.h),
                      U->session_timer > 0.0f ? "Skip timer" : "Start timer", 0)) {
            if (U->session_timer > 0.0f) U->session_timer = 0.0f;
            else U->session_timer = U->session_resting ? (float)x->rest_s
                                                       : (float)(x->duration_s > 0 ? x->duration_s : 30);
        }
        if (ui_button(522, rect_make(row.x + (bw + gap) * 2.0f, row.y, bw, row.h),
                      U->session_resting ? "Next set" : "Done set", 1)) {
            if (!U->session_resting) {
                U->session_resting = 1;
                U->session_timer = (float)x->rest_s;
            } else {
                U->session_resting = 0;
                if (U->session_set < x->sets) {
                    U->session_set++;
                } else if (U->session_exercise + 1 < w->exercise_count) {
                    U->session_exercise++;
                    U->session_set = 1;
                } else {
                    stone_log_workout(w->id, (int)U->session_elapsed);
                    U->session_active = 0;
                    stone_app_toast("Session complete - nice work");
                    ui_back();
                    return;
                }
                U->session_timer = (float)w->exercises[U->session_exercise].duration_s;
            }
        }

        row = ui_row(&cur, ui_dp(46.0f), ui_dp(10.0f));
        {
            int secs = (int)U->session_elapsed;
            snprintf(buf, sizeof(buf), "Elapsed %02d:%02d", secs / 60, secs % 60);
            render_text(buf, row.x + ui_dp(4.0f),
                        row.y + (row.h - render_line_height(ui_dp(12.5f))) * 0.5f,
                        ui_dp(12.5f), t->text_faint, 0);
        }
        if (ui_danger_button(523, rect_make(row.x + row.w - ui_dp(150.0f), row.y,
                                            ui_dp(150.0f), row.h), "Finish & save")) {
            stone_log_workout(w->id, (int)U->session_elapsed);
            U->session_active = 0;
            stone_app_toast("Workout saved");
            ui_back();
        }
    }
}
