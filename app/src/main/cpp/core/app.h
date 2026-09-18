/* app.h - the whole application state plus every mutation that touches it.
 *
 * The UI layer never writes to the arrays directly; it calls the functions
 * below, which keep the in-memory state and the JSON files in sync.
 */
#ifndef STONE_APP_H
#define STONE_APP_H

#include "model.h"
#include "../storage/json.h"

/* File names inside the app's private directory. */
#define FILE_PROFILE   "profile.json"
#define FILE_FOODS     "foods.json"
#define FILE_WORKOUTS   "workouts.json"
#define FILE_HISTORY   "history.json"
#define FILE_PROGRESS  "progress.json"
#define FILE_SETTINGS  "settings.json"
#define FILE_BACKUP    "stone_backup.json"

typedef struct {
    StoneProfile      profile;
    StoneSettings     settings;

    StoneFood        *foods;        int food_count,     food_cap;
    StoneWorkout     *workouts;     int workout_count,  workout_cap;
    StoneFoodLog     *food_log;     int food_log_count, food_log_cap;
    StoneWorkoutLog  *workout_log;  int workout_log_count, workout_log_cap;
    StoneWeightEntry *weights;      int weight_count,   weight_cap;

    int   next_food_id;
    int   dirty_mask;               /* bitfield of pending saves */
    char  today[STONE_DATE_LEN];
    char  status[STONE_TEXT_MAX];   /* last user-visible message (toast) */
    float status_timer;             /* seconds remaining on the toast     */
    int   storage_warning;          /* 1 when data could not be persisted  */
} StoneApp;

enum {
    DIRTY_PROFILE  = 1 << 0,
    DIRTY_FOODS    = 1 << 1,
    DIRTY_WORKOUTS = 1 << 2,
    DIRTY_HISTORY  = 1 << 3,
    DIRTY_PROGRESS = 1 << 4,
    DIRTY_SETTINGS = 1 << 5,
    DIRTY_ALL      = 0x3F
};

StoneApp *stone_app(void);                 /* process-wide singleton */

void stone_app_init(const char *internal_dir, const char *external_dir);
void stone_app_shutdown(void);
void stone_app_tick(float dt);             /* toast timer + autosave */
void stone_app_save_all(void);             /* called on pause/stop too */
void stone_app_mark_dirty(int mask);
void stone_app_toast(const char *fmt, ...);

/* --- dates ---------------------------------------------------------------- */
void stone_today_string(char *out, int len);
long stone_now(void);
int  stone_date_to_days(const char *date);            /* days since 1970-01-01 */
void stone_days_to_date(int days, char *out, int len);
int  stone_date_diff(const char *a, const char *b);   /* a - b, in days */
void stone_date_short(const char *date, char *out, int len); /* "12 Mar" */
const char *stone_weekday(const char *date);          /* "Mon" ... "Sun" */

/* --- diet ----------------------------------------------------------------- */
const StoneFood *stone_find_food(int id);
int   stone_add_custom_food(const char *name, float serving_g, float kcal,
                            float protein, float carbs, float fat);
int   stone_delete_food(int id);
int   stone_log_food(int food_id, float portions, int meal);
int   stone_remove_food_log(int index);
void  stone_day_nutrition(const char *date, float *kcal, float *protein,
                          float *carbs, float *fat);
int   stone_food_search(const char *query, int *out_indices, int max);

/* --- workout -------------------------------------------------------------- */
const StoneWorkout *stone_find_workout(int id);
int   stone_log_workout(int workout_id, int duration_s);
int   stone_remove_workout_log(int index);
int   stone_workouts_on(const char *date);
int   stone_workouts_this_week(void);
int   stone_kcal_burned_on(const char *date);

/* --- progress ------------------------------------------------------------- */
int   stone_add_weight(float kg);          /* one entry per day; replaces same-day */
float stone_latest_weight(void);
float stone_weight_change_7d(void);
int   stone_active_streak(void);           /* consecutive days with any activity */
int   stone_active_days_total(void);
int   stone_weight_series(int max_days, float *out_kg, char out_dates[][STONE_DATE_LEN], int cap);
int   stone_calorie_series(int days, float *out_kcal, char out_dates[][STONE_DATE_LEN], int cap);
int   stone_workout_week_series(int weeks, int *out_counts, int cap);

/* --- profile -------------------------------------------------------------- */
void  stone_save_profile(void);

/* --- backup --------------------------------------------------------------- */
int   stone_backup_export(char *out_path, int out_len);
int   stone_backup_import(const char *path);
int   stone_reset_all_data(void);

#endif /* STONE_APP_H */
