package androidx.window.layout;

import android.app.Activity;
import android.content.Context;

/**
 * An interface to calculate the [WindowMetrics] for an [Activity] or a [UiContext].
 */
public interface WindowMetricsCalculator {

    /**
     * Computes the size and position of the area the window would occupy with
     * [MATCH_PARENT][android.view.WindowManager.LayoutParams.MATCH_PARENT] width and height
     * and any combination of flags that would allow the window to extend behind display cutouts.
     * <p>
     * For example, [android.view.WindowManager.LayoutParams.layoutInDisplayCutoutMode] set to
     * [android.view.WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS] or the
     * [android.view.WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS] flag set.
     * <p>
     * The value returned from this method may be different from platform API(s) used to determine
     * the size and position of the visible area a given context occupies. For example:
     * <p>
     * * [Display.getSize] can be used to determine the size of the visible area
     * a window occupies, but may be subtracted to exclude certain system decorations that
     * always appear on screen, notably the navigation bar.
     * * The decor view's [android.view.View#getWidth] and [android.view.View@getHeight] can be
     * used to determine the size of the top level view in the view hierarchy, but this size is
     * determined through a combination of [android.view.WindowManager.LayoutParams]
     * flags and may not represent the true window size. For example, a window that does not
     * indicate it can be displayed behind a display cutout will have the size of the decor
     * view offset to exclude this region unless this region overlaps with the status bar, while
     * the value returned from this method will include this region.
     * <p>
     * The value returned from this method is guaranteed to be correct on platforms
     * [Q][Build.VERSION_CODES.Q] and above. For older platforms the value may be invalid if
     * the activity is in multi-window mode or if the navigation bar offset can not be accounted
     * for, though a best effort is made to ensure the returned value is as close as possible to
     * the true value. See [.computeWindowBoundsP] and
     * [.computeWindowBoundsN].
     * <p>
     * Note: The value of this is based on the last windowing state reported to the client.
     *
     * @see android.view.WindowManager.getCurrentWindowMetrics
     * @see android.view.WindowMetrics.getBounds
     */
    WindowMetrics computeCurrentWindowMetrics(Activity activity);

    /**
     * Computes the size and position of the area the window would occupy with
     * [MATCH_PARENT][android.view.WindowManager.LayoutParams.MATCH_PARENT] width and height
     * and any combination of flags that would allow the window to extend behind display cutouts.
     * <p>
     * On [Build.VERSION_CODES.Q] and older, a [UiContext] is either an [Activity] or an
     * [InputMethodService]. On [Build.VERSION_CODES.R] and newer, a [UiContext] can also be one
     * created via the [Context.createWindowContext] APIs.
     *
     * @throws UnsupportedOperationException if not implemented. The default implementation from [getOrCreate]
     *                             is guaranteed to implement this method.
     * @see [computeCurrentWindowMetrics]
     */
    default WindowMetrics computeCurrentWindowMetrics(Context context) {
        throw new UnsupportedOperationException("Must override computeCurrentWindowMetrics(context) and" +
                " provide an implementation.");
    }

    /**
     * Computes the maximum size and position of the area the window can expect with
     * [MATCH_PARENT][android.view.WindowManager.LayoutParams.MATCH_PARENT] width and height
     * and any combination of flags that would allow the window to extend behind display cutouts.
     * <p>
     * The value returned from this method will always match [Display.getRealSize] on
     * [Android 10][Build.VERSION_CODES.Q] and below.
     *
     * @see android.view.WindowManager.getMaximumWindowMetrics
     */
    WindowMetrics computeMaximumWindowMetrics(Activity activity);

    /**
     * Computes the maximum size and position of the area the window can expect with
     * [MATCH_PARENT][android.view.WindowManager.LayoutParams.MATCH_PARENT] width and height
     * and any combination of flags that would allow the window to extend behind display cutouts.
     * <p>
     * The value returned from this method will always match [Display.getRealSize] on
     * [Android 10][Build.VERSION_CODES.Q] and below.
     * <p>
     * On [Build.VERSION_CODES.Q] and older, a [UiContext] is either an [Activity] or an
     * [InputMethodService]. On [Build.VERSION_CODES.R] and newer, a [UiContext] can also be one
     * created via the [Context.createWindowContext] APIs.
     *
     * @throws UnsupportedOperationException if not implemented. The default implementation from [getOrCreate]
     *                             is guaranteed to implement this method.
     * @see [computeMaximumWindowMetrics]
     */
    default WindowMetrics computeMaximumWindowMetrics(Context context) {
        throw new UnsupportedOperationException("Must override computeMaximumWindowMetrics(context) and" +
                " provide an implementation.");
    }

//    static WindowMetricsCalculator getOrCreate() {
//        return new WindowMetricsCalculatorCompat();
//    }
}
