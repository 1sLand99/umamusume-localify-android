package androidx.window.layout;

import android.app.Activity;
import android.content.Context;
import android.graphics.Rect;
import android.os.Build;
import android.view.WindowInsets;
import android.view.WindowManager;

import androidx.annotation.DoNotInline;
import androidx.annotation.RequiresApi;
import androidx.annotation.UiContext;
import androidx.core.view.WindowInsetsCompat;

@RequiresApi(Build.VERSION_CODES.R)
final class ContextCompatHelperApi30 {

    static WindowMetrics currentWindowMetrics(@UiContext Context context) {
        WindowManager wm = context.getSystemService(WindowManager.class);
        WindowInsetsCompat insets = WindowInsetsCompat.toWindowInsetsCompat(wm.getCurrentWindowMetrics().getWindowInsets());
        return new WindowMetrics(wm.getCurrentWindowMetrics().getBounds(), insets);
    }

    static Rect currentWindowBounds(@UiContext Context context) {
        WindowManager wm = context.getSystemService(WindowManager.class);
        return wm.getCurrentWindowMetrics().getBounds();
    }

    static WindowInsetsCompat currentWindowInsets(@UiContext Context context) {
        WindowManager wm = context.getSystemService(WindowManager.class);
        return WindowInsetsCompat.toWindowInsetsCompat(wm.getCurrentWindowMetrics().getWindowInsets());
    }

    static Rect maximumWindowBounds(@UiContext Context context) {
        WindowManager wm = context.getSystemService(WindowManager.class);
        return wm.getMaximumWindowMetrics().getBounds();
    }

    /**
     * Computes the [WindowInsetsCompat] for platforms above [Build.VERSION_CODES.R], inclusive.
     *
     * @DoNotInline required for implementation-specific class method to prevent it from being
     * inlined.
     * @see WindowMetrics.getWindowInsets
     */
    @DoNotInline
    static WindowInsetsCompat currentWindowInsets(Activity activity) {
        WindowInsets platformInsets = activity.getWindowManager().getCurrentWindowMetrics().getWindowInsets();
        return WindowInsetsCompat.toWindowInsetsCompat(platformInsets);
    }
}
