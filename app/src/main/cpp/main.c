/* main.c - Android entry point for STONE.
 *
 * The whole application is a NativeActivity: there is no Java source in this
 * project at all. android_native_app_glue runs the looper on a dedicated
 * thread and hands us lifecycle commands plus raw input events; everything we
 * do from there is plain C.
 *
 * Responsibilities kept in this file, and nowhere else:
 *   - EGL surface/context creation and teardown (including context loss)
 *   - the frame pump and the delta-time clock
 *   - translating AInputEvent into the UI layer's pointer/back calls
 *   - flushing data to disk when Android tells us we are going away
 *
 * Created by sobing4413 - Exter Interactive.
 */
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include <android/log.h>
#include <android/input.h>
#include <android/keycodes.h>
#include <android/looper.h>
#include <android/native_activity.h>
#include <android/native_window.h>
#include <android/configuration.h>
#include <android/asset_manager.h>
#include <android_native_app_glue.h>

#include "core/app.h"
#include "renderer/render.h"
#include "ui/ui.h"
#include "platform/window.h"
#include "platform/assets.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  "STONE", __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  "STONE", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "STONE", __VA_ARGS__)

typedef struct {
    struct android_app *app;

    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    EGLConfig  config;

    int   width, height;
    int   gl_ready;          /* render_init() succeeded with a live context */
    int   app_ready;         /* stone_app_init() has run                    */
    int   animating;         /* window is visible and we should draw        */
    int   have_config;

    double last_time;
} StoneEngine;

/* ------------------------------------------------------------------ clock */

static double now_seconds(void)
{
    struct timespec ts;
#if defined(CLOCK_MONOTONIC)
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
        return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
#endif
    return (double)time(NULL);
}

/* -------------------------------------------------------------------- EGL */

static int egl_pick_config(StoneEngine *e)
{
    /* We ask for a plain 24/8 RGB config first: no depth buffer is needed
       because the renderer is a 2D painter's-algorithm batcher. If the device
       cannot give us that we fall back to 16 bit. */
    static const EGLint attribs_24[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
        EGL_BLUE_SIZE,  8,
        EGL_GREEN_SIZE, 8,
        EGL_RED_SIZE,   8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 0,
        EGL_STENCIL_SIZE, 0,
        EGL_NONE
    };
    static const EGLint attribs_16[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
        EGL_BLUE_SIZE,  5,
        EGL_GREEN_SIZE, 6,
        EGL_RED_SIZE,   5,
        EGL_NONE
    };
    EGLint count = 0;

    if (eglChooseConfig(e->display, attribs_24, &e->config, 1, &count) && count > 0)
        return 1;
    count = 0;
    if (eglChooseConfig(e->display, attribs_16, &e->config, 1, &count) && count > 0) {
        LOGW("falling back to a 16 bit EGL config");
        return 1;
    }
    LOGE("no usable EGL config on this device");
    return 0;
}

static int engine_init_display(StoneEngine *e)
{
    const EGLint ctx_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    EGLint format = 0, w = 0, h = 0;

    if (e->app->window == NULL) return 0;

    if (e->display == EGL_NO_DISPLAY) {
        e->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (e->display == EGL_NO_DISPLAY) { LOGE("eglGetDisplay failed"); return 0; }
        if (!eglInitialize(e->display, NULL, NULL)) {
            LOGE("eglInitialize failed (0x%x)", eglGetError());
            e->display = EGL_NO_DISPLAY;
            return 0;
        }
        e->have_config = 0;
    }

    if (!e->have_config) {
        if (!egl_pick_config(e)) return 0;
        e->have_config = 1;
    }

    /* ANativeWindow must be told which pixel format the config expects. */
    eglGetConfigAttrib(e->display, e->config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(e->app->window, 0, 0, format);

    if (e->surface == EGL_NO_SURFACE) {
        e->surface = eglCreateWindowSurface(e->display, e->config, e->app->window, NULL);
        if (e->surface == EGL_NO_SURFACE) {
            LOGE("eglCreateWindowSurface failed (0x%x)", eglGetError());
            return 0;
        }
    }

    if (e->context == EGL_NO_CONTEXT) {
        e->context = eglCreateContext(e->display, e->config, EGL_NO_CONTEXT, ctx_attribs);
        if (e->context == EGL_NO_CONTEXT) {
            LOGE("eglCreateContext failed (0x%x)", eglGetError());
            return 0;
        }
        e->gl_ready = 0;   /* brand new context: GL objects must be rebuilt */
    }

    if (!eglMakeCurrent(e->display, e->surface, e->surface, e->context)) {
        LOGE("eglMakeCurrent failed (0x%x)", eglGetError());
        return 0;
    }

    eglQuerySurface(e->display, e->surface, EGL_WIDTH,  &w);
    eglQuerySurface(e->display, e->surface, EGL_HEIGHT, &h);
    e->width  = (w > 0) ? w : 1;
    e->height = (h > 0) ? h : 1;

    /* Do not fight the compositor: a swap interval of 1 keeps us at the
       display refresh rate and keeps battery drain predictable. */
    eglSwapInterval(e->display, 1);

    if (!e->gl_ready) {
        if (!render_init()) {
            LOGE("renderer initialisation failed");
            return 0;
        }
        e->gl_ready = 1;
        LOGI("GL ready: %s / %s", (const char *)glGetString(GL_RENDERER),
             (const char *)glGetString(GL_VERSION));
    }

    e->animating = 1;
    e->last_time = now_seconds();
    return 1;
}

/* Releases the surface (and the context when the process is being torn down)
   while leaving the application model untouched. */
static void engine_term_surface(StoneEngine *e, int drop_context)
{
    if (e->display == EGL_NO_DISPLAY) return;

    if (drop_context && e->gl_ready) {
        render_shutdown();
        e->gl_ready = 0;
    }

    eglMakeCurrent(e->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    if (e->surface != EGL_NO_SURFACE) {
        eglDestroySurface(e->display, e->surface);
        e->surface = EGL_NO_SURFACE;
    }
    if (drop_context && e->context != EGL_NO_CONTEXT) {
        eglDestroyContext(e->display, e->context);
        e->context = EGL_NO_CONTEXT;
    }
    e->animating = 0;
}

static void engine_term_display(StoneEngine *e)
{
    engine_term_surface(e, 1);
    if (e->display != EGL_NO_DISPLAY) {
        eglTerminate(e->display);
        e->display = EGL_NO_DISPLAY;
    }
    e->have_config = 0;
}

/* ------------------------------------------------------------------ frame */

static void engine_draw(StoneEngine *e)
{
    double now;
    float dt;

    if (!e->animating || e->display == EGL_NO_DISPLAY || !e->gl_ready) return;

    now = now_seconds();
    dt  = (float)(now - e->last_time);
    e->last_time = now;
    if (dt < 0.0f)    dt = 0.0f;
    if (dt > 0.25f)   dt = 0.25f;   /* a stall must not teleport the timers */

    stone_app_tick(dt);
    ui_frame(dt, e->width, e->height);

    if (!eglSwapBuffers(e->display, e->surface)) {
        EGLint err = eglGetError();
        if (err == EGL_CONTEXT_LOST) {
            /* Rare, but real on some drivers after a GPU reset: throw away
               every GL object and rebuild on the next window event. */
            LOGW("EGL context lost, rebuilding");
            render_shutdown();
            e->gl_ready = 0;
            engine_term_surface(e, 1);
            engine_init_display(e);
        } else if (err == EGL_BAD_SURFACE) {
            LOGW("EGL surface went bad, recreating");
            engine_term_surface(e, 0);
            engine_init_display(e);
        } else {
            LOGE("eglSwapBuffers failed (0x%x)", err);
        }
    }
}

/* ------------------------------------------------------------------ input */

static int32_t on_input(struct android_app *app, AInputEvent *event)
{
    StoneEngine *e = (StoneEngine *)app->userData;
    int32_t type = AInputEvent_getType(event);

    if (type == AINPUT_EVENT_TYPE_MOTION) {
        int32_t action = AMotionEvent_getAction(event);
        int32_t masked = action & AMOTION_EVENT_ACTION_MASK;
        size_t  index  = (size_t)((action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                                  >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);
        float x, y;

        /* Single-pointer UI: anything beyond the first finger is ignored so a
           stray palm touch cannot hijack a press. */
        if (masked == AMOTION_EVENT_ACTION_POINTER_DOWN ||
            masked == AMOTION_EVENT_ACTION_POINTER_UP) {
            if (AMotionEvent_getPointerId(event, index) != 0) return 1;
        } else {
            index = 0;
        }
        if (AMotionEvent_getPointerCount(event) <= index) return 1;

        x = AMotionEvent_getX(event, index);
        y = AMotionEvent_getY(event, index);

        switch (masked) {
        case AMOTION_EVENT_ACTION_DOWN:
        case AMOTION_EVENT_ACTION_POINTER_DOWN:
            ui_pointer_down(x, y);
            break;
        case AMOTION_EVENT_ACTION_MOVE:
            ui_pointer_move(x, y);
            break;
        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_POINTER_UP:
            ui_pointer_up(x, y);
            break;
        case AMOTION_EVENT_ACTION_CANCEL:
        default:
            ui_pointer_cancel();
            break;
        }
        if (e) e->animating = 1;
        return 1;
    }

    if (type == AINPUT_EVENT_TYPE_KEY) {
        int32_t code   = AKeyEvent_getKeyCode(event);
        int32_t action = AKeyEvent_getAction(event);

        if (code == AKEYCODE_BACK) {
            /* Consume the press, act on the release: that is what users
               expect and it stops auto-repeat from popping several pages. */
            if (action == AKEY_EVENT_ACTION_UP) {
                if (!ui_back()) {
                    stone_app_save_all();
                    ANativeActivity_finish(app->activity);
                }
            }
            return 1;
        }
    }
    return 0;
}

/* -------------------------------------------------------------- lifecycle */

static void on_cmd(struct android_app *app, int32_t cmd)
{
    StoneEngine *e = (StoneEngine *)app->userData;

    switch (cmd) {
    case APP_CMD_INIT_WINDOW:
        if (app->window != NULL) {
            if (engine_init_display(e))
                engine_draw(e);
            /* No-op when StoneActivity.java is present, which is the normal
               case: hiding the system bars is its job precisely because it
               owns the UI thread and this one does not. */
            stone_window_reapply(app);
        }
        break;

    case APP_CMD_TERM_WINDOW:
        /* The window is gone but the process lives on. Keep the EGL context
           so returning from the recents screen is instant; only the surface
           is invalid. */
        engine_term_surface(e, 0);
        break;

    case APP_CMD_WINDOW_RESIZED:
    case APP_CMD_CONFIG_CHANGED:
        if (e->display != EGL_NO_DISPLAY && e->surface != EGL_NO_SURFACE) {
            EGLint w = 0, h = 0;
            eglQuerySurface(e->display, e->surface, EGL_WIDTH,  &w);
            eglQuerySurface(e->display, e->surface, EGL_HEIGHT, &h);
            if (w > 0 && h > 0) { e->width = w; e->height = h; }
        }
        break;

    case APP_CMD_GAINED_FOCUS:
        e->animating  = 1;
        e->last_time  = now_seconds();
        stone_window_reapply(app);
        break;

    case APP_CMD_LOST_FOCUS:
        ui_pointer_cancel();
        engine_draw(e);          /* one last frame without the pressed state */
        e->animating = 0;
        break;

    case APP_CMD_PAUSE:
    case APP_CMD_STOP:
    case APP_CMD_SAVE_STATE:
        /* Android can kill us at any point after this, so everything dirty
           goes to disk right now. storage.c writes atomically, which is what
           makes "closed while saving" survivable. */
        stone_app_save_all();
        break;

    case APP_CMD_LOW_MEMORY:
        stone_app_save_all();
        break;

    case APP_CMD_DESTROY:
        stone_app_save_all();
        break;

    default:
        break;
    }
}

/* ------------------------------------------------------------------- main */

void android_main(struct android_app *app)
{
    static StoneEngine engine;
    const char *internal_dir;
    const char *external_dir;

    memset(&engine, 0, sizeof(engine));
    engine.app     = app;
    engine.display = EGL_NO_DISPLAY;
    engine.surface = EGL_NO_SURFACE;
    engine.context = EGL_NO_CONTEXT;

    app->userData     = &engine;
    app->onAppCmd     = on_cmd;
    app->onInputEvent = on_input;

    /* internalDataPath is always writable and private to the app; the
       external one is only used for backup files the user may want to copy
       off the device, and may legitimately be NULL. */
    internal_dir = app->activity->internalDataPath;
    external_dir = app->activity->externalDataPath;
    if (internal_dir == NULL) {
        LOGE("no internalDataPath: running with in-memory data only");
    }

    stone_app_init(internal_dir, external_dir);
    engine.app_ready = 1;

    /* Map the texture pack before the first frame: the splash is the very
       first thing drawn and it wants its artwork immediately. A missing or
       damaged pack is not fatal - every draw call falls back to vectors. */
    stone_assets_init((struct AAssetManager *)app->activity->assetManager);
    stone_window_init(app);

    ui_init();

    LOGI("%s %s - %s / %s", STONE_APP_NAME, STONE_APP_VERSION,
         STONE_AUTHOR, STONE_STUDIO);

    for (;;) {
        int ident, events;
        struct android_poll_source *source;

        /* Block when nothing is animating so an idle app costs no CPU.
           ALooper_pollOnce is used instead of the deprecated ALooper_pollAll
           so this file keeps compiling on NDK r26/r27 and later. */
        while ((ident = ALooper_pollOnce(engine.animating ? 0 : -1, NULL,
                                         &events, (void **)&source)) >= 0) {
            if (source != NULL) source->process(app, source);

            if (app->destroyRequested != 0) {
                stone_app_save_all();
                engine_term_display(&engine);
                stone_assets_shutdown();
                stone_app_shutdown();
                return;
            }
        }

        engine_draw(&engine);
    }
}
