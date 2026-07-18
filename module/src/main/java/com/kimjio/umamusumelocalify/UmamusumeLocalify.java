package com.kimjio.umamusumelocalify;

import android.app.Activity;
import android.os.Build;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;

import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;

@SuppressWarnings({"unused", "JavaJniMissingFunction"})
public final class UmamusumeLocalify {
    private static final String TAG = "UmamusumeLocalify[Java]";

    public static native void onLayoutChange_native(Activity activity, View v, int left, int top, int right, int bottom, int oldLeft, int oldTop, int oldRight, int oldBottom);

    public static void load() {
        Log.i(TAG, TAG + " loaded");
    }

    public static void registerCallback(Activity activity) {
        activity.getWindow().getDecorView().addOnLayoutChangeListener((View v, int left, int top, int right, int bottom, int oldLeft, int oldTop, int oldRight, int oldBottom) -> onLayoutChange_native(activity, v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom));
    }

    public static Insets getCaptionBarInsets(Activity activity) {
        var insets = ViewCompat.getRootWindowInsets(activity.getWindow().getDecorView());
        if (insets == null) {
            return null;
        }
        return insets.getInsets(WindowInsetsCompat.Type.captionBar());
    }

    public static boolean isEdgeToEdgeEnabled(Activity activity) {
        if (activity.getApplicationInfo().targetSdkVersion < Build.VERSION_CODES.VANILLA_ICE_CREAM) {
            return false;
        }

        WindowManager.LayoutParams attributes = activity.getWindow().getAttributes();
        return attributes.layoutInDisplayCutoutMode != WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER;
    }
}
