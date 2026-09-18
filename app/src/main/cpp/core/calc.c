#include "calc.h"

#include <math.h>

float stone_clampf(float v, float lo, float hi)
{
    if (v != v) return lo;            /* NaN from a corrupted file */
    return v < lo ? lo : (v > hi ? hi : v);
}

int stone_clampi(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

float stone_bmi(float weight_kg, float height_cm)
{
    float m;
    if (height_cm < 60.0f || weight_kg <= 0.0f) return 0.0f;
    m = height_cm / 100.0f;
    return weight_kg / (m * m);
}

const char *stone_bmi_category(float bmi)
{
    if (bmi <= 0.0f)  return "-";
    if (bmi < 18.5f)  return "Underweight";
    if (bmi < 25.0f)  return "Normal";
    if (bmi < 30.0f)  return "Overweight";
    return "Obese";
}

float stone_bmr(const StoneProfile *p)
{
    float w, h;
    int age;
    if (!p) return 0.0f;
    w = stone_clampf(p->weight_kg, 25.0f, 400.0f);
    h = stone_clampf(p->height_cm, 100.0f, 250.0f);
    age = stone_clampi(p->age, 10, 100);
    /* Mifflin-St Jeor */
    if (p->sex == SEX_FEMALE)
        return 10.0f * w + 6.25f * h - 5.0f * (float)age - 161.0f;
    return 10.0f * w + 6.25f * h - 5.0f * (float)age + 5.0f;
}

float stone_activity_factor(int activity)
{
    switch (activity) {
    case ACT_SEDENTARY:   return 1.200f;
    case ACT_LIGHT:       return 1.375f;
    case ACT_MODERATE:    return 1.550f;
    case ACT_ACTIVE:      return 1.725f;
    case ACT_VERY_ACTIVE: return 1.900f;
    default:              return 1.375f;
    }
}

float stone_tdee(const StoneProfile *p)
{
    return stone_bmr(p) * stone_activity_factor(p ? p->activity : ACT_LIGHT);
}

float stone_target_calories(const StoneProfile *p)
{
    float tdee = stone_tdee(p);
    float kcal = tdee;
    if (!p) return 0.0f;

    /* A conservative +/-15 % adjustment, floored so the target never drops to
       an unsafe level regardless of what the user typed. */
    if (p->goal == GOAL_LOSE) kcal = tdee * 0.85f;
    else if (p->goal == GOAL_GAIN) kcal = tdee * 1.12f;

    if (kcal < 1200.0f) kcal = 1200.0f;
    if (kcal > 5000.0f) kcal = 5000.0f;
    return kcal;
}

void stone_macro_targets(const StoneProfile *p, float kcal,
                         float *protein_g, float *carbs_g, float *fat_g)
{
    float pr = 0.25f, fa = 0.28f, ca;
    if (p) {
        if (p->goal == GOAL_LOSE)      { pr = 0.30f; fa = 0.28f; }
        else if (p->goal == GOAL_GAIN) { pr = 0.25f; fa = 0.25f; }
    }
    ca = 1.0f - pr - fa;
    if (protein_g) *protein_g = kcal * pr / 4.0f;
    if (carbs_g)   *carbs_g   = kcal * ca / 4.0f;
    if (fat_g)     *fat_g     = kcal * fa / 9.0f;
}

float stone_ideal_weight(const StoneProfile *p)
{
    float m;
    if (!p) return 0.0f;
    m = stone_clampf(p->height_cm, 100.0f, 250.0f) / 100.0f;
    return 22.0f * m * m;
}

const char *stone_activity_name(int a)
{
    switch (a) {
    case ACT_SEDENTARY:   return "Sedentary";
    case ACT_LIGHT:       return "Light";
    case ACT_MODERATE:    return "Moderate";
    case ACT_ACTIVE:      return "Active";
    case ACT_VERY_ACTIVE: return "Very Active";
    default:              return "Light";
    }
}

const char *stone_goal_name(int g)
{
    switch (g) {
    case GOAL_LOSE:     return "Lose Weight";
    case GOAL_MAINTAIN: return "Maintain";
    case GOAL_GAIN:     return "Gain Weight";
    default:            return "Maintain";
    }
}

const char *stone_level_name(int l)
{
    switch (l) {
    case LEVEL_BEGINNER:     return "Beginner";
    case LEVEL_INTERMEDIATE: return "Intermediate";
    case LEVEL_ADVANCED:     return "Advanced";
    default:                 return "Beginner";
    }
}

const char *stone_category_name(int c)
{
    switch (c) {
    case CAT_FULL_BODY:  return "Full Body";
    case CAT_UPPER_BODY: return "Upper Body";
    case CAT_LOWER_BODY: return "Lower Body";
    case CAT_CORE:       return "Core";
    case CAT_STRENGTH:   return "Strength";
    case CAT_CARDIO:     return "Cardio";
    default:             return "Full Body";
    }
}

const char *stone_meal_name(int m)
{
    switch (m) {
    case MEAL_BREAKFAST: return "Breakfast";
    case MEAL_LUNCH:     return "Lunch";
    case MEAL_DINNER:    return "Dinner";
    case MEAL_SNACK:     return "Snack";
    default:             return "Snack";
    }
}
