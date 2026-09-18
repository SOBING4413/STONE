/* ui.h - immediate-mode widget layer sitting on top of render.h.
 *
 * There is no retained widget tree: every frame the active page walks its
 * layout top to bottom, asks for a rect, draws into it and reads back whether
 * the pointer interacted with it. Widget identity is the caller-supplied id,
 * which keeps press/hover tracking stable across frames.
 */
#ifndef STONE_UI_H
#define STONE_UI_H

#include "theme.h"
#include "../core/app.h"

/* ------------------------------------------------------------------ pages */
typedef enum {
    PAGE_DASHBOARD = 0,
    PAGE_DIET,
    PAGE_WORKOUT,
    PAGE_PROGRESS,
    PAGE_PROFILE,
    PAGE_HISTORY,
    PAGE_WORKOUT_DETAIL,
    PAGE_WORKOUT_SESSION,
    PAGE_ADD_FOOD,
    PAGE_ABOUT,
    PAGE_ACHIEVEMENTS,
    PAGE_ONBOARDING,
    PAGE_COUNT
} StonePage;

#define UI_NAV_TABS  5            /* first five pages live in the nav bar */
#define UI_NAV_DEPTH 8

/* --------------------------------------------------------------- keyboard */
typedef enum { KB_NONE = 0, KB_NUMERIC, KB_DECIMAL, KB_TEXT } KeyboardMode;

typedef struct {
    KeyboardMode mode;
    char         title[48];
    char         buffer[64];
    int          active;
    int          shift;
    int          symbols;         /* text mode: digits/punctuation page */
    /* Set for exactly the frame in which the keyboard was opened. The tap
       that opens it is still travelling through the same frame, and without
       this guard the panel's own scrim would read that release as "user
       tapped outside" and close the keyboard again before it ever appeared -
       which is what made every edit field look broken. */
    int          just_opened;
    void        *target;          /* opaque: who asked for the input   */
    int          target_kind;     /* caller-defined tag                */
    float        anim;
} UiKeyboard;

/* ------------------------------------------------------------------ input */
typedef struct {
    float x, y;
    float down_x, down_y;
    float move_dx, move_dy;
    int   down;            /* pointer currently on the glass */
    int   pressed;         /* went down this frame           */
    int   released;        /* came up this frame             */
    int   dragged;         /* moved beyond the tap threshold */
    float hold_time;
} UiInput;

/* ------------------------------------------------------------------ state */
typedef struct {
    float offset;
    float velocity;
    float content_h;
    float view_h;
    int   grabbing;
} UiScroll;

typedef struct {
    StonePage page;
    StonePage nav_stack[UI_NAV_DEPTH];
    int       nav_depth;

    UiScroll  scroll[PAGE_COUNT];
    UiInput   input;
    UiKeyboard keyboard;

    float     dt;
    float     time;
    float     page_anim;          /* 0..1 page-enter animation */
    float     scale;              /* dp -> px factor           */
    Rect      safe;               /* content area minus nav bar and insets */

    float     nav_indicator_x;    /* eased x of the nav-bar highlight pill */
    float     splash_timer;       /* counts down from launch; splash while > 0 */

    /* System safe area in pixels, pushed up from StoneActivity.java. Zero on
       every edge once the status and navigation bars are hidden, non-zero the
       moment the system puts one back on screen - which is exactly when the
       header and the tab bar have to move out of the way. */
    float     inset_left, inset_top, inset_right, inset_bottom;

    /* selections shared between pages */
    int   selected_workout;
    int   selected_level;         /* -1 = all */
    int   selected_category;      /* -1 = all */
    int   selected_meal;
    int   selected_food;
    float food_portions;
    char  food_query[32];
    int   history_day_offset;
    int   progress_tab;           /* 0 weight, 1 calories, 2 workouts */

    /* workout session */
    int   session_active;
    int   session_exercise;
    int   session_set;
    int   session_resting;
    float session_timer;          /* counts down inside a timed block */
    float session_elapsed;        /* total seconds of the session     */

    /* add-food form */
    char  form_name[STONE_NAME_MAX];
    float form_serving, form_kcal, form_protein, form_carbs, form_fat;

    int   confirm_reset;
    int   active_id;              /* widget currently pressed */

    /* first-run setup wizard */
    int   onboard_step;
    float onboard_anim;

    int   achievement_focus;      /* -1 = none expanded */
} UiState;

UiState *ui_state(void);

void ui_init(void);
void ui_frame(float dt, int width, int height);   /* draws the whole frame */
void ui_navigate(StonePage page);
int  ui_back(void);                               /* 1 = handled, 0 = exit app */

/* pointer plumbing, called from the Android input callback */
void ui_pointer_down(float x, float y);
void ui_pointer_move(float x, float y);
void ui_pointer_up(float x, float y);
void ui_pointer_cancel(void);

/* ----------------------------------------------------------- layout utils */
float ui_dp(float v);
Rect  ui_row(Rect *cursor, float height, float gap);  /* carve a row off the top */
Rect  ui_split_left(Rect r, float w, float gap);
Rect  ui_split_right(Rect r, float w, float gap);

/* -------------------------------------------------------------- primitives */
void ui_card(Rect r);
void ui_card_shadowed(Rect r);
void ui_section_title(Rect *cursor, const char *title);
int  ui_button(int id, Rect r, const char *label, int primary);
int  ui_ghost_button(int id, Rect r, const char *label);
int  ui_danger_button(int id, Rect r, const char *label);
int  ui_chip(int id, Rect r, const char *label, int selected);
int  ui_touch_area(int id, Rect r);      /* invisible tap target */
void ui_progress_bar(Rect r, float t, Color fill);
/* Draws one tile of the badge atlas, dimmed when the badge is still locked.
   Falls back to a vector medallion when the asset pack is not present. */
void ui_badge(Rect r, int tile, int unlocked);
/* Header artwork band for a tab, sampled out of the "hero" atlas. */
void ui_hero_band(Rect r, int tab, float alpha);
void ui_stat_tile(Rect r, const char *label, const char *value, const char *sub, Color accent);
void ui_ring_stat(float cx, float cy, float radius, float t, const char *value,
                  const char *label, Color c);
void ui_kv_row(Rect r, const char *key, const char *value);
void ui_empty_state(Rect r, const char *title, const char *hint);
void ui_toast(void);
void ui_disclaimer(Rect *cursor, float width);

/* ------------------------------------------------------------- scrolling */
Rect ui_scroll_begin(StonePage page, Rect area, float content_h);
void ui_scroll_set_content(StonePage page, float height);
void ui_scroll_end(void);
int  ui_scroll_is_dragging(void);

/* -------------------------------------------------------------- keyboard */
void ui_keyboard_open(KeyboardMode mode, const char *title, const char *initial,
                      int target_kind, void *target);
void ui_keyboard_draw(void);
int  ui_keyboard_active(void);
/* Consumes the committed value once; returns 0 when nothing was committed. */
int  ui_keyboard_take(int *out_kind, void **out_target, char *out_text, int out_len);

/* ------------------------------------------------------------------ pages */
void page_dashboard(Rect area);
void page_diet(Rect area);
void page_workout(Rect area);
void page_workout_detail(Rect area);
void page_workout_session(Rect area);
void page_progress(Rect area);
void page_history(Rect area);
void page_profile(Rect area);
void page_add_food(Rect area);
void page_about(Rect area);
void page_achievements(Rect area);
void page_onboarding(Rect area);

/* Page-owned handling of committed keyboard values. */
void ui_handle_input_commit(int kind, void *target, const char *text);

/* keyboard target kinds */
enum {
    KB_TARGET_NONE = 0,
    KB_TARGET_WEIGHT,
    KB_TARGET_TARGET_WEIGHT,
    KB_TARGET_HEIGHT,
    KB_TARGET_AGE,
    KB_TARGET_NAME,
    KB_TARGET_WEEKLY,
    KB_TARGET_PORTIONS,
    KB_TARGET_FOOD_SEARCH,
    KB_TARGET_FORM_NAME,
    KB_TARGET_FORM_SERVING,
    KB_TARGET_FORM_KCAL,
    KB_TARGET_FORM_PROTEIN,
    KB_TARGET_FORM_CARBS,
    KB_TARGET_FORM_FAT
};

#endif /* STONE_UI_H */
