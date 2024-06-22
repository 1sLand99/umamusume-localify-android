package androidx.window.layout;

import static androidx.window.layout.ContextCompatHelperApi24.isInMultiWindowMode;
import static androidx.window.layout.ContextCompatHelperApi30.currentWindowBounds;
import static androidx.window.layout.ContextCompatHelperApi30.currentWindowInsets;
import static androidx.window.layout.ContextCompatHelperApi30.currentWindowMetrics;
import static androidx.window.layout.ContextCompatHelperApi30.maximumWindowBounds;
import static androidx.window.layout.util.ContextUtils.unwrapUiContext;
import static androidx.window.layout.util.DisplayCompatHelperApi17.getRealSize;
import static androidx.window.layout.util.DisplayCompatHelperApi28.safeInsetBottom;
import static androidx.window.layout.util.DisplayCompatHelperApi28.safeInsetLeft;
import static androidx.window.layout.util.DisplayCompatHelperApi28.safeInsetRight;
import static androidx.window.layout.util.DisplayCompatHelperApi28.safeInsetTop;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Point;
import android.graphics.Rect;
import android.inputmethodservice.InputMethodService;
import android.os.Build;
import android.util.Log;
import android.view.Display;
import android.view.DisplayCutout;
import android.view.WindowManager;

import androidx.annotation.RequiresApi;
import androidx.annotation.UiContext;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.WindowInsetsCompat;
import androidx.window.core.Bounds;

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.List;

public class WindowMetricsCalculatorCompat implements WindowMetricsCalculator {
    private static final String TAG = WindowMetricsCalculatorCompat.class.getSimpleName();

    /**
     * Computes the current [WindowMetrics] for a given [Context]. The context can be either
     * an [Activity], a Context created with [Context#createWindowContext], or an
     * [InputMethodService].
     *
     * @see WindowMetricsCalculator.computeCurrentWindowMetrics
     */
    @Override
    public WindowMetrics computeCurrentWindowMetrics(@UiContext Context context) {
        // TODO(b/259148796): Make WindowMetricsCalculatorCompat more testable
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            return currentWindowMetrics(context);
        } else {
            Context context1 = unwrapUiContext(context);

            if (context1 instanceof Activity) {
                return computeCurrentWindowMetrics((Activity) context1);
            }

            if (context1 instanceof InputMethodService) {
                WindowManager wm = context1.getSystemService(WindowManager.class);

                // On older SDK levels, the app and IME could show up on different displays.
                // However, there isn't a way for us to figure this out from the application
                // layer. But, this should be good enough for now given the small likelihood of
                // IMEs showing up on non-primary displays on these SDK levels.
                @SuppressWarnings("deprecation")
                Point displaySize = getRealSizeForDisplay(wm.getDefaultDisplay());

                // IME occupies the whole display bounds.
                Rect imeBounds = new Rect(0, 0, displaySize.x, displaySize.y);
                return new WindowMetrics(imeBounds, null);
            }

            throw new IllegalArgumentException(context + " is not a UiContext");
        }
    }

    /**
     * Computes the current [WindowMetrics] for a given [Activity]
     *
     * @see WindowMetricsCalculator.computeCurrentWindowMetrics
     */
    @Override
    public WindowMetrics computeCurrentWindowMetrics(Activity activity) {
        Rect bounds;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            bounds = currentWindowBounds(activity);
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            bounds = computeWindowBoundsQ(activity);
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            bounds = computeWindowBoundsP(activity);
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            bounds = computeWindowBoundsN(activity);
        } else {
            bounds = computeWindowBoundsIceCreamSandwich(activity);
        }
        // TODO (b/233899790): compute insets for other platform versions below R
        WindowInsetsCompat windowInsetsCompat;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            windowInsetsCompat = computeWindowInsetsCompat(activity);
        } else {
            windowInsetsCompat = new WindowInsetsCompat.Builder().build();
        }
        return new WindowMetrics(new Bounds(bounds), windowInsetsCompat);
    }

    /**
     * Computes the maximum [WindowMetrics] for a given [Activity]
     *
     * @see WindowMetricsCalculator.computeMaximumWindowMetrics
     */
    @Override
    public WindowMetrics computeMaximumWindowMetrics(Activity activity) {
        return computeMaximumWindowMetrics((Context) activity);
    }

    /**
     * Computes the maximum [WindowMetrics] for a given [UiContext]
     *
     * @See WindowMetricsCalculator.computeMaximumWindowMetrics
     */
    @Override
    public WindowMetrics computeMaximumWindowMetrics(Context context) {
        // TODO(b/259148796): Make WindowMetricsCalculatorCompat more testable
        Rect bounds = null;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            bounds = maximumWindowBounds(context);
        } else {
            WindowManager wm = context.getSystemService(WindowManager.class);
            // [WindowManager#getDefaultDisplay] is deprecated but we have this for
            // compatibility with older versions, as we can't reliably get the display associated
            // with a Context through public APIs either.
            @SuppressWarnings("deprecation")
            Display display = wm.getDefaultDisplay();
            Point displaySize = getRealSizeForDisplay(display);
            bounds = new Rect(0, 0, displaySize.x, displaySize.y);
        }
        // TODO (b/233899790): compute insets for other platform versions below R
        WindowInsetsCompat windowInsetsCompat = null;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            windowInsetsCompat = computeWindowInsetsCompat(context);
        } else {
            windowInsetsCompat = new WindowInsetsCompat.Builder().build();
        }
        return new WindowMetrics(new Bounds(bounds), windowInsetsCompat);
    }


    /**
     * Computes the window bounds for [Build.VERSION_CODES.Q].
     */
    @SuppressLint({"BanUncheckedReflection", "BlockedPrivateApi"})
    @RequiresApi(Build.VERSION_CODES.Q)
    private Rect computeWindowBoundsQ(Activity activity) {
        Rect bounds = null;
        Configuration config = activity.getResources().getConfiguration();
        try {
            Field windowConfigField =
                    Configuration.class.getDeclaredField("windowConfiguration");
            windowConfigField.setAccessible(true);
            Object windowConfig = windowConfigField.get(config);
            Method getBoundsMethod = windowConfig.getClass().getDeclaredMethod("getBounds");
            bounds = new Rect((Rect) getBoundsMethod.invoke(windowConfig));
        } catch (NoSuchFieldException | NoSuchMethodException | IllegalAccessException |
                 InvocationTargetException e) {
            Log.w(TAG, e);
            // If reflection fails for some reason default to the P implementation which still
            // has the ability to account for display cutouts.
            bounds = computeWindowBoundsP(activity);
        }
        return bounds;
    }

    /**
     * Computes the window bounds for [Build.VERSION_CODES.P].
     * <p>
     * <p>
     * NOTE: This method may result in incorrect values if the [android.content.res.Resources]
     * value stored at 'navigation_bar_height' does not match the true navigation bar inset on
     * the window.
     */
    @SuppressLint({"BanUncheckedReflection", "BlockedPrivateApi"})
    @RequiresApi(Build.VERSION_CODES.P)
    private Rect computeWindowBoundsP(Activity activity) {
        Rect bounds = new Rect();
        Configuration config = activity.getResources().getConfiguration();
        try {
            Field windowConfigField =
                    Configuration.class.getDeclaredField("windowConfiguration");
            windowConfigField.setAccessible(true);
            Object windowConfig = windowConfigField.get(config);

            // In multi-window mode we'll use the WindowConfiguration#mBounds property which
            // should match the window size. Otherwise we'll use the mAppBounds property and
            // will adjust it below.
            if (isInMultiWindowMode(activity)) {
                Method getAppBounds = windowConfig.getClass().getDeclaredMethod("getBounds");
                bounds.set((Rect) getAppBounds.invoke(windowConfig));
            } else {
                Method getAppBounds = windowConfig.getClass().getDeclaredMethod("getAppBounds");
                bounds.set((Rect) getAppBounds.invoke(windowConfig));
            }
        } catch (NoSuchFieldException | NoSuchMethodException | IllegalAccessException |
                 InvocationTargetException e) {
            Log.w(TAG, e);
            getRectSizeFromDisplay(activity, bounds);
        }
        WindowManager platformWindowManager = activity.getWindowManager();

        // [WindowManager#getDefaultDisplay] is deprecated but we have this for
        // compatibility with older versions
        @SuppressWarnings("deprecation")
        Display currentDisplay = platformWindowManager.getDefaultDisplay();
        Point realDisplaySize = new Point();
        // [Display#getRealSize] is deprecated but we have this for
        // compatibility with older versions
        getRealSize(currentDisplay, realDisplaySize);
        if (!isInMultiWindowMode(activity)) {
            // The activity is not in multi-window mode. Check if the addition of the
            // navigation bar size to mAppBounds results in the real display size and if so
            // assume the nav bar height should be added to the result.
            int navigationBarHeight = getNavigationBarHeight(activity);
            if (bounds.bottom + navigationBarHeight == realDisplaySize.y) {
                bounds.bottom += navigationBarHeight;
            } else if (bounds.right + navigationBarHeight == realDisplaySize.x) {
                bounds.right += navigationBarHeight;
            } else if (bounds.left == navigationBarHeight) {
                bounds.left = 0;
            }
        }
        if ((bounds.width() < realDisplaySize.x || bounds.height() < realDisplaySize.y) &&
                !isInMultiWindowMode(activity)
        ) {
            // If the corrected bounds are not the same as the display size and the activity is
            // not in multi-window mode it is possible there are unreported cutouts inset-ing
            // the window depending on the layoutInCutoutMode. Check for them here by getting
            // the cutout from the display itself.
            DisplayCutout displayCutout = getCutoutForDisplay(currentDisplay);
            if (displayCutout != null) {
                if (bounds.left == safeInsetLeft(displayCutout)) {
                    bounds.left = 0;
                }
                if (realDisplaySize.x - bounds.right == safeInsetRight(displayCutout)) {
                    bounds.right += safeInsetRight(displayCutout);
                }
                if (bounds.top == safeInsetTop(displayCutout)) {
                    bounds.top = 0;
                }
                if (realDisplaySize.y - bounds.bottom == safeInsetBottom(displayCutout)) {
                    bounds.bottom += safeInsetBottom(displayCutout);
                }
            }
        }
        return bounds;
    }

    @SuppressWarnings("deprecation")
    private void getRectSizeFromDisplay(Activity activity, Rect bounds) {
        // [WindowManager#getDefaultDisplay] is deprecated but we have this for
        // compatibility with older versions
        Display defaultDisplay = activity.getWindowManager().getDefaultDisplay();
        // [Display#getRectSize] is deprecated but we have this for
        // compatibility with older versions
        defaultDisplay.getRectSize(bounds);
    }

    /**
     * Computes the window bounds for platforms between [Build.VERSION_CODES.N]
     * and [Build.VERSION_CODES.O_MR1], inclusive.
     * <p>
     * <p>
     * NOTE: This method may result in incorrect values under the following conditions:
     * <p>
     * * If the activity is in multi-window mode the origin of the returned bounds will
     * always be anchored at (0, 0).
     * * If the [android.content.res.Resources] value stored at 'navigation_bar_height' does
     * not match the true navigation bar size the returned bounds will not take into account
     * the navigation
     * bar.
     */
    @RequiresApi(Build.VERSION_CODES.N)
    private Rect computeWindowBoundsN(Activity activity) {
        Rect bounds = new Rect();
        // [WindowManager#getDefaultDisplay] is deprecated but we have this for
        // compatibility with older versions
        @SuppressWarnings("deprecation")
        Display defaultDisplay = activity.getWindowManager().getDefaultDisplay();
        // [Display#getRectSize] is deprecated but we have this for
        // compatibility with older versions
        defaultDisplay.getRectSize(bounds);
        if (!isInMultiWindowMode(activity)) {
            // The activity is not in multi-window mode. Check if the addition of the
            // navigation bar size to Display#getSize() results in the real display size and
            // if so return this value. If not, return the result of Display#getSize().
            Point realDisplaySize = getRealSizeForDisplay(defaultDisplay);
            int navigationBarHeight = getNavigationBarHeight(activity);
            if (bounds.bottom + navigationBarHeight == realDisplaySize.y) {
                bounds.bottom += navigationBarHeight;
            } else if (bounds.right + navigationBarHeight == realDisplaySize.x) {
                bounds.right += navigationBarHeight;
            }
        }
        return bounds;
    }

    /**
     * Computes the window bounds for platforms between [Build.VERSION_CODES.JELLY_BEAN]
     * and [Build.VERSION_CODES.M], inclusive.
     * <p>
     * <p>
     * Given that multi-window mode isn't supported before N we simply return the real display
     * size which should match the window size of a full-screen app.
     */
    private Rect computeWindowBoundsIceCreamSandwich(Activity activity) {
        // [WindowManager#getDefaultDisplay] is deprecated but we have this for
        // compatibility with older versions

        @SuppressWarnings("deprecation")
        Display defaultDisplay = activity.getWindowManager().getDefaultDisplay();
        Point realDisplaySize = getRealSizeForDisplay(defaultDisplay);
        Rect bounds = new Rect();
        if (realDisplaySize.x == 0 || realDisplaySize.y == 0) {
            // [Display#getRectSize] is deprecated but we have this for
            // compatibility with older versions
            defaultDisplay.getRectSize(bounds);
        } else {
            bounds.right = realDisplaySize.x;
            bounds.bottom = realDisplaySize.y;
        }
        return bounds;
    }

    /**
     * Returns the full (real) size of the display, in pixels, without subtracting any window
     * decor or applying any compatibility scale factors.
     * <p>
     * <p>
     * The size is adjusted based on the current rotation of the display.
     *
     * @return a point representing the real display size in pixels.
     * @see Display.getRealSize
     */
    @VisibleForTesting
    private Point getRealSizeForDisplay(Display display) {
        Point size = new Point();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            getRealSize(display, size);
        } else {
            try {
                Method getRealSizeMethod = Display.class.getDeclaredMethod(
                        "getRealSize",
                        Point.class
                );
                getRealSizeMethod.setAccessible(true);
                getRealSizeMethod.invoke(display, size);
            } catch (NoSuchMethodException | IllegalAccessException | InvocationTargetException e) {
                Log.w(TAG, e);
            }
        }
        return size;
    }

    /**
     * Returns the [android.content.res.Resources] value stored as 'navigation_bar_height'.
     * <p>
     * <p>
     * Note: This is error-prone and is **not** the recommended way to determine the size
     * of the overlapping region between the navigation bar and a given window. The best
     * approach is to acquire the [android.view.WindowInsets].
     */
    private int getNavigationBarHeight(Context context) {
        Resources resources = context.getResources();
        int resourceId = resources.getIdentifier("navigation_bar_height", "dimen", "android");
        if (resourceId > 0) {
            return resources.getDimensionPixelSize(resourceId);
        }
        return 0;
    }

    /**
     * Returns the [DisplayCutout] for the given display. Note that display cutout returned
     * here is for the display and the insets provided are in the display coordinate system.
     *
     * @return the display cutout for the given display.
     */
    @SuppressLint("BanUncheckedReflection")
    @RequiresApi(Build.VERSION_CODES.P)
    private DisplayCutout getCutoutForDisplay(Display display) {
        DisplayCutout displayCutout = null;
        try {
            Class<?> displayInfoClass = Class.forName("android.view.DisplayInfo");
            Constructor<?> displayInfoConstructor = displayInfoClass.getConstructor();
            displayInfoConstructor.setAccessible(true);
            Object displayInfo = displayInfoConstructor.newInstance();
            Method getDisplayInfoMethod = display.getClass().getDeclaredMethod(
                    "getDisplayInfo", displayInfo.getClass()
            );
            getDisplayInfoMethod.setAccessible(true);
            getDisplayInfoMethod.invoke(display, displayInfo);
            Field displayCutoutField = displayInfo.getClass().getDeclaredField("displayCutout");
            displayCutoutField.setAccessible(true);
            Object cutout = displayCutoutField.get(displayInfo);
            if (cutout instanceof DisplayCutout){
                displayCutout = (DisplayCutout) cutout;
            }
        } catch (ClassNotFoundException | NoSuchMethodException | NoSuchFieldException |
                 IllegalAccessException | InvocationTargetException | InstantiationException e) {
            Log.w(TAG, e);
        }
        return displayCutout;
    }

    /**
     * [ArrayList] that defines different types of sources causing window insets.
     */
    private List<Integer> insetsTypeMasks = List.of(
            WindowInsetsCompat.Type.statusBars(),
            WindowInsetsCompat.Type.navigationBars(),
            WindowInsetsCompat.Type.captionBar(),
            WindowInsetsCompat.Type.ime(),
            WindowInsetsCompat.Type.systemGestures(),
            WindowInsetsCompat.Type.mandatorySystemGestures(),
            WindowInsetsCompat.Type.tappableElement(),
            WindowInsetsCompat.Type.displayCutout()
    );

    /**
     * Computes the current [WindowInsetsCompat] for a given [Context].
     */
    @RequiresApi(Build.VERSION_CODES.R)
    private WindowInsetsCompat computeWindowInsetsCompat(@UiContext Context context) {
        int build = Build.VERSION.SDK_INT;
        WindowInsetsCompat windowInsetsCompat = null;
        if (build >= Build.VERSION_CODES.R) {
            windowInsetsCompat = currentWindowInsets(context);
        } else {
            throw new RuntimeException("Incompatible SDK version");
        }
        return windowInsetsCompat;
    }
}
