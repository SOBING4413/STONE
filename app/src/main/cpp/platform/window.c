/* platform/window.c - see window.h. */
#include "window.h"

#include <jni.h>
#include <android/log.h>
#include <android_native_app_glue.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "STONE", __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, "STONE", __VA_ARGS__)

/* Plain ints written by the UI thread and read by the render thread. Both are
   naturally aligned 32 bit loads/stores, which are single instructions and
   never tear on any ABI this app ships for; a torn *set* of four would at
   worst mean one frame laid out with last frame's bottom inset, so no lock is
   warranted on a value that changes about twice per app launch. */
static volatile int   g_left, g_top, g_right, g_bottom;
static volatile float g_density = 1.0f;
static volatile int   g_bridge;          /* Java side checked in */

JNIEXPORT void JNICALL
Java_com_stone_app_StoneActivity_nativeSetInsets(JNIEnv *env, jclass cls,
                                                 jint left, jint top,
                                                 jint right, jint bottom,
                                                 jfloat density)
{
    (void)env; (void)cls;

    /* Clamp: a broken OEM inset must never push the whole UI off screen. */
    if (left   < 0) left   = 0;
    if (top    < 0) top    = 0;
    if (right  < 0) right  = 0;
    if (bottom < 0) bottom = 0;
    if (left   > 400) left   = 400;
    if (top    > 400) top    = 400;
    if (right  > 400) right  = 400;
    if (bottom > 400) bottom = 400;

    g_left   = left;
    g_top    = top;
    g_right  = right;
    g_bottom = bottom;
    if (density > 0.5f && density < 8.0f) g_density = density;
    if (!g_bridge) {
        g_bridge = 1;
        LOGI("window: Java inset bridge is live (density %.2f)", (double)density);
    }
}

StoneInsets stone_window_insets(void)
{
    StoneInsets in;
    in.left   = (float)g_left;
    in.top    = (float)g_top;
    in.right  = (float)g_right;
    in.bottom = (float)g_bottom;
    return in;
}

float stone_window_density(void)      { return g_density; }
int   stone_window_has_java_bridge(void) { return g_bridge; }

/* ------------------------------------------------------------- fallback --
 * Only reached when the APK was built without StoneActivity (stock
 * NativeActivity, android:hasCode="false"). Reproduces the legacy
 * View.SYSTEM_UI_FLAG_* immersive request by value, because there is no Java
 * classpath to read the constants from.
 */
#define SYSUI_HIDE_NAVIGATION        0x00000002
#define SYSUI_FULLSCREEN             0x00000004
#define SYSUI_LAYOUT_STABLE          0x00000100
#define SYSUI_LAYOUT_HIDE_NAVIGATION 0x00000200
#define SYSUI_LAYOUT_FULLSCREEN      0x00000400
#define SYSUI_IMMERSIVE_STICKY       0x00001000

#define SYSUI_IMMERSIVE (SYSUI_LAYOUT_STABLE | SYSUI_LAYOUT_HIDE_NAVIGATION | \
                         SYSUI_LAYOUT_FULLSCREEN | SYSUI_HIDE_NAVIGATION |    \
                         SYSUI_FULLSCREEN | SYSUI_IMMERSIVE_STICKY)

static void clear_exception(JNIEnv *env)
{
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
    }
}

static void fallback_immersive(struct android_app *app)
{
    JavaVM *vm;
    JNIEnv *env = NULL;
    jint    res;
    int     attached = 0;
    jobject activity, window = NULL, decor = NULL;
    jclass  acls = NULL, wcls = NULL, vcls = NULL;
    jmethodID mid;

    if (!app || !app->activity || !app->activity->vm || !app->activity->clazz) return;

    vm = app->activity->vm;
    activity = app->activity->clazz;

    res = (*vm)->GetEnv(vm, (void **)&env, JNI_VERSION_1_6);
    if (res == JNI_EDETACHED) {
        if ((*vm)->AttachCurrentThread(vm, &env, NULL) != JNI_OK) return;
        attached = 1;
    } else if (res != JNI_OK) {
        return;
    }

    acls = (*env)->GetObjectClass(env, activity);
    if (!acls) { clear_exception(env); goto done; }
    mid = (*env)->GetMethodID(env, acls, "getWindow", "()Landroid/view/Window;");
    if (!mid) { clear_exception(env); goto done; }
    window = (*env)->CallObjectMethod(env, activity, mid);
    if (!window) { clear_exception(env); goto done; }

    wcls = (*env)->GetObjectClass(env, window);
    if (!wcls) { clear_exception(env); goto done; }
    mid = (*env)->GetMethodID(env, wcls, "getDecorView", "()Landroid/view/View;");
    if (!mid) { clear_exception(env); goto done; }
    decor = (*env)->CallObjectMethod(env, window, mid);
    if (!decor) { clear_exception(env); goto done; }

    vcls = (*env)->GetObjectClass(env, decor);
    if (!vcls) { clear_exception(env); goto done; }
    mid = (*env)->GetMethodID(env, vcls, "setSystemUiVisibility", "(I)V");
    if (!mid) { clear_exception(env); goto done; }

    (*env)->CallVoidMethod(env, decor, mid, (jint)SYSUI_IMMERSIVE);
    clear_exception(env);

done:
    if (decor)  (*env)->DeleteLocalRef(env, decor);
    if (window) (*env)->DeleteLocalRef(env, window);
    if (vcls)   (*env)->DeleteLocalRef(env, vcls);
    if (wcls)   (*env)->DeleteLocalRef(env, wcls);
    if (acls)   (*env)->DeleteLocalRef(env, acls);
    if (attached) (*vm)->DetachCurrentThread(vm);
}

void stone_window_init(struct android_app *app)
{
    if (!g_bridge) {
        LOGW("window: no Java bridge yet, using the native immersive fallback");
        fallback_immersive(app);
    }
}

void stone_window_reapply(struct android_app *app)
{
    /* When StoneActivity is present it already re-arms immersive mode on the
       UI thread at onResume / onWindowFocusChanged, which is both earlier and
       more reliable than anything this thread can do. Staying out of the way
       is the correct behaviour, not a missing feature. */
    if (g_bridge) return;
    fallback_immersive(app);
}
