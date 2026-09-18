/* page_onboarding.c - the first-run setup wizard.
 *
 * Shown once, when profile.configured is still 0. It collects only the values
 * the calorie and macro maths actually needs, and it reuses the same on-screen
 * keyboard and the same commit path (ui_input.c) as the profile page, so
 * there is exactly one place where a typed number is validated.
 *
 * The wizard owns the whole screen: ui_frame() draws no header and no tab bar
 * while PAGE_ONBOARDING is active, so there is nothing to wander off into
 * half-configured.
 */
#include "ui.h"

#include <stdio.h>
#include <string.h>

#include "../core/calc.h"

#define ONBOARD_STEPS 4

static const char *sex_name(int i) { return i == SEX_FEMALE ? "Female" : "Male"; }

/* One tappable value row; identical in behaviour to the profile page's, kept
   local so the two pages can drift apart visually without fighting. */
static void wiz_field(Rect *cur, int id, const char *label, const char *value,
                      KeyboardMode mode, const char *title, const char *initial,
                      int kind)
{
    const StoneTheme *t = theme();
    float pad = ui_dp(15.0f);
    Rect row = ui_row(cur, ui_dp(60.0f), ui_dp(9.0f));

    if (ui_touch_area(id, row))
        ui_keyboard_open(mode, title, initial, kind, NULL);

    render_rect(row, t->surface, ui_dp(15.0f));
    render_rect_outline(row, t->border, ui_dp(15.0f), 1.0f);
    render_text(label, row.x + pad, row.y + ui_dp(11.0f), ui_dp(10.5f), t->text_faint, 0);
    render_text(value, row.x + pad, row.y + ui_dp(29.0f), ui_dp(15.0f), t->text, 1);
    render_text_aligned("edit", rect_inset(row, pad, 0.0f), ui_dp(11.5f),
                        t->primary, 1, TEXT_RIGHT);
}

static void wiz_chips(Rect *cur, int base_id, const char *label, int count,
                      const char *(*name_fn)(int), int *value)
{
    const StoneTheme *t = theme();
    Rect head = ui_row(cur, ui_dp(18.0f), ui_dp(5.0f));
    float gap = ui_dp(8.0f);
    int per_row = (count <= 3) ? count : 3;
    int r, i, rows = (count + per_row - 1) / per_row;

    render_text(label, head.x + ui_dp(2.0f), head.y, ui_dp(10.5f), t->text_faint, 0);

    for (r = 0; r < rows; ++r) {
        int start = r * per_row;
        int n = count - start;
        Rect row;
        float cw;
        if (n > per_row) n = per_row;
        row = ui_row(cur, ui_dp(38.0f), ui_dp(8.0f));
        cw = (row.w - gap * (float)(per_row - 1)) / (float)per_row;
        for (i = 0; i < n; ++i) {
            Rect chip = rect_make(row.x + (cw + gap) * (float)i, row.y, cw, row.h);
            if (ui_chip(base_id + start + i, chip, name_fn(start + i),
                        *value == start + i))
                *value = start + i;
        }
    }
}

static void wiz_dots(Rect r, int step)
{
    const StoneTheme *t = theme();
    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;
    float gap = ui_dp(16.0f);
    int i;

    for (i = 0; i < ONBOARD_STEPS; ++i) {
        float x = cx + ((float)i - (float)(ONBOARD_STEPS - 1) * 0.5f) * gap;
        int on = (i <= step);
        if (i == step)
            render_rect(rect_make(x - ui_dp(9.0f), cy - ui_dp(3.0f),
                                  ui_dp(18.0f), ui_dp(6.0f)), t->primary, ui_dp(3.0f));
        else
            render_circle(x, cy, ui_dp(3.0f),
                          on ? color_alpha(t->primary, 0.55f) : t->text_faint);
    }
}

void page_onboarding(Rect area)
{
    StoneApp *app = stone_app();
    UiState *U = ui_state();
    const StoneTheme *t = theme();
    StoneProfile *p = &app->profile;
    Rect cur, art, footer, row;
    float pad = ui_dp(18.0f);
    char buf[96], init[32];
    int step = U->onboard_step;

    if (step < 0) step = 0;
    if (step >= ONBOARD_STEPS) step = ONBOARD_STEPS - 1;

    U->onboard_anim += (1.0f - U->onboard_anim) * 0.18f;

    cur = rect_inset(area, pad, 0.0f);
    cur.y += ui_dp(8.0f);

    /* ---- artwork header -------------------------------------------------- */
    {
        const char *art_name = (step == 0) ? "onboard1"
                             : (step == 3) ? "onboard3" : "onboard2";
        float art_h = (step == 0 || step == 3) ? area.h * 0.36f : area.h * 0.19f;
        if (art_h < ui_dp(96.0f)) art_h = ui_dp(96.0f);
        art = ui_row(&cur, art_h, ui_dp(14.0f));
        render_image_cover(art_name, art, color_rgba(1.0f, 1.0f, 1.0f, 1.0f), ui_dp(20.0f));
        if (!render_image_available(art_name))
            ui_hero_band(art, step, 1.0f);
        render_rect_outline(art, t->border, ui_dp(20.0f), 1.0f);
    }

    /* ---- step body ------------------------------------------------------- */
    switch (step) {
    case 0:
        render_text(STONE_APP_NAME, cur.x, cur.y, ui_dp(30.0f), t->text, 1);
        cur.y += ui_dp(38.0f);
        render_text("Train offline. Own your data.", cur.x, cur.y,
                    ui_dp(14.0f), t->primary, 1);
        cur.y += ui_dp(28.0f);
        {
            Rect box = rect_make(cur.x, cur.y, cur.w, ui_dp(120.0f));
            float h = render_text_wrapped(
                "Workouts, meals and weight, tracked entirely on this phone. "
                "No account, no sync, no network permission at all. Set a few "
                "numbers up now and every target in the app is calculated for "
                "you.", box, ui_dp(13.0f), t->text_dim, 0, 1);
            cur.y += h + ui_dp(10.0f);
        }
        break;

    case 1:
        render_text("About you", cur.x, cur.y, ui_dp(22.0f), t->text, 1);
        cur.y += ui_dp(30.0f);
        render_text("Used for the Mifflin-St Jeor energy estimate.",
                    cur.x, cur.y, ui_dp(11.5f), t->text_faint, 0);
        cur.y += ui_dp(20.0f);

        wiz_field(&cur, 1700, "Name", p->name[0] ? p->name : "-",
                  KB_TEXT, "Your name", p->name, KB_TARGET_NAME);

        snprintf(buf, sizeof(buf), "%d years", p->age);
        snprintf(init, sizeof(init), "%d", p->age);
        wiz_field(&cur, 1701, "Age", buf, KB_NUMERIC, "Age (years)", init,
                  KB_TARGET_AGE);

        snprintf(buf, sizeof(buf), "%.0f cm", p->height_cm);
        snprintf(init, sizeof(init), "%.0f", p->height_cm);
        wiz_field(&cur, 1702, "Height", buf, KB_DECIMAL, "Height (cm)", init,
                  KB_TARGET_HEIGHT);

        wiz_chips(&cur, 1710, "SEX", 2, sex_name, &p->sex);
        break;

    case 2:
        render_text("Your goal", cur.x, cur.y, ui_dp(22.0f), t->text, 1);
        cur.y += ui_dp(30.0f);

        snprintf(buf, sizeof(buf), "%.1f kg", p->weight_kg);
        snprintf(init, sizeof(init), "%.1f", p->weight_kg);
        wiz_field(&cur, 1720, "Current weight", buf, KB_DECIMAL, "Weight (kg)",
                  init, KB_TARGET_WEIGHT);

        snprintf(buf, sizeof(buf), "%.1f kg", p->target_weight_kg);
        snprintf(init, sizeof(init), "%.1f", p->target_weight_kg);
        wiz_field(&cur, 1721, "Target weight", buf, KB_DECIMAL,
                  "Target weight (kg)", init, KB_TARGET_TARGET_WEIGHT);

        wiz_chips(&cur, 1730, "GOAL", GOAL_COUNT, stone_goal_name, &p->goal);
        wiz_chips(&cur, 1740, "ACTIVITY LEVEL", ACT_COUNT, stone_activity_name,
                  &p->activity);
        break;

    default:
        render_text("You are set", cur.x, cur.y, ui_dp(22.0f), t->text, 1);
        cur.y += ui_dp(32.0f);
        {
            Rect card = ui_row(&cur, ui_dp(122.0f), ui_dp(12.0f));
            Rect inner = rect_inset(card, ui_dp(16.0f), ui_dp(14.0f));
            ui_card(card);
            snprintf(buf, sizeof(buf), "%.0f kcal", stone_bmr(p));
            ui_kv_row(rect_make(inner.x, inner.y, inner.w, ui_dp(24.0f)),
                      "Basal metabolic rate", buf);
            snprintf(buf, sizeof(buf), "%.0f kcal", stone_tdee(p));
            ui_kv_row(rect_make(inner.x, inner.y + ui_dp(30.0f), inner.w, ui_dp(24.0f)),
                      "Daily energy estimate", buf);
            snprintf(buf, sizeof(buf), "%.0f kcal", stone_target_calories(p));
            ui_kv_row(rect_make(inner.x, inner.y + ui_dp(60.0f), inner.w, ui_dp(24.0f)),
                      "Your calorie target", buf);
        }

        snprintf(buf, sizeof(buf), "%d workouts / week", p->weekly_workout_target);
        snprintf(init, sizeof(init), "%d", p->weekly_workout_target);
        wiz_field(&cur, 1750, "Weekly workout target", buf, KB_NUMERIC,
                  "Workouts per week", init, KB_TARGET_WEEKLY);

        render_text_wrapped("Everything above stays editable later on the "
                            "Profile tab.",
                            rect_make(cur.x, cur.y + ui_dp(4.0f), cur.w, ui_dp(40.0f)),
                            ui_dp(11.0f), t->text_faint, 0, 1);
        break;
    }

    /* ---- footer ---------------------------------------------------------- */
    footer = rect_make(area.x + pad, area.y + area.h - ui_dp(96.0f),
                       area.w - pad * 2.0f, ui_dp(88.0f));

    wiz_dots(rect_make(footer.x, footer.y, footer.w, ui_dp(20.0f)), step);

    row = rect_make(footer.x, footer.y + ui_dp(26.0f), footer.w, ui_dp(50.0f));
    if (step == 0) {
        float gap = ui_dp(10.0f);
        float bw = (row.w - gap) * 0.5f;
        if (ui_ghost_button(1760, rect_make(row.x, row.y, bw, row.h), "Skip setup")) {
            stone_save_profile();
            U->page = PAGE_DASHBOARD;
            U->page_anim = 0.0f;
            stone_app_toast("You can finish this any time on Profile");
        }
        if (ui_button(1761, rect_make(row.x + bw + gap, row.y, bw, row.h),
                      "Get started", 1)) {
            U->onboard_step = 1;
            U->onboard_anim = 0.0f;
        }
    } else {
        float gap = ui_dp(10.0f);
        float bw = (row.w - gap) * 0.5f;
        if (ui_ghost_button(1762, rect_make(row.x, row.y, bw, row.h), "Back")) {
            U->onboard_step = step - 1;
            U->onboard_anim = 0.0f;
        }
        if (ui_button(1763, rect_make(row.x + bw + gap, row.y, bw, row.h),
                      step == ONBOARD_STEPS - 1 ? "Start training" : "Next", 1)) {
            if (step == ONBOARD_STEPS - 1) {
                stone_save_profile();            /* marks the profile configured */
                stone_add_weight(p->weight_kg);  /* seed the progress chart      */
                U->page = PAGE_DASHBOARD;
                U->page_anim = 0.0f;
                stone_app_toast("Welcome aboard, %s", p->name[0] ? p->name : "athlete");
            } else {
                U->onboard_step = step + 1;
                U->onboard_anim = 0.0f;
            }
        }
    }
}
