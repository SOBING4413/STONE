/* StoneActivity.java - the one and only Java class in STONE.
 *
 * Why it exists at all
 * -------------------
 * STONE used to run on the framework's stock android.app.NativeActivity with
 * android:hasCode="false", and asked for immersive mode from C over JNI. That
 * call has to happen on the Activity's UI thread; android_main() runs on the
 * glue library's own pthread. On a cold start the decor view is still being
 * laid out on the UI thread while the native thread pokes
 * setSystemUiVisibility() from the side, so the request lost the race and the
 * system's 3-button / gesture bar stayed pinned on top of STONE's own bottom
 * tab bar. Coming back from Recents worked, because by then the view was
 * already attached and the retry on APP_CMD_GAINED_FOCUS landed - which is
 * exactly the "only broken on first launch" symptom.
 *
 * This class fixes it at the source: every window call below runs on the UI
 * thread, at the documented lifecycle points, and it re-arms itself whenever
 * the system decides to show the bars again.
 *
 * It also forwards the real window insets to the renderer, so that even on a
 * device or accessibility setting where the bars cannot be hidden, STONE lays
 * its header and tab bar out inside the safe area instead of underneath the
 * system UI. Hiding is the fast path; insets are the guarantee.
 *
 * Created by sobing4413 - Exter Interactive.
 */
package com.stone.app;

import android.app.NativeActivity;
import android.graphics.Insets;
import android.os.Build;
import android.os.Bundle;
import android.util.DisplayMetrics;
import android.view.DisplayCutout;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;

public class StoneActivity extends NativeActivity {

    static {
        /* NativeActivity dlopen()s libstone.so by itself (android.app.lib_name
           in the manifest), but a raw dlopen handle is invisible to the JVM's
           JNI symbol resolver. Loading the same library through the class
           loader as well - it is reference counted, not a second copy in
           memory - is what makes nativeSetInsets() resolvable from here. */
        System.loadLibrary("stone");
    }

    /** Pushes the current safe-area insets, in pixels, into the C renderer. */
    private static native void nativeSetInsets(int left, int top, int right,
                                               int bottom, float density);

    /* The legacy View.SYSTEM_UI_FLAG_* immersive set. Deprecated since API 30
       but still honoured through API 34 (this app's targetSdk) and the only
       mechanism that exists on API 21..29 (this app's minSdk is 21), so it is
       applied on every level and the WindowInsetsController path below is
       layered on top where it is available. */
    private static final int IMMERSIVE_FLAGS =
            View.SYSTEM_UI_FLAG_LAYOUT_STABLE
          | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
          | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
          | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
          | View.SYSTEM_UI_FLAG_FULLSCREEN
          | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;

    private final Runnable mReapply = new Runnable() {
        @Override public void run() { applyImmersive(); }
    };

    private int mLeft = -1, mTop = -1, mRight = -1, mBottom = -1;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        final Window win = getWindow();

        /* Edge to edge: the GL surface owns the whole display and the system
           bars, when they are shown at all, float above it instead of
           shrinking it. Without this the surface is resized every time the
           bars toggle and the layout jumps. */
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            win.setDecorFitsSystemWindows(false);
        }
        win.addFlags(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);
        win.clearFlags(WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS
                     | WindowManager.LayoutParams.FLAG_TRANSLUCENT_NAVIGATION);
        win.setStatusBarColor(0x00000000);
        win.setNavigationBarColor(0x00000000);

        /* Let the app paint into the notch/punch-hole area too; the insets we
           forward below keep the actual content clear of it. */
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            WindowManager.LayoutParams lp = win.getAttributes();
            lp.layoutInDisplayCutoutMode =
                    WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            win.setAttributes(lp);
        }

        final View decor = win.getDecorView();

        decor.setOnApplyWindowInsetsListener(new View.OnApplyWindowInsetsListener() {
            @Override
            public WindowInsets onApplyWindowInsets(View v, WindowInsets insets) {
                publishInsets(insets);
                return insets;
            }
        });

        /* Android un-hides the bars on its own after a swipe, a dialog, a
           screenshot, an incoming notification shade pull... Re-arm shortly
           after, which is what "sticky" is supposed to do anyway but does not
           always survive an OEM skin. */
        decor.setOnSystemUiVisibilityChangeListener(
                new View.OnSystemUiVisibilityChangeListener() {
            @Override
            public void onSystemUiVisibilityChange(int visibility) {
                if ((visibility & View.SYSTEM_UI_FLAG_HIDE_NAVIGATION) == 0) {
                    decor.removeCallbacks(mReapply);
                    decor.postDelayed(mReapply, 1500);
                }
            }
        });

        applyImmersive();
        publishInsets(null);          /* seed the renderer before the first frame */
    }

    @Override
    protected void onResume() {
        super.onResume();
        applyImmersive();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (!hasFocus) return;
        /* The very first focus gain on a cold start is the moment the decor
           view is finally attached, which is precisely where the old native
           JNI attempt used to be too early. Post once more so the request is
           the last thing that touches the window this frame. */
        applyImmersive();
        final View decor = getWindow().getDecorView();
        decor.removeCallbacks(mReapply);
        decor.postDelayed(mReapply, 300);
    }

    private void applyImmersive() {
        final Window win = getWindow();
        final View decor = win.getDecorView();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            WindowInsetsController c = win.getInsetsController();
            if (c != null) {
                c.setSystemBarsBehavior(
                        WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
                c.hide(WindowInsets.Type.systemBars());
            }
        }
        decor.setSystemUiVisibility(IMMERSIVE_FLAGS);
        publishInsets(decor.getRootWindowInsets());
    }

    /** Reads the safe area out of a WindowInsets and hands it to the renderer. */
    private void publishInsets(WindowInsets insets) {
        int l = 0, t = 0, r = 0, b = 0;

        if (insets == null) {
            View decor = getWindow().getDecorView();
            insets = decor.getRootWindowInsets();
        }

        if (insets != null) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                Insets bars = insets.getInsets(WindowInsets.Type.systemBars()
                                             | WindowInsets.Type.displayCutout());
                l = bars.left; t = bars.top; r = bars.right; b = bars.bottom;
            } else {
                l = insets.getSystemWindowInsetLeft();
                t = insets.getSystemWindowInsetTop();
                r = insets.getSystemWindowInsetRight();
                b = insets.getSystemWindowInsetBottom();
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                    DisplayCutout cut = insets.getDisplayCutout();
                    if (cut != null) {
                        l = Math.max(l, cut.getSafeInsetLeft());
                        t = Math.max(t, cut.getSafeInsetTop());
                        r = Math.max(r, cut.getSafeInsetRight());
                        b = Math.max(b, cut.getSafeInsetBottom());
                    }
                }
            }
        }

        /* A bar that is currently hidden reports zero, which is what we want:
           STONE then gets the whole screen. If it is showing - gesture handle,
           an OEM that refuses to hide it, an accessibility setting - the
           renderer pads by exactly this much and nothing ever overlaps. */
        if (l == mLeft && t == mTop && r == mRight && b == mBottom) return;
        mLeft = l; mTop = t; mRight = r; mBottom = b;

        float density = 1.0f;
        DisplayMetrics dm = getResources().getDisplayMetrics();
        if (dm != null && dm.density > 0.0f) density = dm.density;

        try {
            nativeSetInsets(l, t, r, b, density);
        } catch (UnsatisfiedLinkError ignored) {
            /* Native side not up yet; the next callback will deliver it. */
        }
    }
}
