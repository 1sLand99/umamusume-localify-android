package androidx.window.layout.util;

import android.os.Build;
import android.view.DisplayCutout;

import androidx.annotation.RequiresApi;

@RequiresApi(Build.VERSION_CODES.P)
public final class DisplayCompatHelperApi28 {
    public static int safeInsetLeft(DisplayCutout displayCutout) {
        return displayCutout.getSafeInsetLeft();
    }

    public static int safeInsetTop(DisplayCutout displayCutout) {
        return displayCutout.getSafeInsetTop();
    }

    public static int safeInsetRight(DisplayCutout displayCutout) {
        return displayCutout.getSafeInsetRight();
    }

    public static int safeInsetBottom(DisplayCutout displayCutout) {
        return displayCutout.getSafeInsetBottom();
    }
}
