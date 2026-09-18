/* defaults.h - the offline seed database shipped inside the binary.
 * No network is ever touched: these tables are the app's initial content and
 * are written out to foods.json / workouts.json on first run.
 */
#ifndef STONE_DEFAULTS_H
#define STONE_DEFAULTS_H

#include "../core/model.h"

extern const StoneFood    k_default_foods[];
extern const int          k_default_food_count;
extern const StoneWorkout k_default_workouts[];
extern const int          k_default_workout_count;

#endif /* STONE_DEFAULTS_H */
