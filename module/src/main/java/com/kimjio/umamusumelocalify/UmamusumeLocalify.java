package com.kimjio.umamusumelocalify;

import android.app.Activity;
import android.content.ComponentCallbacks;
import android.content.res.Configuration;
import android.util.Log;
import android.view.WindowManager;

import androidx.annotation.NonNull;

@SuppressWarnings({"unused", "JavaJniMissingFunction"})
public final class UmamusumeLocalify {
    private static final String TAG = "UmamusumeLocalify[Java]";

    public static native void onConfigurationChanged_native(Activity activity, Configuration newConfig);

    public static void load() {
        Log.i(TAG, TAG + " loaded");
    }

    public static void registerCallback(Activity activity) {
        activity.registerComponentCallbacks(new ComponentCallbacks() {
            @Override
            public void onConfigurationChanged(@NonNull Configuration newConfig) {
                onConfigurationChanged_native(activity, newConfig);
            }

            @SuppressWarnings("deprecated")
            @Override
            public void onLowMemory() {
            }
        });
    }

    public static boolean isEdgeToEdgeEnabled(Activity activity) {
        WindowManager.LayoutParams attributes = activity.getWindow().getAttributes();
        return attributes.layoutInDisplayCutoutMode != WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER;
    }
}
