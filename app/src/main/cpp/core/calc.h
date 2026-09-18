/* calc.h - fitness estimates. General wellness math only, never a diagnosis. */
#ifndef STONE_CALC_H
#define STONE_CALC_H

#include "model.h"

float       stone_bmi(float weight_kg, float height_cm);
const char *stone_bmi_category(float bmi);
float       stone_bmr(const StoneProfile *p);          /* Mifflin-St Jeor */
float       stone_tdee(const StoneProfile *p);         /* BMR x activity  */
float       stone_target_calories(const StoneProfile *p);
float       stone_activity_factor(int activity);
void        stone_macro_targets(const StoneProfile *p, float kcal,
                                float *protein_g, float *carbs_g, float *fat_g);
float       stone_ideal_weight(const StoneProfile *p); /* BMI 22 reference */
float       stone_clampf(float v, float lo, float hi);
int         stone_clampi(int v, int lo, int hi);

const char *stone_activity_name(int activity);
const char *stone_goal_name(int goal);
const char *stone_level_name(int level);
const char *stone_category_name(int category);
const char *stone_meal_name(int meal);

#endif /* STONE_CALC_H */
