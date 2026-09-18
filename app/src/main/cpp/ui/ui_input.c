/* ui_input.c - turns a committed on-screen keyboard value into a model change.
 *
 * Every text field in STONE funnels through here, so all validation, clamping
 * and persistence for user input lives in exactly one place.
 */
#include "ui.h"

#include <stdlib.h>
#include <string.h>

#include "../core/calc.h"
#include "../utils/strbuf.h"

/* Lenient float parse: returns 0 when the text holds no digit at all. */
static int parse_float(const char *s, float *out)
{
    char *end = NULL;
    double v;
    int has_digit = 0;
    const char *p = s;

    if (!s) return 0;
    while (*p) { if (*p >= '0' && *p <= '9') { has_digit = 1; break; } ++p; }
    if (!has_digit) return 0;

    v = strtod(s, &end);
    if (end == s) return 0;
    if (v != v) return 0;                      /* NaN guard */
    if (v < -1.0e6 || v > 1.0e6) return 0;
    *out = (float)v;
    return 1;
}

static int parse_int(const char *s, int *out)
{
    float f;
    if (!parse_float(s, &f)) return 0;
    *out = (int)(f + 0.5f);
    return 1;
}

static void trim_copy(char *dst, int dst_len, const char *src)
{
    const char *start = src;
    const char *end;
    int n;

    while (*start == ' ' || *start == '\t') ++start;
    end = start + strlen(start);
    while (end > start && (end[-1] == ' ' || end[-1] == '\t')) --end;

    n = (int)(end - start);
    if (n > dst_len - 1) n = dst_len - 1;
    if (n < 0) n = 0;
    memcpy(dst, start, (size_t)n);
    dst[n] = '\0';
}

void ui_handle_input_commit(int kind, void *target, const char *text)
{
    StoneApp *app = stone_app();
    UiState *U = ui_state();
    StoneProfile *p = &app->profile;
    float f = 0.0f;
    int i = 0;

    (void)target;
    if (!text) return;

    switch (kind) {

    case KB_TARGET_NAME:
        trim_copy(p->name, (int)sizeof(p->name), text);
        stone_save_profile();
        stone_app_toast(p->name[0] ? "Name updated" : "Name cleared");
        break;

    case KB_TARGET_AGE:
        if (!parse_int(text, &i)) { stone_app_toast("Please enter a number"); break; }
        p->age = stone_clampi(i, 10, 100);
        stone_save_profile();
        break;

    case KB_TARGET_HEIGHT:
        if (!parse_float(text, &f)) { stone_app_toast("Please enter a number"); break; }
        p->height_cm = stone_clampf(f, 100.0f, 250.0f);
        stone_save_profile();
        break;

    case KB_TARGET_WEIGHT:
        if (!parse_float(text, &f)) { stone_app_toast("Please enter a number"); break; }
        f = stone_clampf(f, 25.0f, 300.0f);
        p->weight_kg = f;
        stone_save_profile();
        if (stone_add_weight(f)) stone_app_toast("Weight logged for today");
        else stone_app_toast("Weight updated");
        break;

    case KB_TARGET_TARGET_WEIGHT:
        if (!parse_float(text, &f)) { stone_app_toast("Please enter a number"); break; }
        p->target_weight_kg = stone_clampf(f, 25.0f, 300.0f);
        stone_save_profile();
        stone_app_toast("Target weight updated");
        break;

    case KB_TARGET_WEEKLY:
        if (!parse_int(text, &i)) { stone_app_toast("Please enter a number"); break; }
        p->weekly_workout_target = stone_clampi(i, 1, 14);
        stone_save_profile();
        break;

    case KB_TARGET_PORTIONS:
        if (!parse_float(text, &f) || f <= 0.0f) {
            stone_app_toast("Portions must be greater than zero");
            break;
        }
        f = stone_clampf(f, 0.1f, 20.0f);
        U->food_portions = f;
        if (stone_log_food(U->selected_food, f, U->selected_meal)) {
            const StoneFood *food = stone_find_food(U->selected_food);
            stone_app_toast("%s logged (%s)",
                            food ? food->name : "Food",
                            stone_meal_name(U->selected_meal));
        } else {
            stone_app_toast("Could not log this food");
        }
        break;

    case KB_TARGET_FOOD_SEARCH:
        trim_copy(U->food_query, (int)sizeof(U->food_query), text);
        break;

    case KB_TARGET_FORM_NAME:
        trim_copy(U->form_name, (int)sizeof(U->form_name), text);
        break;

    case KB_TARGET_FORM_SERVING:
        if (parse_float(text, &f)) U->form_serving = stone_clampf(f, 1.0f, 5000.0f);
        else stone_app_toast("Please enter a number");
        break;

    case KB_TARGET_FORM_KCAL:
        if (parse_float(text, &f)) U->form_kcal = stone_clampf(f, 0.0f, 5000.0f);
        else stone_app_toast("Please enter a number");
        break;

    case KB_TARGET_FORM_PROTEIN:
        if (parse_float(text, &f)) U->form_protein = stone_clampf(f, 0.0f, 500.0f);
        else stone_app_toast("Please enter a number");
        break;

    case KB_TARGET_FORM_CARBS:
        if (parse_float(text, &f)) U->form_carbs = stone_clampf(f, 0.0f, 500.0f);
        else stone_app_toast("Please enter a number");
        break;

    case KB_TARGET_FORM_FAT:
        if (parse_float(text, &f)) U->form_fat = stone_clampf(f, 0.0f, 500.0f);
        else stone_app_toast("Please enter a number");
        break;

    default:
        break;
    }
}
