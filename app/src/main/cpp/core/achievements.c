/* achievements.c - see achievements.h. */
#include "achievements.h"

#include <math.h>
#include <string.h>

#include "app.h"
#include "calc.h"

/* Order matters: `tile` is the position in the badge atlas, and the table is
   laid out so the page reads as a difficulty ramp down the grid. */
static const StoneAchievement k_achievements[STONE_ACHIEVEMENT_COUNT] = {
    { "First Spark",    "Stay active three days in a row",      "days",    0,     3 },
    { "Week Warrior",   "A seven day active streak",            "days",    1,     7 },
    { "Unstoppable",    "A thirty day active streak",           "days",    2,    30 },
    { "Heart Starter",  "Finish your first workout",            "workouts",3,     1 },

    { "Iron Habit",     "Finish ten workouts",                  "workouts",4,    10 },
    { "Clean Plate",    "Log ten meals",                        "meals",   5,    10 },
    { "Macro Tracker",  "Log fifty meals",                      "meals",   6,    50 },
    { "On The Scale",   "Record five weigh-ins",                "entries", 7,     5 },

    { "Centurion",      "Finish twenty five workouts",          "workouts",8,    25 },
    { "Consistency",    "Hit your weekly workout target",       "workouts",9,     1 },
    { "Ten Hours",      "Ten hours of logged training",         "hours",  10,    10 },
    { "Meal Prep",      "Log one hundred meals",                "meals",  11,   100 },

    { "Kilo Crusher",   "Burn one thousand kcal in total",      "kcal",   12,  1000 },
    { "Furnace",        "Burn ten thousand kcal in total",      "kcal",   13, 10000 },
    { "Dedicated",      "Thirty days with something logged",    "days",   14,    30 },
    { "Goal Reached",   "Arrive at your target weight",         "%",      15,   100 },
};

const StoneAchievement *stone_achievement(int index)
{
    if (index < 0 || index >= STONE_ACHIEVEMENT_COUNT) return NULL;
    return &k_achievements[index];
}

/* ------------------------------------------------------------ aggregates */

static int total_workouts(void)
{
    return stone_app()->workout_log_count;
}

static int total_meals(void)
{
    return stone_app()->food_log_count;
}

static int total_training_hours(void)
{
    const StoneApp *a = stone_app();
    long seconds = 0;
    int i;
    for (i = 0; i < a->workout_log_count; ++i)
        seconds += (long)a->workout_log[i].duration_s;
    return (int)(seconds / 3600);
}

static int total_kcal_burned(void)
{
    const StoneApp *a = stone_app();
    long kcal = 0;
    int i;
    for (i = 0; i < a->workout_log_count; ++i)
        kcal += (long)a->workout_log[i].kcal_burned;
    if (kcal > 2000000L) kcal = 2000000L;
    return (int)kcal;
}

/* Percentage of the distance from the starting weight to the target weight
   that has actually been covered. The "starting" weight is the oldest entry
   on record, so the number is meaningful in both directions: cutting and
   bulking both count as progress towards the goal the user set. */
static int goal_percent(void)
{
    const StoneApp *a = stone_app();
    float start, now, target, span, done;

    if (a->weight_count == 0) return 0;

    start  = a->weights[0].weight_kg;
    now    = stone_latest_weight();
    target = a->profile.target_weight_kg;

    span = target - start;
    if (fabsf(span) < 0.05f)
        return (fabsf(now - target) < 0.5f) ? 100 : 0;

    done = (now - start) / span;
    if (done < 0.0f) done = 0.0f;
    if (done > 1.0f) done = 1.0f;
    return (int)(done * 100.0f + 0.5f);
}

int stone_achievement_value(int index)
{
    const StoneAchievement *a = stone_achievement(index);
    int v = 0;

    if (!a) return 0;

    switch (index) {
    case 0: case 1: case 2: v = stone_active_streak();     break;
    case 3: case 4: case 8: v = total_workouts();          break;
    case 5: case 6: case 11:v = total_meals();             break;
    case 7:                 v = stone_app()->weight_count; break;
    case 9:                 v = (stone_workouts_this_week() >=
                                 stone_app()->profile.weekly_workout_target) ? 1 : 0; break;
    case 10:                v = total_training_hours();    break;
    case 12: case 13:       v = total_kcal_burned();       break;
    case 14:                v = stone_active_days_total(); break;
    case 15:                v = goal_percent();            break;
    default:                v = 0;                         break;
    }

    if (v < 0) v = 0;
    if (v > a->goal) v = a->goal;
    return v;
}

int stone_achievement_unlocked(int index)
{
    const StoneAchievement *a = stone_achievement(index);
    if (!a) return 0;
    return stone_achievement_value(index) >= a->goal;
}

int stone_achievements_unlocked_count(void)
{
    int i, n = 0;
    for (i = 0; i < STONE_ACHIEVEMENT_COUNT; ++i)
        if (stone_achievement_unlocked(i)) n++;
    return n;
}

float stone_achievements_ratio(void)
{
    return (float)stone_achievements_unlocked_count() / (float)STONE_ACHIEVEMENT_COUNT;
}

int stone_achievement_next(void)
{
    int i, best = -1;
    float best_ratio = -1.0f;

    for (i = 0; i < STONE_ACHIEVEMENT_COUNT; ++i) {
        const StoneAchievement *a = &k_achievements[i];
        float r;
        if (stone_achievement_unlocked(i)) continue;
        r = a->goal > 0 ? (float)stone_achievement_value(i) / (float)a->goal : 0.0f;
        if (r > best_ratio) { best_ratio = r; best = i; }
    }
    return best;
}
