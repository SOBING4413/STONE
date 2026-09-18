/* achievements.h - derived milestones, no extra storage.
 *
 * Every badge is recomputed from the logs that already exist on disk, so
 * there is no achievements.json to keep in sync, nothing to migrate, and a
 * restored backup lights the right badges up the moment it loads. The badge
 * artwork lives in the 4x4 atlas named "badges" inside stone_pack.stpk and
 * `tile` is the index into it, read left to right, top to bottom.
 *
 * Created by sobing4413 - Exter Interactive.
 */
#ifndef STONE_ACHIEVEMENTS_H
#define STONE_ACHIEVEMENTS_H

typedef struct {
    const char *name;
    const char *detail;    /* what unlocks it, in one short line */
    const char *unit;      /* appended to the progress readout   */
    int         tile;      /* 0..15 into the badge atlas         */
    int         goal;
} StoneAchievement;

#define STONE_ACHIEVEMENT_COUNT 16

const StoneAchievement *stone_achievement(int index);

/* Current value of the achievement's metric, clamped at its goal. */
int  stone_achievement_value(int index);
int  stone_achievement_unlocked(int index);
int  stone_achievements_unlocked_count(void);

/* 0..1 completion of the whole set, for the summary ring. */
float stone_achievements_ratio(void);

/* Index of the badge the user is closest to unlocking but has not yet, or -1
   when everything is already unlocked. */
int  stone_achievement_next(void);

#endif /* STONE_ACHIEVEMENTS_H */
