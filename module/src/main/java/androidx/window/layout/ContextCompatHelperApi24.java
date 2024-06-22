package androidx.window.layout;

import android.app.Activity;
import android.os.Build;

import androidx.annotation.RequiresApi;

@RequiresApi(Build.VERSION_CODES.N)
final class ContextCompatHelperApi24 {
    static boolean isInMultiWindowMode(Activity activity) {
        return activity.isInMultiWindowMode();
    }
}
