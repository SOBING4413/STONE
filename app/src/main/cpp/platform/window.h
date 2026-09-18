/* platform/window.h - system bars and the safe area.
 *
 * STONE draws edge to edge. Two things have to be true for that to look right
 * instead of broken:
 *
 *   1. the status bar and the 3-button / gesture navigation bar are hidden
 *      (immersive sticky), and
 *   2. when they are on screen anyway - the user swiped them up, the device
 *      forces a gesture handle, an accessibility setting pins them - the app
 *      knows exactly how many pixels they occupy so its own header and bottom
 *      tab bar can sit inside the remainder instead of underneath them.
 *
 * (1) is owned by StoneActivity.java, because those calls must run on the
 * Activity's UI thread and android_main() does not. (2) arrives here through
 * Java_com_stone_app_StoneActivity_nativeSetInsets().
 *
 * If the app is ever launched through the stock android.app.NativeActivity
 * again (no Java class, no bridge), the fallback in window.c re-applies the
 * legacy SYSTEM_UI_FLAG_* immersive set over JNI so the build still behaves.
 *
 * Created by sobing4413 - Exter Interactive.
 */
#ifndef STONE_PLATFORM_WINDOW_H
#define STONE_PLATFORM_WINDOW_H

struct android_app;

typedef struct {
    float left, top, right, bottom;   /* pixels */
} StoneInsets;

/* Called once from android_main(). */
void stone_window_init(struct android_app *app);

/* Called on APP_CMD_INIT_WINDOW and APP_CMD_GAINED_FOCUS. Does nothing when
   StoneActivity.java is driving the window; otherwise runs the JNI fallback. */
void stone_window_reapply(struct android_app *app);

/* Safe area in pixels. All zeroes means "the app owns the whole screen",
   which is the normal state once the bars are hidden. */
StoneInsets stone_window_insets(void);

/* Screen density reported by Java (1.0 until the bridge checks in). */
float stone_window_density(void);

/* 1 once StoneActivity has delivered insets at least once. */
int stone_window_has_java_bridge(void);

#endif /* STONE_PLATFORM_WINDOW_H */
