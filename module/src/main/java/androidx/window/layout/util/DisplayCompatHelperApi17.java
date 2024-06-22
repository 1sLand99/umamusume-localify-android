package androidx.window.layout.util;

import android.graphics.Point;
import android.os.Build;
import android.view.Display;

import androidx.annotation.RequiresApi;

@RequiresApi(Build.VERSION_CODES.JELLY_BEAN_MR1)
public final class DisplayCompatHelperApi17 {
    
    public static void getRealSize(Display display, Point point) {
        display.getRealSize(point);
    }
}
