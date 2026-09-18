/* page_profile.c - user data, preferences, backup/restore and the reset flow.
 *
 * Every edit goes through the on-screen keyboard (see ui.c) or through chips,
 * so the page never needs the Android IME and stays pure C.
 */
#include "ui.h"

#include <stdio.h>
#include <string.h>

#include "../core/calc.h"
#include "../core/achievements.h"
#include "../utils/strbuf.h"

/* Remembered so "Restore" can find the file the user just exported. */
static char g_last_backup[512];

const char *stone_profile_last_backup(void) { return g_last_backup; }

static void field_row(Rect *cur, int id, const char *label, const char *value,
                      KeyboardMode mode, const char *title, const char *initial,
                      int kind)
{
    const StoneTheme *t = theme();
    float pad = ui_dp(16.0f);
    Rect row = ui_row(cur, ui_dp(62.0f), ui_dp(8.0f));

    if (ui_touch_area(id, row))
        ui_keyboard_open(mode, title, initial, kind, NULL);

    ui_card(row);
    render_text(label, row.x + pad, row.y + ui_dp(11.0f), ui_dp(11.0f), t->text_faint, 0);
    render_text(value, row.x + pad, row.y + ui_dp(30.0f), ui_dp(15.0f), t->text, 1);
    render_text_aligned("edit", rect_inset(row, pad, 0.0f), ui_dp(12.0f),
                        t->primary, 1, TEXT_RIGHT);
}

static void chip_row(Rect *cur, int base_id, const char *label, int count,
                     const char *(*name_fn)(int), int *value)
{
    const StoneTheme *t = theme();
    Rect head = ui_row(cur, ui_dp(20.0f), ui_dp(4.0f));
    Rect row;
    float gap = ui_dp(8.0f);
    float cw;
    int i;

    render_text(label, head.x + ui_dp(2.0f), head.y, ui_dp(11.0f), t->text_faint, 0);

    if (count <= 3) {
        row = ui_row(cur, ui_dp(38.0f), ui_dp(12.0f));
        cw = (row.w - gap * (float)(count - 1)) / (float)count;
        for (i = 0; i < count; ++i) {
            Rect chip = rect_make(row.x + (cw + gap) * (float)i, row.y, cw, row.h);
            if (ui_chip(base_id + i, chip, name_fn(i), *value == i)) {
                *value = i;
                stone_save_profile();
            }
        }
    } else {
        /* Wrap into rows of at most three chips. */
        int rows = (count + 2) / 3;
        int r;
        for (r = 0; r < rows; ++r) {
            int start = r * 3;
            int n = count - start; if (n > 3) n = 3;
            row = ui_row(cur, ui_dp(38.0f), ui_dp(8.0f));
            cw = (row.w - gap * 2.0f) / 3.0f;
            for (i = 0; i < n; ++i) {
                Rect chip = rect_make(row.x + (cw + gap) * (float)i, row.y, cw, row.h);
                if (ui_chip(base_id + start + i, chip, name_fn(start + i),
                            *value == start + i)) {
                    *value = start + i;
                    stone_save_profile();
                }
            }
        }
        cur->y += ui_dp(4.0f);
    }
}

static const char *sex_name(int i) { return i == SEX_FEMALE ? "Female" : "Male"; }

void page_profile(Rect area)
{
    StoneApp *app = stone_app();
    UiState *U = ui_state();
    const StoneTheme *t = theme();
    StoneProfile *p = &app->profile;
    Rect content, cur, card;
    float pad = ui_dp(16.0f);
    char buf[128], init[32];
    float bmi = stone_bmi(p->weight_kg, p->height_cm);

    content = ui_scroll_begin(PAGE_PROFILE, area, ui_dp(1320.0f));
    cur = rect_inset(content, pad, 0.0f);
    cur.y += ui_dp(4.0f);

    /* ---- identity header ------------------------------------------------ */
    card = ui_row(&cur, ui_dp(112.0f), ui_dp(14.0f));
    ui_card_shadowed(card);
    {
        float cx = card.x + pad + ui_dp(30.0f);
        float cy = card.y + card.h * 0.5f;
        char initial[2];
        render_circle(cx, cy, ui_dp(30.0f), color_alpha(t->primary, 0.18f));
        render_ring(cx, cy, ui_dp(30.0f), ui_dp(1.6f), color_alpha(t->primary, 0.55f));
        initial[0] = p->name[0] ? p->name[0] : 'S';
        initial[1] = '\0';
        render_text_aligned(initial, rect_make(cx - ui_dp(30.0f), cy - ui_dp(14.0f),
                                               ui_dp(60.0f), ui_dp(28.0f)),
                            ui_dp(24.0f), t->primary, 1, TEXT_CENTER);

        render_text(p->name[0] ? p->name : "Set your name",
                    card.x + ui_dp(86.0f), card.y + ui_dp(26.0f), ui_dp(18.0f),
                    p->name[0] ? t->text : t->text_faint, 1);
        snprintf(buf, sizeof(buf), "%d y  -  %s  -  %.0f cm",
                 p->age, sex_name(p->sex), p->height_cm);
        render_text(buf, card.x + ui_dp(86.0f), card.y + ui_dp(52.0f),
                    ui_dp(12.0f), t->text_dim, 0);
        snprintf(buf, sizeof(buf), "BMI %.1f  -  %s", bmi, stone_bmi_category(bmi));
        render_text(buf, card.x + ui_dp(86.0f), card.y + ui_dp(72.0f),
                    ui_dp(11.5f), t->text_faint, 0);
    }

    /* ---- estimates ------------------------------------------------------ */
    card = ui_row(&cur, ui_dp(104.0f), ui_dp(14.0f));
    ui_card(card);
    {
        Rect inner = rect_inset(card, pad, ui_dp(12.0f));
        snprintf(buf, sizeof(buf), "%.0f kcal", stone_bmr(p));
        ui_kv_row(rect_make(inner.x, inner.y, inner.w, ui_dp(24.0f)),
                  "Basal metabolic rate", buf);
        snprintf(buf, sizeof(buf), "%.0f kcal", stone_tdee(p));
        ui_kv_row(rect_make(inner.x, inner.y + ui_dp(26.0f), inner.w, ui_dp(24.0f)),
                  "Daily energy estimate", buf);
        snprintf(buf, sizeof(buf), "%.0f kcal", stone_target_calories(p));
        ui_kv_row(rect_make(inner.x, inner.y + ui_dp(52.0f), inner.w, ui_dp(24.0f)),
                  "Calorie target", buf);
    }

    /* ---- editable fields ------------------------------------------------ */
    ui_section_title(&cur, "YOUR DATA");

    field_row(&cur, 400, "Name", p->name[0] ? p->name : "-",
              KB_TEXT, "Your name", p->name, KB_TARGET_NAME);

    snprintf(buf, sizeof(buf), "%d years", p->age);
    snprintf(init, sizeof(init), "%d", p->age);
    field_row(&cur, 401, "Age", buf, KB_NUMERIC, "Age (years)", init, KB_TARGET_AGE);

    snprintf(buf, sizeof(buf), "%.0f cm", p->height_cm);
    snprintf(init, sizeof(init), "%.0f", p->height_cm);
    field_row(&cur, 402, "Height", buf, KB_DECIMAL, "Height (cm)", init, KB_TARGET_HEIGHT);

    snprintf(buf, sizeof(buf), "%.1f kg", p->weight_kg);
    snprintf(init, sizeof(init), "%.1f", p->weight_kg);
    field_row(&cur, 403, "Current weight", buf, KB_DECIMAL, "Weight (kg)", init,
              KB_TARGET_WEIGHT);

    snprintf(buf, sizeof(buf), "%.1f kg", p->target_weight_kg);
    snprintf(init, sizeof(init), "%.1f", p->target_weight_kg);
    field_row(&cur, 404, "Target weight", buf, KB_DECIMAL, "Target weight (kg)", init,
              KB_TARGET_TARGET_WEIGHT);

    snprintf(buf, sizeof(buf), "%d workouts / week", p->weekly_workout_target);
    snprintf(init, sizeof(init), "%d", p->weekly_workout_target);
    field_row(&cur, 405, "Weekly workout target", buf, KB_NUMERIC,
              "Workouts per week", init, KB_TARGET_WEEKLY);

    cur.y += ui_dp(6.0f);
    chip_row(&cur, 420, "SEX (USED FOR THE BMR FORMULA)", 2, sex_name, &p->sex);
    chip_row(&cur, 430, "GOAL", GOAL_COUNT, stone_goal_name, &p->goal);
    chip_row(&cur, 440, "ACTIVITY LEVEL", ACT_COUNT, stone_activity_name, &p->activity);

    /* ---- preferences ---------------------------------------------------- */
    ui_section_title(&cur, "PREFERENCES");
    {
        Rect row = ui_row(&cur, ui_dp(58.0f), ui_dp(8.0f));
        Rect knob;
        Rect track = rect_make(row.x + row.w - ui_dp(66.0f), row.y + ui_dp(16.0f),
                               ui_dp(50.0f), ui_dp(26.0f));
        int dark = app->settings.dark_theme;
        ui_card(row);
        render_text("Dark theme", row.x + pad, row.y + ui_dp(12.0f),
                    ui_dp(13.5f), t->text, 1);
        render_text(dark ? "On - default STONE look" : "Off - light surfaces",
                    row.x + pad, row.y + ui_dp(33.0f), ui_dp(11.0f), t->text_faint, 0);
        if (ui_touch_area(450, row)) {
            app->settings.dark_theme = !dark;
            stone_app_mark_dirty(DIRTY_SETTINGS);
            stone_app_toast(app->settings.dark_theme ? "Dark theme on" : "Light theme on");
        }
        render_rect(track, dark ? t->primary : t->surface_alt, track.h * 0.5f);
        knob = rect_make(dark ? track.x + track.w - ui_dp(23.0f) : track.x + ui_dp(3.0f),
                         track.y + ui_dp(3.0f), ui_dp(20.0f), ui_dp(20.0f));
        render_rect(knob, dark ? t->bg : t->text_dim, knob.h * 0.5f);
    }
    {
        Rect row = ui_row(&cur, ui_dp(58.0f), ui_dp(8.0f));
        Rect track = rect_make(row.x + row.w - ui_dp(66.0f), row.y + ui_dp(16.0f),
                               ui_dp(50.0f), ui_dp(26.0f));
        Rect knob;
        int on = app->settings.animations_enabled;
        ui_card(row);
        render_text("Animations", row.x + pad, row.y + ui_dp(12.0f),
                    ui_dp(13.5f), t->text, 1);
        render_text(on ? "Page and button transitions enabled"
                       : "Disabled for maximum performance",
                    row.x + pad, row.y + ui_dp(33.0f), ui_dp(11.0f), t->text_faint, 0);
        if (ui_touch_area(451, row)) {
            app->settings.animations_enabled = !on;
            stone_app_mark_dirty(DIRTY_SETTINGS);
        }
        render_rect(track, on ? t->primary : t->surface_alt, track.h * 0.5f);
        knob = rect_make(on ? track.x + track.w - ui_dp(23.0f) : track.x + ui_dp(3.0f),
                         track.y + ui_dp(3.0f), ui_dp(20.0f), ui_dp(20.0f));
        render_rect(knob, on ? t->bg : t->text_dim, knob.h * 0.5f);
    }

    /* ---- data ----------------------------------------------------------- */
    ui_section_title(&cur, "DATA");
    {
        Rect row = ui_row(&cur, ui_dp(46.0f), ui_dp(10.0f));
        float gap = ui_dp(10.0f);
        float bw = (row.w - gap) * 0.5f;
        if (ui_button(460, rect_make(row.x, row.y, bw, row.h), "Backup JSON", 1)) {
            char path[512];
            if (stone_backup_export(path, sizeof(path))) {
                stone_strlcpy(g_last_backup, path, sizeof(g_last_backup));
                stone_app_toast("Backup saved: %s", path);
            }
        }
        if (ui_ghost_button(461, rect_make(row.x + bw + gap, row.y, bw, row.h),
                            "Restore JSON")) {
            char path[512];
            if (g_last_backup[0]) {
                stone_strlcpy(path, g_last_backup, sizeof(path));
            } else {
                char today[STONE_DATE_LEN];
                stone_today_string(today, sizeof(today));
                snprintf(path, sizeof(path), "stone_backup_%s.json", today);
            }
            if (stone_backup_import(path))
                stone_app_toast("Data restored from backup");
        }
    }
    {
        Rect note = ui_row(&cur, ui_dp(52.0f), ui_dp(10.0f));
        render_rect(note, t->surface_alt, ui_dp(12.0f));
        render_text_wrapped("Backups are plain JSON files written to this device only. "
                            "Copy one back into the app folder to restore it.",
                            rect_inset(note, ui_dp(12.0f), ui_dp(9.0f)),
                            ui_dp(11.0f), t->text_faint, 0, 1);
    }
    {
        Rect row = ui_row(&cur, ui_dp(46.0f), ui_dp(10.0f));
        if (!U->confirm_reset) {
            if (ui_danger_button(470, row, "Reset all data")) U->confirm_reset = 1;
        } else {
            float gap = ui_dp(10.0f);
            float bw = (row.w - gap) * 0.5f;
            if (ui_ghost_button(471, rect_make(row.x, row.y, bw, row.h), "Cancel"))
                U->confirm_reset = 0;
            if (ui_danger_button(472, rect_make(row.x + bw + gap, row.y, bw, row.h),
                                 "Yes, erase")) {
                stone_reset_all_data();
                U->confirm_reset = 0;
                U->selected_food = 0;
                U->food_query[0] = '\0';
                ui_navigate(PAGE_DASHBOARD);
            }
        }
    }
    if (U->confirm_reset) {
        Rect warn = ui_row(&cur, ui_dp(34.0f), ui_dp(6.0f));
        render_text_wrapped("This deletes your profile, logs and progress permanently.",
                            warn, ui_dp(11.0f), t->danger, 0, 1);
    }

    /* ---- shortcuts ------------------------------------------------------ */
    ui_section_title(&cur, "MORE");
    {
        Rect row = ui_row(&cur, ui_dp(62.0f), ui_dp(10.0f));
        char sub[64];
        int got = stone_achievements_unlocked_count();

        if (ui_touch_area(482, row)) ui_navigate(PAGE_ACHIEVEMENTS);
        ui_card(row);
        ui_badge(rect_make(row.x + ui_dp(10.0f), row.y + ui_dp(7.0f),
                           ui_dp(48.0f), ui_dp(48.0f)),
                 stone_achievement_next() >= 0
                     ? stone_achievement(stone_achievement_next())->tile : 13,
                 got > 0);
        render_text("Achievements", row.x + ui_dp(66.0f), row.y + ui_dp(13.0f),
                    ui_dp(14.0f), t->text, 1);
        snprintf(sub, sizeof(sub), "%d of %d badges unlocked",
                 got, STONE_ACHIEVEMENT_COUNT);
        render_text(sub, row.x + ui_dp(66.0f), row.y + ui_dp(34.0f),
                    ui_dp(11.0f), t->text_faint, 0);
        render_text_aligned(">", rect_inset(row, ui_dp(16.0f), 0.0f),
                            ui_dp(16.0f), t->text_faint, 1, TEXT_RIGHT);
    }
    {
        Rect row = ui_row(&cur, ui_dp(46.0f), ui_dp(10.0f));
        float gap = ui_dp(10.0f);
        float bw = (row.w - gap) * 0.5f;
        if (ui_ghost_button(480, rect_make(row.x, row.y, bw, row.h), "History"))
            ui_navigate(PAGE_HISTORY);
        if (ui_ghost_button(481, rect_make(row.x + bw + gap, row.y, bw, row.h),
                            "About STONE"))
            ui_navigate(PAGE_ABOUT);
    }

    ui_disclaimer(&cur, cur.w);
    cur.y += ui_dp(10.0f);
    ui_scroll_set_content(PAGE_PROFILE, cur.y - content.y);
    ui_scroll_end();
}
