package androidx.window.layout.util;

import android.app.Activity;
import android.content.Context;
import android.content.ContextWrapper;
import android.inputmethodservice.InputMethodService;

import androidx.annotation.UiContext;

public final class ContextUtils {
    /**
     * Given a [UiContext], check if it is a [ContextWrapper]. If so, we need to unwrap it and
     * return the actual [UiContext] within.
     */
    @UiContext
    public static Context unwrapUiContext(@UiContext Context context) {
        Context iterator = context;

        while (iterator instanceof ContextWrapper) {
            if (iterator instanceof Activity) {
                // Activities are always ContextWrappers
                return iterator;
            } else if (iterator instanceof InputMethodService) {
                // InputMethodService are always ContextWrappers
                return iterator;
            } else if (((ContextWrapper) iterator).getBaseContext() == null) {
                return iterator;
            }

            iterator = ((ContextWrapper) iterator).getBaseContext();
        }

        // TODO(b/259148796): This code path is not needed for APIs R and above. However, that is
        //  not clear and also not enforced anywhere. Once we move to version-based implementations,
        //  this ambiguity will no longer exist. Again for clarity, on APIs before R, UiContexts are
        //  Activities or InputMethodServices, so we should never reach this point.
        throw new IllegalArgumentException("Context " + context + " is not a UiContext");
    }
}
