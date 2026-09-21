/* localtime_r(), fsync(), fileno() */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "app.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "calc.h"
#include "../data/defaults.h"
#include "../storage/storage.h"
#include "../utils/strbuf.h"

static StoneApp g_app;
static float    g_autosave_timer = 0.0f;

StoneApp *stone_app(void) { return &g_app; }

/* ------------------------------------------------------------ tiny vectors */

static int vec_grow(void **items, int *cap, int count, size_t elem)
{
    int ncap;
    void *p;
    if (count < *cap) return 1;
    ncap = *cap ? *cap * 2 : 16;
    if (ncap > 200000) return 0;            /* sanity ceiling */
    p = realloc(*items, (size_t)ncap * elem);
    if (!p) return 0;
    *items = p;
    *cap = ncap;
    return 1;
}

#define VEC_PUSH(arr, count, cap) \
    vec_grow((void **)&(arr), &(cap), (count), sizeof(*(arr)))

/* ---------------------------------------------------------------- toasts */

void stone_app_toast(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_app.status, sizeof(g_app.status), fmt, ap);
    va_end(ap);
    g_app.status_timer = 2.6f;
}

void stone_app_mark_dirty(int mask)
{
    g_app.dirty_mask |= mask;
}

/* ------------------------------------------------------------------ dates */

long stone_now(void) { return (long)time(NULL); }

void stone_today_string(char *out, int len)
{
    time_t t = time(NULL);
    struct tm tmv;
    if (!out || len < STONE_DATE_LEN) return;
#if defined(_WIN32)
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    snprintf(out, (size_t)len, "%04d-%02d-%02d",
             tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday);
}

/* Days since 1970-01-01 using a civil-calendar algorithm (no time zone math,
   no libc dependency, valid for any date the user can type). */
int stone_date_to_days(const char *date)
{
    int y, m, d, era, yoe, doy, doe;
    long days;

    if (!date || strlen(date) < 10) return 0;
    if (sscanf(date, "%4d-%2d-%2d", &y, &m, &d) != 3) return 0;
    if (y < 1970 || y > 2200 || m < 1 || m > 12 || d < 1 || d > 31) return 0;

    y -= (m <= 2);
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = y - era * 400;
    doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    days = (long)era * 146097 + doe - 719468;
    if (days < 0) days = 0;
    return (int)days;
}

void stone_days_to_date(int days, char *out, int len)
{
    int z, era, doe, yoe, y, doy, mp, d, m;
    if (!out || len < STONE_DATE_LEN) return;
    if (days < 0) days = 0;

    z = days + 719468;
    era = (z >= 0 ? z : z - 146096) / 146097;
    doe = z - era * 146097;
    yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    y = yoe + era * 400;
    doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    mp = (5 * doy + 2) / 153;
    d = doy - (153 * mp + 2) / 5 + 1;
    m = mp + (mp < 10 ? 3 : -9);
    y += (m <= 2);
    snprintf(out, (size_t)len, "%04d-%02d-%02d", y, m, d);
}

int stone_date_diff(const char *a, const char *b)
{
    return stone_date_to_days(a) - stone_date_to_days(b);
}

void stone_date_short(const char *date, char *out, int len)
{
    static const char *mn[] = {"Jan","Feb","Mar","Apr","May","Jun",
                               "Jul","Aug","Sep","Oct","Nov","Dec"};
    int y, m, d;
    if (!out || len < 8) return;
    if (!date || sscanf(date, "%4d-%2d-%2d", &y, &m, &d) != 3 || m < 1 || m > 12) {
        stone_strlcpy(out, "-", (size_t)len);
        return;
    }
    snprintf(out, (size_t)len, "%d %s", d, mn[m - 1]);
}

const char *stone_weekday(const char *date)
{
    static const char *wd[] = {"Thu","Fri","Sat","Sun","Mon","Tue","Wed"};
    int days = stone_date_to_days(date);      /* 1970-01-01 was a Thursday */
    return wd[((days % 7) + 7) % 7];
}

/* ------------------------------------------------------- (de)serialisation */

static void profile_defaults(StoneProfile *p)
{
    memset(p, 0, sizeof(*p));
    stone_strlcpy(p->name, "Athlete", sizeof(p->name));
    p->age = 25;
    p->sex = SEX_MALE;
    p->height_cm = 170.0f;
    p->weight_kg = 70.0f;
    p->target_weight_kg = 65.0f;
    p->activity = ACT_LIGHT;
    p->goal = GOAL_LOSE;
    p->weekly_workout_target = 4;
    p->configured = 0;
}

static void settings_defaults(StoneSettings *s)
{
    s->use_metric = 1;
    s->dark_theme = 1;
    s->sound_enabled = 1;
    s->animations_enabled = 1;
}

static void profile_from_json(StoneProfile *p, const JsonValue *o)
{
    if (!o) return;
    stone_strlcpy(p->name, json_str(o, "name", p->name), sizeof(p->name));
    p->age                   = stone_clampi(json_int(o, "age", p->age), 10, 100);
    p->sex                   = json_int(o, "sex", p->sex) == SEX_FEMALE ? SEX_FEMALE : SEX_MALE;
    p->height_cm             = stone_clampf((float)json_num(o, "height_cm", p->height_cm), 100.0f, 250.0f);
    p->weight_kg             = stone_clampf((float)json_num(o, "weight_kg", p->weight_kg), 25.0f, 400.0f);
    p->target_weight_kg      = stone_clampf((float)json_num(o, "target_weight_kg", p->target_weight_kg), 25.0f, 400.0f);
    p->activity              = stone_clampi(json_int(o, "activity", p->activity), 0, ACT_COUNT - 1);
    p->goal                  = stone_clampi(json_int(o, "goal", p->goal), 0, GOAL_COUNT - 1);
    p->weekly_workout_target = stone_clampi(json_int(o, "weekly_workout_target", p->weekly_workout_target), 1, 14);
    p->configured            = json_bool(o, "configured", p->configured);
}

static JsonValue *profile_to_json(const StoneProfile *p)
{
    JsonValue *o = json_new_object();
    if (!o) return NULL;
    json_set_int(o, "schema", STONE_SCHEMA);
    json_set_str(o, "name", p->name);
    json_set_int(o, "age", p->age);
    json_set_int(o, "sex", p->sex);
    json_set_num(o, "height_cm", p->height_cm);
    json_set_num(o, "weight_kg", p->weight_kg);
    json_set_num(o, "target_weight_kg", p->target_weight_kg);
    json_set_int(o, "activity", p->activity);
    json_set_int(o, "goal", p->goal);
    json_set_int(o, "weekly_workout_target", p->weekly_workout_target);
    json_set_bool(o, "configured", p->configured);
    return o;
}

static JsonValue *settings_to_json(const StoneSettings *s)
{
    JsonValue *o = json_new_object();
    if (!o) return NULL;
    json_set_int(o, "schema", STONE_SCHEMA);
    json_set_bool(o, "use_metric", s->use_metric);
    json_set_bool(o, "dark_theme", s->dark_theme);
    json_set_bool(o, "sound_enabled", s->sound_enabled);
    json_set_bool(o, "animations_enabled", s->animations_enabled);
    return o;
}

static JsonValue *foods_to_json(void)
{
    JsonValue *root = json_new_object();
    JsonValue *arr = json_new_array();
    int i;
    if (!root || !arr) { json_free(root); json_free(arr); return NULL; }

    json_set_int(root, "schema", STONE_SCHEMA);
    for (i = 0; i < g_app.food_count; ++i) {
        const StoneFood *f = &g_app.foods[i];
        JsonValue *o = json_new_object();
        json_set_int(o, "id", f->id);
        json_set_str(o, "name", f->name);
        json_set_num(o, "serving_g", f->serving_g);
        json_set_num(o, "kcal", f->kcal);
        json_set_num(o, "protein_g", f->protein_g);
        json_set_num(o, "carbs_g", f->carbs_g);
        json_set_num(o, "fat_g", f->fat_g);
        json_set_bool(o, "custom", f->custom);
        json_array_push(arr, o);
    }
    json_object_set(root, "foods", arr);
    return root;
}

static int push_food(const StoneFood *src)
{
    if (!VEC_PUSH(g_app.foods, g_app.food_count, g_app.food_cap)) return 0;
    g_app.foods[g_app.food_count++] = *src;
    if (src->id >= g_app.next_food_id) g_app.next_food_id = src->id + 1;
    return 1;
}

static void load_default_foods(void)
{
    int i;
    for (i = 0; i < k_default_food_count; ++i) push_food(&k_default_foods[i]);
}

static void load_default_workouts(void)
{
    int i;
    for (i = 0; i < k_default_workout_count; ++i) {
        if (!VEC_PUSH(g_app.workouts, g_app.workout_count, g_app.workout_cap)) return;
        g_app.workouts[g_app.workout_count++] = k_default_workouts[i];
    }
}

static void foods_from_json(const JsonValue *root)
{
    const JsonValue *arr = json_get(root, "foods");
    int i, n = json_count(arr);

    if (!arr || arr->type != JSON_ARRAY || n == 0) { load_default_foods(); return; }
    for (i = 0; i < n; ++i) {
        const JsonValue *o = json_at(arr, i);
        StoneFood f;
        if (!o || o->type != JSON_OBJECT) continue;
        memset(&f, 0, sizeof(f));
        f.id = json_int(o, "id", 0);
        stone_strlcpy(f.name, json_str(o, "name", "Unknown"), sizeof(f.name));
        f.serving_g = stone_clampf((float)json_num(o, "serving_g", 100.0), 1.0f, 5000.0f);
        f.kcal      = stone_clampf((float)json_num(o, "kcal", 0.0), 0.0f, 5000.0f);
        f.protein_g = stone_clampf((float)json_num(o, "protein_g", 0.0), 0.0f, 1000.0f);
        f.carbs_g   = stone_clampf((float)json_num(o, "carbs_g", 0.0), 0.0f, 1000.0f);
        f.fat_g     = stone_clampf((float)json_num(o, "fat_g", 0.0), 0.0f, 1000.0f);
        f.custom    = json_bool(o, "custom", 0);
        if (f.id <= 0) f.id = g_app.next_food_id;
        push_food(&f);
    }
    if (g_app.food_count == 0) load_default_foods();
}

static JsonValue *workouts_to_json(void)
{
    JsonValue *root = json_new_object();
    JsonValue *arr = json_new_array();
    int i, e;
    if (!root || !arr) { json_free(root); json_free(arr); return NULL; }

    json_set_int(root, "schema", STONE_SCHEMA);
    for (i = 0; i < g_app.workout_count; ++i) {
        const StoneWorkout *w = &g_app.workouts[i];
        JsonValue *o = json_new_object();
        JsonValue *ex = json_new_array();
        json_set_int(o, "id", w->id);
        json_set_str(o, "name", w->name);
        json_set_str(o, "description", w->description);
        json_set_int(o, "level", w->level);
        json_set_int(o, "category", w->category);
        json_set_int(o, "est_minutes", w->est_minutes);
        json_set_num(o, "kcal_per_min", w->kcal_per_min);
        for (e = 0; e < w->exercise_count; ++e) {
            const StoneExercise *x = &w->exercises[e];
            JsonValue *xo = json_new_object();
            json_set_str(xo, "name", x->name);
            json_set_str(xo, "description", x->description);
            json_set_str(xo, "instruction", x->instruction);
            json_set_int(xo, "sets", x->sets);
            json_set_int(xo, "reps", x->reps);
            json_set_int(xo, "duration_s", x->duration_s);
            json_set_int(xo, "rest_s", x->rest_s);
            json_array_push(ex, xo);
        }
        json_object_set(o, "exercises", ex);
        json_array_push(arr, o);
    }
    json_object_set(root, "workouts", arr);
    return root;
}

static void workouts_from_json(const JsonValue *root)
{
    const JsonValue *arr = json_get(root, "workouts");
    int i, n = json_count(arr);

    if (!arr || arr->type != JSON_ARRAY || n == 0) { load_default_workouts(); return; }
    for (i = 0; i < n; ++i) {
        const JsonValue *o = json_at(arr, i);
        const JsonValue *ex;
        StoneWorkout w;
        int e, en;
        if (!o || o->type != JSON_OBJECT) continue;

        memset(&w, 0, sizeof(w));
        w.id = json_int(o, "id", i + 1);
        stone_strlcpy(w.name, json_str(o, "name", "Workout"), sizeof(w.name));
        stone_strlcpy(w.description, json_str(o, "description", ""), sizeof(w.description));
        w.level        = stone_clampi(json_int(o, "level", 0), 0, LEVEL_COUNT - 1);
        w.category     = stone_clampi(json_int(o, "category", 0), 0, CAT_COUNT - 1);
        w.est_minutes  = stone_clampi(json_int(o, "est_minutes", 20), 1, 300);
        w.kcal_per_min = stone_clampf((float)json_num(o, "kcal_per_min", 6.0), 0.5f, 30.0f);

        ex = json_get(o, "exercises");
        en = json_count(ex);
        if (en > STONE_MAX_EXERCISES) en = STONE_MAX_EXERCISES;
        for (e = 0; e < en; ++e) {
            const JsonValue *xo = json_at(ex, e);
            StoneExercise *x = &w.exercises[w.exercise_count];
            if (!xo || xo->type != JSON_OBJECT) continue;
            memset(x, 0, sizeof(*x));
            stone_strlcpy(x->name, json_str(xo, "name", "Exercise"), sizeof(x->name));
            stone_strlcpy(x->description, json_str(xo, "description", ""), sizeof(x->description));
            stone_strlcpy(x->instruction, json_str(xo, "instruction", ""), sizeof(x->instruction));
            x->sets       = stone_clampi(json_int(xo, "sets", 3), 1, 20);
            x->reps       = stone_clampi(json_int(xo, "reps", 0), 0, 500);
            x->duration_s = stone_clampi(json_int(xo, "duration_s", 0), 0, 7200);
            x->rest_s     = stone_clampi(json_int(xo, "rest_s", 30), 0, 600);
            w.exercise_count++;
        }
        if (w.exercise_count == 0) continue;
        if (!VEC_PUSH(g_app.workouts, g_app.workout_count, g_app.workout_cap)) break;
        g_app.workouts[g_app.workout_count++] = w;
    }
    if (g_app.workout_count == 0) load_default_workouts();
}

static JsonValue *history_to_json(void)
{
    JsonValue *root = json_new_object();
    JsonValue *fl = json_new_array();
    JsonValue *wl = json_new_array();
    int i;
    if (!root || !fl || !wl) { json_free(root); json_free(fl); json_free(wl); return NULL; }

    json_set_int(root, "schema", STONE_SCHEMA);
    for (i = 0; i < g_app.food_log_count; ++i) {
        const StoneFoodLog *l = &g_app.food_log[i];
        JsonValue *o = json_new_object();
        json_set_str(o, "date", l->date);
        json_set_int(o, "food_id", l->food_id);
        json_set_str(o, "food_name", l->food_name);
        json_set_num(o, "portions", l->portions);
        json_set_num(o, "kcal", l->kcal);
        json_set_num(o, "protein_g", l->protein_g);
        json_set_num(o, "carbs_g", l->carbs_g);
        json_set_num(o, "fat_g", l->fat_g);
        json_set_int(o, "meal", l->meal);
        json_set_num(o, "timestamp", (double)l->timestamp);
        json_array_push(fl, o);
    }
    for (i = 0; i < g_app.workout_log_count; ++i) {
        const StoneWorkoutLog *l = &g_app.workout_log[i];
        JsonValue *o = json_new_object();
        json_set_str(o, "date", l->date);
        json_set_int(o, "workout_id", l->workout_id);
        json_set_str(o, "workout_name", l->workout_name);
        json_set_int(o, "duration_s", l->duration_s);
        json_set_int(o, "kcal_burned", l->kcal_burned);
        json_set_num(o, "timestamp", (double)l->timestamp);
        json_array_push(wl, o);
    }
    json_object_set(root, "food_log", fl);
    json_object_set(root, "workout_log", wl);
    return root;
}

static void history_from_json(const JsonValue *root)
{
    const JsonValue *fl = json_get(root, "food_log");
    const JsonValue *wl = json_get(root, "workout_log");
    int i, n;

    n = json_count(fl);
    for (i = 0; i < n; ++i) {
        const JsonValue *o = json_at(fl, i);
        StoneFoodLog l;
        if (!o || o->type != JSON_OBJECT) continue;
        memset(&l, 0, sizeof(l));
        stone_strlcpy(l.date, json_str(o, "date", ""), sizeof(l.date));
        if (stone_date_to_days(l.date) == 0) continue;     /* drop broken rows */
        l.food_id   = json_int(o, "food_id", 0);
        stone_strlcpy(l.food_name, json_str(o, "food_name", "Food"), sizeof(l.food_name));
        l.portions  = stone_clampf((float)json_num(o, "portions", 1.0), 0.05f, 50.0f);
        l.kcal      = stone_clampf((float)json_num(o, "kcal", 0.0), 0.0f, 20000.0f);
        l.protein_g = stone_clampf((float)json_num(o, "protein_g", 0.0), 0.0f, 5000.0f);
        l.carbs_g   = stone_clampf((float)json_num(o, "carbs_g", 0.0), 0.0f, 5000.0f);
        l.fat_g     = stone_clampf((float)json_num(o, "fat_g", 0.0), 0.0f, 5000.0f);
        l.meal      = stone_clampi(json_int(o, "meal", MEAL_SNACK), 0, MEAL_COUNT - 1);
        l.timestamp = (long)json_num(o, "timestamp", 0.0);
        if (!VEC_PUSH(g_app.food_log, g_app.food_log_count, g_app.food_log_cap)) break;
        g_app.food_log[g_app.food_log_count++] = l;
    }

    n = json_count(wl);
    for (i = 0; i < n; ++i) {
        const JsonValue *o = json_at(wl, i);
        StoneWorkoutLog l;
        if (!o || o->type != JSON_OBJECT) continue;
        memset(&l, 0, sizeof(l));
        stone_strlcpy(l.date, json_str(o, "date", ""), sizeof(l.date));
        if (stone_date_to_days(l.date) == 0) continue;
        l.workout_id = json_int(o, "workout_id", 0);
        stone_strlcpy(l.workout_name, json_str(o, "workout_name", "Workout"), sizeof(l.workout_name));
        l.duration_s  = stone_clampi(json_int(o, "duration_s", 0), 0, 24 * 3600);
        l.kcal_burned = stone_clampi(json_int(o, "kcal_burned", 0), 0, 20000);
        l.timestamp   = (long)json_num(o, "timestamp", 0.0);
        if (!VEC_PUSH(g_app.workout_log, g_app.workout_log_count, g_app.workout_log_cap)) break;
        g_app.workout_log[g_app.workout_log_count++] = l;
    }
}

static JsonValue *progress_to_json(void)
{
    JsonValue *root = json_new_object();
    JsonValue *arr = json_new_array();
    int i;
    if (!root || !arr) { json_free(root); json_free(arr); return NULL; }

    json_set_int(root, "schema", STONE_SCHEMA);
    for (i = 0; i < g_app.weight_count; ++i) {
        JsonValue *o = json_new_object();
        json_set_str(o, "date", g_app.weights[i].date);
        json_set_num(o, "weight_kg", g_app.weights[i].weight_kg);
        json_set_num(o, "timestamp", (double)g_app.weights[i].timestamp);
        json_array_push(arr, o);
    }
    json_object_set(root, "weights", arr);
    return root;
}

static void progress_from_json(const JsonValue *root)
{
    const JsonValue *arr = json_get(root, "weights");
    int i, n = json_count(arr);
    for (i = 0; i < n; ++i) {
        const JsonValue *o = json_at(arr, i);
        StoneWeightEntry w;
        if (!o || o->type != JSON_OBJECT) continue;
        memset(&w, 0, sizeof(w));
        stone_strlcpy(w.date, json_str(o, "date", ""), sizeof(w.date));
        if (stone_date_to_days(w.date) == 0) continue;
        w.weight_kg = stone_clampf((float)json_num(o, "weight_kg", 0.0), 25.0f, 400.0f);
        w.timestamp = (long)json_num(o, "timestamp", 0.0);
        if (!VEC_PUSH(g_app.weights, g_app.weight_count, g_app.weight_cap)) break;
        g_app.weights[g_app.weight_count++] = w;
    }
}

/* --------------------------------------------------------- init / shutdown */

static void sort_weights(void)
{
    int i, j;
    /* Insertion sort: the list is tiny and almost always already ordered. */
    for (i = 1; i < g_app.weight_count; ++i) {
        StoneWeightEntry key = g_app.weights[i];
        int kd = stone_date_to_days(key.date);
        for (j = i - 1; j >= 0 && stone_date_to_days(g_app.weights[j].date) > kd; --j)
            g_app.weights[j + 1] = g_app.weights[j];
        g_app.weights[j + 1] = key;
    }
}

void stone_app_init(const char *internal_dir, const char *external_dir)
{
    JsonValue *v;
    StorageResult err;

    memset(&g_app, 0, sizeof(g_app));
    profile_defaults(&g_app.profile);
    settings_defaults(&g_app.settings);
    g_app.next_food_id = 1;
    stone_today_string(g_app.today, sizeof(g_app.today));

    storage_init(internal_dir, external_dir);
    if (!storage_ready()) {
        g_app.storage_warning = 1;
        load_default_foods();
        load_default_workouts();
        stone_app_toast("Storage unavailable - running in memory only");
        return;
    }

    v = storage_read_json(FILE_PROFILE, &err);
    if (v) { profile_from_json(&g_app.profile, v); json_free(v); }
    else if (err == STORAGE_ERR_PARSE) stone_app_toast("profile.json was corrupt - defaults restored");

    v = storage_read_json(FILE_SETTINGS, &err);
    if (v) {
        g_app.settings.use_metric        = json_bool(v, "use_metric", 1);
        g_app.settings.dark_theme        = json_bool(v, "dark_theme", 1);
        g_app.settings.sound_enabled     = json_bool(v, "sound_enabled", 1);
        g_app.settings.animations_enabled= json_bool(v, "animations_enabled", 1);
        json_free(v);
    }

    v = storage_read_json(FILE_FOODS, &err);
    if (v) { foods_from_json(v); json_free(v); }
    else { load_default_foods(); stone_app_mark_dirty(DIRTY_FOODS); }

    v = storage_read_json(FILE_WORKOUTS, &err);
    if (v) { workouts_from_json(v); json_free(v); }
    else { load_default_workouts(); stone_app_mark_dirty(DIRTY_WORKOUTS); }

    v = storage_read_json(FILE_HISTORY, &err);
    if (v) { history_from_json(v); json_free(v); }

    v = storage_read_json(FILE_PROGRESS, &err);
    if (v) { progress_from_json(v); json_free(v); sort_weights(); }

    /* Seed the very first weight point so the chart is never empty. */
    if (g_app.weight_count == 0 && g_app.profile.configured)
        stone_add_weight(g_app.profile.weight_kg);

    if (g_app.dirty_mask) stone_app_save_all();
}

void stone_app_shutdown(void)
{
    stone_app_save_all();
    free(g_app.foods);
    free(g_app.workouts);
    free(g_app.food_log);
    free(g_app.workout_log);
    free(g_app.weights);
    memset(&g_app, 0, sizeof(g_app));
}

static void save_one(int flag, const char *file, JsonValue *(*build)(void))
{
    JsonValue *v;
    if (!(g_app.dirty_mask & flag)) return;
    v = build();
    if (!v) { g_app.storage_warning = 1; return; }
    if (storage_write_json(file, v) != STORAGE_OK) {
        g_app.storage_warning = 1;
    } else {
        g_app.dirty_mask &= ~flag;
    }
    json_free(v);
}

static JsonValue *build_profile(void)  { return profile_to_json(&g_app.profile); }
static JsonValue *build_settings(void) { return settings_to_json(&g_app.settings); }

void stone_app_save_all(void)
{
    if (!storage_ready()) return;
    save_one(DIRTY_PROFILE,  FILE_PROFILE,  build_profile);
    save_one(DIRTY_SETTINGS, FILE_SETTINGS, build_settings);
    save_one(DIRTY_FOODS,    FILE_FOODS,    foods_to_json);
    save_one(DIRTY_WORKOUTS, FILE_WORKOUTS, workouts_to_json);
    save_one(DIRTY_HISTORY,  FILE_HISTORY,  history_to_json);
    save_one(DIRTY_PROGRESS, FILE_PROGRESS, progress_to_json);
}

void stone_app_tick(float dt)
{
    if (g_app.status_timer > 0.0f) {
        g_app.status_timer -= dt;
        if (g_app.status_timer <= 0.0f) g_app.status[0] = '\0';
    }

    /* Autosave a couple of seconds after the last change, so rapid edits do
       not hammer the filesystem but nothing stays unsaved for long. */
    if (g_app.dirty_mask) {
        g_autosave_timer += dt;
        if (g_autosave_timer > 2.0f) { stone_app_save_all(); g_autosave_timer = 0.0f; }
    } else {
        g_autosave_timer = 0.0f;
    }

    /* Roll over the cached "today" when the app stays open past midnight. */
    {
        char now[STONE_DATE_LEN];
        stone_today_string(now, sizeof(now));
        if (strcmp(now, g_app.today) != 0) stone_strlcpy(g_app.today, now, sizeof(g_app.today));
    }
}

void stone_save_profile(void)
{
    g_app.profile.configured = 1;
    stone_app_mark_dirty(DIRTY_PROFILE);
    stone_app_save_all();
}

/* -------------------------------------------------------------------- diet */

const StoneFood *stone_find_food(int id)
{
    int i;
    for (i = 0; i < g_app.food_count; ++i)
        if (g_app.foods[i].id == id) return &g_app.foods[i];
    return NULL;
}

int stone_add_custom_food(const char *name, float serving_g, float kcal,
                          float protein, float carbs, float fat)
{
    StoneFood f;
    if (!name || !name[0]) return 0;

    memset(&f, 0, sizeof(f));
    f.id = g_app.next_food_id;
    stone_strlcpy(f.name, name, sizeof(f.name));
    f.serving_g = stone_clampf(serving_g, 1.0f, 5000.0f);
    f.kcal      = stone_clampf(kcal, 0.0f, 5000.0f);
    f.protein_g = stone_clampf(protein, 0.0f, 1000.0f);
    f.carbs_g   = stone_clampf(carbs, 0.0f, 1000.0f);
    f.fat_g     = stone_clampf(fat, 0.0f, 1000.0f);
    f.custom    = 1;

    if (!push_food(&f)) return 0;
    stone_app_mark_dirty(DIRTY_FOODS);
    return f.id;
}

int stone_delete_food(int id)
{
    int i;
    for (i = 0; i < g_app.food_count; ++i) {
        if (g_app.foods[i].id != id) continue;
        if (!g_app.foods[i].custom) return 0;      /* built-ins stay put */
        memmove(&g_app.foods[i], &g_app.foods[i + 1],
                (size_t)(g_app.food_count - i - 1) * sizeof(StoneFood));
        g_app.food_count--;
        stone_app_mark_dirty(DIRTY_FOODS);
        return 1;
    }
    return 0;
}

int stone_log_food(int food_id, float portions, int meal)
{
    const StoneFood *f = stone_find_food(food_id);
    StoneFoodLog l;

    if (!f) return 0;
    portions = stone_clampf(portions, 0.05f, 50.0f);

    memset(&l, 0, sizeof(l));
    stone_strlcpy(l.date, g_app.today, sizeof(l.date));
    l.food_id = f->id;
    stone_strlcpy(l.food_name, f->name, sizeof(l.food_name));
    l.portions  = portions;
    l.kcal      = f->kcal * portions;
    l.protein_g = f->protein_g * portions;
    l.carbs_g   = f->carbs_g * portions;
    l.fat_g     = f->fat_g * portions;
    l.meal      = stone_clampi(meal, 0, MEAL_COUNT - 1);
    l.timestamp = stone_now();

    if (!VEC_PUSH(g_app.food_log, g_app.food_log_count, g_app.food_log_cap)) return 0;
    g_app.food_log[g_app.food_log_count++] = l;
    stone_app_mark_dirty(DIRTY_HISTORY);
    return 1;
}

int stone_remove_food_log(int index)
{
    if (index < 0 || index >= g_app.food_log_count) return 0;
    memmove(&g_app.food_log[index], &g_app.food_log[index + 1],
            (size_t)(g_app.food_log_count - index - 1) * sizeof(StoneFoodLog));
    g_app.food_log_count--;
    stone_app_mark_dirty(DIRTY_HISTORY);
    return 1;
}

void stone_day_nutrition(const char *date, float *kcal, float *protein,
                         float *carbs, float *fat)
{
    int i;
    float k = 0, p = 0, c = 0, f = 0;
    for (i = 0; i < g_app.food_log_count; ++i) {
        if (strcmp(g_app.food_log[i].date, date) != 0) continue;
        k += g_app.food_log[i].kcal;
        p += g_app.food_log[i].protein_g;
        c += g_app.food_log[i].carbs_g;
        f += g_app.food_log[i].fat_g;
    }
    if (kcal) *kcal = k;
    if (protein) *protein = p;
    if (carbs) *carbs = c;
    if (fat) *fat = f;
}

static int ci_contains(const char *hay, const char *needle)
{
    size_t nl;
    if (!needle || !needle[0]) return 1;
    if (!hay) return 0;
    nl = strlen(needle);
    for (; *hay; ++hay) {
        size_t i;
        for (i = 0; i < nl; ++i) {
            char a = hay[i], b = needle[i];
            if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
            if (a != b || a == '\0') break;
        }
        if (i == nl) return 1;
    }
    return 0;
}

int stone_food_search(const char *query, int *out_indices, int max)
{
    int i, n = 0;
    for (i = 0; i < g_app.food_count && n < max; ++i)
        if (ci_contains(g_app.foods[i].name, query)) out_indices[n++] = i;
    return n;
}

/* ----------------------------------------------------------------- workout */

const StoneWorkout *stone_find_workout(int id)
{
    int i;
    for (i = 0; i < g_app.workout_count; ++i)
        if (g_app.workouts[i].id == id) return &g_app.workouts[i];
    return NULL;
}

int stone_log_workout(int workout_id, int duration_s)
{
    const StoneWorkout *w = stone_find_workout(workout_id);
    StoneWorkoutLog l;

    if (!w) return 0;
    duration_s = stone_clampi(duration_s, 0, 24 * 3600);

    memset(&l, 0, sizeof(l));
    stone_strlcpy(l.date, g_app.today, sizeof(l.date));
    l.workout_id = w->id;
    stone_strlcpy(l.workout_name, w->name, sizeof(l.workout_name));
    l.duration_s = duration_s;
    l.kcal_burned = (int)(w->kcal_per_min * (float)duration_s / 60.0f);
    l.timestamp = stone_now();

    if (!VEC_PUSH(g_app.workout_log, g_app.workout_log_count, g_app.workout_log_cap)) return 0;
    g_app.workout_log[g_app.workout_log_count++] = l;
    stone_app_mark_dirty(DIRTY_HISTORY);
    return 1;
}

int stone_remove_workout_log(int index)
{
    if (index < 0 || index >= g_app.workout_log_count) return 0;
    memmove(&g_app.workout_log[index], &g_app.workout_log[index + 1],
            (size_t)(g_app.workout_log_count - index - 1) * sizeof(StoneWorkoutLog));
    g_app.workout_log_count--;
    stone_app_mark_dirty(DIRTY_HISTORY);
    return 1;
}

int stone_workouts_on(const char *date)
{
    int i, n = 0;
    for (i = 0; i < g_app.workout_log_count; ++i)
        if (strcmp(g_app.workout_log[i].date, date) == 0) n++;
    return n;
}

int stone_kcal_burned_on(const char *date)
{
    int i, n = 0;
    for (i = 0; i < g_app.workout_log_count; ++i)
        if (strcmp(g_app.workout_log[i].date, date) == 0) n += g_app.workout_log[i].kcal_burned;
    return n;
}

int stone_workouts_this_week(void)
{
    /* This is deliberately a calendar week (Mon--Sun), not a moving seven
       day window.  The value is presented as "this week" throughout the UI,
       and a rolling window made the target look complete on a Monday because
       workouts from the previous week were still included. */
    int today = stone_date_to_days(g_app.today);
    int weekday = (today + 3) % 7;       /* 1970-01-01 was Thursday */
    int week_start;
    int i, n = 0;

    if (weekday < 0) weekday += 7;
    week_start = today - weekday;        /* Monday */
    for (i = 0; i < g_app.workout_log_count; ++i) {
        int d = stone_date_to_days(g_app.workout_log[i].date);
        if (d >= week_start && d <= today) n++;
    }
    return n;
}

/* ---------------------------------------------------------------- progress */

int stone_add_weight(float kg)
{
    int i;
    StoneWeightEntry e;

    kg = stone_clampf(kg, 25.0f, 400.0f);
    for (i = 0; i < g_app.weight_count; ++i) {
        if (strcmp(g_app.weights[i].date, g_app.today) == 0) {
            g_app.weights[i].weight_kg = kg;         /* one entry per day */
            g_app.weights[i].timestamp = stone_now();
            g_app.profile.weight_kg = kg;
            stone_app_mark_dirty(DIRTY_PROGRESS | DIRTY_PROFILE);
            return 1;
        }
    }

    memset(&e, 0, sizeof(e));
    stone_strlcpy(e.date, g_app.today, sizeof(e.date));
    e.weight_kg = kg;
    e.timestamp = stone_now();

    if (!VEC_PUSH(g_app.weights, g_app.weight_count, g_app.weight_cap)) return 0;
    g_app.weights[g_app.weight_count++] = e;
    sort_weights();
    g_app.profile.weight_kg = kg;
    stone_app_mark_dirty(DIRTY_PROGRESS | DIRTY_PROFILE);
    return 1;
}

float stone_latest_weight(void)
{
    if (g_app.weight_count > 0) return g_app.weights[g_app.weight_count - 1].weight_kg;
    return g_app.profile.weight_kg;
}

float stone_weight_change_7d(void)
{
    int i, today = stone_date_to_days(g_app.today);
    float base = -1.0f;
    for (i = 0; i < g_app.weight_count; ++i) {
        int age = today - stone_date_to_days(g_app.weights[i].date);
        if (age <= 7) { base = g_app.weights[i].weight_kg; break; }
    }
    if (base < 0.0f) return 0.0f;
    return stone_latest_weight() - base;
}

static int day_has_activity(int day)
{
    char date[STONE_DATE_LEN];
    int i;
    stone_days_to_date(day, date, sizeof(date));
    for (i = 0; i < g_app.workout_log_count; ++i)
        if (strcmp(g_app.workout_log[i].date, date) == 0) return 1;
    for (i = 0; i < g_app.food_log_count; ++i)
        if (strcmp(g_app.food_log[i].date, date) == 0) return 1;
    for (i = 0; i < g_app.weight_count; ++i)
        if (strcmp(g_app.weights[i].date, date) == 0) return 1;
    return 0;
}

int stone_active_streak(void)
{
    int today = stone_date_to_days(g_app.today);
    int streak = 0, d;
    if (!day_has_activity(today)) today -= 1;   /* today still counts if empty */
    for (d = today; d > today - 400; --d) {
        if (!day_has_activity(d)) break;
        streak++;
    }
    return streak;
}

int stone_active_days_total(void)
{
    /* Counts distinct days across every log without allocating. */
    int today = stone_date_to_days(g_app.today);
    int d, n = 0;
    for (d = today; d > today - 400; --d)
        if (day_has_activity(d)) n++;
    return n;
}

int stone_weight_series(int max_days, float *out_kg, char out_dates[][STONE_DATE_LEN], int cap)
{
    int i, n = 0, today = stone_date_to_days(g_app.today);
    for (i = 0; i < g_app.weight_count && n < cap; ++i) {
        int age = today - stone_date_to_days(g_app.weights[i].date);
        if (max_days > 0 && age > max_days) continue;
        out_kg[n] = g_app.weights[i].weight_kg;
        if (out_dates) stone_strlcpy(out_dates[n], g_app.weights[i].date, STONE_DATE_LEN);
        n++;
    }
    return n;
}

int stone_calorie_series(int days, float *out_kcal, char out_dates[][STONE_DATE_LEN], int cap)
{
    int today = stone_date_to_days(g_app.today);
    int i, n = 0;
    if (days > cap) days = cap;
    for (i = days - 1; i >= 0; --i) {
        char date[STONE_DATE_LEN];
        float k = 0;
        stone_days_to_date(today - i, date, sizeof(date));
        stone_day_nutrition(date, &k, NULL, NULL, NULL);
        out_kcal[n] = k;
        if (out_dates) stone_strlcpy(out_dates[n], date, STONE_DATE_LEN);
        n++;
    }
    return n;
}

int stone_workout_week_series(int weeks, int *out_counts, int cap)
{
    int today = stone_date_to_days(g_app.today);
    int w, n = 0;
    if (weeks > cap) weeks = cap;
    for (w = weeks - 1; w >= 0; --w) {
        int lo = today - (w + 1) * 7 + 1, hi = today - w * 7;
        int i, c = 0;
        for (i = 0; i < g_app.workout_log_count; ++i) {
            int d = stone_date_to_days(g_app.workout_log[i].date);
            if (d >= lo && d <= hi) c++;
        }
        out_counts[n++] = c;
    }
    return n;
}

/* ------------------------------------------------------------------ backup */

int stone_backup_export(char *out_path, int out_len)
{
    JsonValue *bundle = json_new_object();
    JsonValue *p, *f, *w, *h, *pr, *s;
    StorageResult r;
    char filename[64];
    char today[STONE_DATE_LEN];

    if (!bundle) return 0;
    stone_today_string(today, sizeof(today));

    json_set_str(bundle, "app", STONE_APP_NAME);
    json_set_str(bundle, "version", STONE_APP_VERSION);
    json_set_int(bundle, "schema", STONE_SCHEMA);
    json_set_str(bundle, "exported_at", today);

    p  = profile_to_json(&g_app.profile);
    s  = settings_to_json(&g_app.settings);
    f  = foods_to_json();
    w  = workouts_to_json();
    h  = history_to_json();
    pr = progress_to_json();

    json_object_set(bundle, "profile", p);
    json_object_set(bundle, "settings", s);
    json_object_set(bundle, "foods", f);
    json_object_set(bundle, "workouts", w);
    json_object_set(bundle, "history", h);
    json_object_set(bundle, "progress", pr);

    snprintf(filename, sizeof(filename), "stone_backup_%s.json", today);
    r = storage_export_bundle(filename, bundle);
    json_free(bundle);

    if (r != STORAGE_OK) {
        stone_app_toast("Backup failed: %s", storage_error_text(r));
        return 0;
    }
    if (out_path && out_len > 0)
        snprintf(out_path, (size_t)out_len, "%s/%s", storage_external_dir(), filename);
    stone_app_toast("Backup saved: %s", filename);
    return 1;
}

int stone_backup_import(const char *path)
{
    StorageResult err = STORAGE_OK;
    JsonValue *bundle = storage_import_bundle(path, &err);
    const JsonValue *sub;

    if (!bundle) {
        stone_app_toast("Restore failed: %s", storage_error_text(err));
        return 0;
    }

    /* Wipe the in-memory lists, then refill from the bundle. */
    g_app.food_count = g_app.workout_count = 0;
    g_app.food_log_count = g_app.workout_log_count = g_app.weight_count = 0;
    g_app.next_food_id = 1;

    sub = json_get(bundle, "profile");
    if (sub) profile_from_json(&g_app.profile, sub);

    sub = json_get(bundle, "settings");
    if (sub) {
        g_app.settings.dark_theme         = json_bool(sub, "dark_theme", 1);
        g_app.settings.sound_enabled      = json_bool(sub, "sound_enabled", 1);
        g_app.settings.animations_enabled = json_bool(sub, "animations_enabled", 1);
        g_app.settings.use_metric         = json_bool(sub, "use_metric", 1);
    }

    foods_from_json(json_get(bundle, "foods"));
    workouts_from_json(json_get(bundle, "workouts"));
    history_from_json(json_get(bundle, "history"));
    progress_from_json(json_get(bundle, "progress"));
    sort_weights();
    json_free(bundle);

    stone_app_mark_dirty(DIRTY_ALL);
    stone_app_save_all();
    stone_app_toast("Backup restored");
    return 1;
}

int stone_reset_all_data(void)
{
    storage_delete(FILE_PROFILE);
    storage_delete(FILE_FOODS);
    storage_delete(FILE_WORKOUTS);
    storage_delete(FILE_HISTORY);
    storage_delete(FILE_PROGRESS);
    storage_delete(FILE_SETTINGS);

    g_app.food_count = g_app.workout_count = 0;
    g_app.food_log_count = g_app.workout_log_count = g_app.weight_count = 0;
    g_app.next_food_id = 1;
    profile_defaults(&g_app.profile);
    settings_defaults(&g_app.settings);
    load_default_foods();
    load_default_workouts();

    stone_app_mark_dirty(DIRTY_ALL);
    stone_app_save_all();
    stone_app_toast("All data reset");
    return 1;
}
