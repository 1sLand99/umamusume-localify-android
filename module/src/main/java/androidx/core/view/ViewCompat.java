/*
 * Copyright 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package androidx.core.view;

import android.annotation.SuppressLint;
import android.graphics.Rect;
import android.os.Build;
import android.view.View;
import android.view.WindowInsets;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.util.List;

/**
 * Helper for accessing features in {@link View}.
 */
@SuppressWarnings({"JavadocReference", "DeprecatedIsStillUsed", "JavaDoc", "RedundantSuppression"})
// Unreliable warnings.
@SuppressLint("PrivateConstructorForUtilityClass") // deprecated non-private constructor
public class ViewCompat {
    /**
     * Sets a list of areas within this view's post-layout coordinate space where the system
     * should not intercept touch or other pointing device gestures. <em>This method should
     * be called by {@link View#onLayout(boolean, int, int, int, int)} or
     * {@link View#onDraw(Canvas)}.</em>
     * <p>
     * On devices running API 28 and below, this method has no effect.
     *
     * @param rects A list of precision gesture regions that this view needs to function correctly
     * @see View#setSystemGestureExclusionRects
     */
    public static void setSystemGestureExclusionRects(@NonNull View view,
            @NonNull List<Rect> rects) {
        if (Build.VERSION.SDK_INT >= 29) {
            view.setSystemGestureExclusionRects(rects);
        }
    }

    /**
     * Provide original {@link WindowInsetsCompat} that are dispatched to the view hierarchy.
     * The insets are only available if the view is attached.
     * <p>
     * On devices running API 20 and below, this method always returns null.
     *
     * @return WindowInsetsCompat from the top of the view hierarchy or null if View is detached
     */
    @Nullable
    public static WindowInsetsCompat getRootWindowInsets(@NonNull View view) {
        final WindowInsets wi = view.getRootWindowInsets();
        if (wi == null) return null;

        final WindowInsetsCompat insets = WindowInsetsCompat.toWindowInsetsCompat(wi);
        // This looks strange, but the WindowInsetsCompat instance still needs to know about
        // what the root window insets, and the root view visible bounds are
        insets.setRootWindowInsets(insets);
        insets.copyRootViewBounds(view.getRootView());
        return insets;
    }

    /**
     * Returns true if the provided view is currently attached to a window.
     */
    public static boolean isAttachedToWindow(@NonNull View view) {
        return view.isAttachedToWindow();
    }
}
