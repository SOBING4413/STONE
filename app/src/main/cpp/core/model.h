/* model.h - plain-C data model shared by every STONE subsystem.
 *
 * Everything is fixed-size or dynamically grown through the small vector
 * helpers in app.c; there are no external containers and no C++.
 */
#ifndef STONE_MODEL_H
#define STONE_MODEL_H

#define STONE_APP_NAME      "STONE"
#define STONE_APP_VERSION   "2.0.0"
#define STONE_AUTHOR        "Created by sobing4413"
#define STONE_STUDIO        "Exter Interactive"
#define STONE_SCHEMA        1

#define STONE_NAME_MAX      40
#define STONE_TEXT_MAX      160
#define STONE_DATE_LEN      11   /* "YYYY-MM-DD" + NUL */
#define STONE_MAX_EXERCISES 10

/* ------------------------------------------------------------------ enums */

typedef enum { SEX_MALE = 0, SEX_FEMALE = 1 } StoneSex;

typedef enum {
    ACT_SEDENTARY = 0,
    ACT_LIGHT,
    ACT_MODERATE,
    ACT_ACTIVE,
    ACT_VERY_ACTIVE,
    ACT_COUNT
} StoneActivity;

typedef enum {
    GOAL_LOSE = 0,
    GOAL_MAINTAIN,
    GOAL_GAIN,
    GOAL_COUNT
} StoneGoal;

typedef enum {
    LEVEL_BEGINNER = 0,
    LEVEL_INTERMEDIATE,
    LEVEL_ADVANCED,
    LEVEL_COUNT
} StoneLevel;

typedef enum {
    CAT_FULL_BODY = 0,
    CAT_UPPER_BODY,
    CAT_LOWER_BODY,
    CAT_CORE,
    CAT_STRENGTH,
    CAT_CARDIO,
    CAT_COUNT
} StoneCategory;

typedef enum { MEAL_BREAKFAST = 0, MEAL_LUNCH, MEAL_DINNER, MEAL_SNACK, MEAL_COUNT } StoneMeal;

/* ------------------------------------------------------------------ records */

typedef struct {
    char  name[STONE_NAME_MAX];
    int   age;
    int   sex;              /* StoneSex - used only for the BMR formula */
    float height_cm;
    float weight_kg;
    float target_weight_kg;
    int   activity;         /* StoneActivity */
    int   goal;             /* StoneGoal */
    int   weekly_workout_target;
    int   configured;       /* 1 once the user has saved the profile at least once */
} StoneProfile;

typedef struct {
    int   id;
    char  name[STONE_NAME_MAX];
    float serving_g;        /* grams (or ml) per 1 serving */
    float kcal;             /* per serving */
    float protein_g;
    float carbs_g;
    float fat_g;
    int   custom;           /* 1 = added by the user */
} StoneFood;

typedef struct {
    char  date[STONE_DATE_LEN];
    int   food_id;
    char  food_name[STONE_NAME_MAX]; /* denormalised so history survives deletes */
    float portions;
    float kcal;
    float protein_g;
    float carbs_g;
    float fat_g;
    int   meal;             /* StoneMeal */
    long  timestamp;
} StoneFoodLog;

typedef struct {
    char name[STONE_NAME_MAX];
    char description[STONE_TEXT_MAX];
    char instruction[STONE_TEXT_MAX];
    int  sets;
    int  reps;              /* 0 when the exercise is time-based */
    int  duration_s;        /* 0 when the exercise is rep-based  */
    int  rest_s;
} StoneExercise;

typedef struct {
    int           id;
    char          name[STONE_NAME_MAX];
    char          description[STONE_TEXT_MAX];
    int           level;    /* StoneLevel    */
    int           category; /* StoneCategory */
    int           est_minutes;
    float         kcal_per_min;
    int           exercise_count;
    StoneExercise exercises[STONE_MAX_EXERCISES];
} StoneWorkout;

typedef struct {
    char date[STONE_DATE_LEN];
    int  workout_id;
    char workout_name[STONE_NAME_MAX];
    int  duration_s;
    int  kcal_burned;
    long timestamp;
} StoneWorkoutLog;

typedef struct {
    char  date[STONE_DATE_LEN];
    float weight_kg;
    long  timestamp;
} StoneWeightEntry;

typedef struct {
    int use_metric;         /* 1 = kg/cm (only metric is implemented today) */
    int dark_theme;         /* 1 = dark (default), 0 = light                */
    int sound_enabled;
    int animations_enabled;
} StoneSettings;

#endif /* STONE_MODEL_H */
