package androidx.window.layout;

import android.graphics.Rect;

import androidx.core.view.WindowInsetsCompat;
import androidx.window.core.Bounds;
import androidx.window.core.ExperimentalWindowApi;

/**
 * Metrics about a [android.view.Window], consisting of its bounds.
 *
 *
 * This is obtained from [WindowMetricsCalculator.computeCurrentWindowMetrics] or
 * [WindowMetricsCalculator.computeMaximumWindowMetrics].
 *
 * @see WindowMetricsCalculator
 */
public class WindowMetrics {
    private final Bounds _bounds;
    private final WindowInsetsCompat _windowInsetsCompat;

    public WindowMetrics(Bounds _bounds, WindowInsetsCompat _windowInsetsCompat) {
        this._bounds = _bounds;
        this._windowInsetsCompat = _windowInsetsCompat;
    }

    /**
     * An internal constructor for [WindowMetrics]
     */
    public WindowMetrics(Rect bounds, WindowInsetsCompat insets) {
        this(new Bounds(bounds), insets == null ? new WindowInsetsCompat.Builder().build() : insets);
    }

    /**
     * Returns a new [Rect] describing the bounds of the area the window occupies.
     * <p>
     * <p>
     * **Note that the size of the reported bounds can have different size than
     * [Display#getSize].** This method reports the window size including all system
     * decorations, while [Display#getSize] reports the area excluding navigation bars
     * and display cutout areas.
     *
     * @return window bounds in pixels.
     */
    public Rect getBounds() {
        return _bounds.toRect();
    }

    @Override
    public String toString() {
        return "WindowMetrics( bounds=" + _bounds + ", windowInsetsCompat=" + _windowInsetsCompat + ")";
    }

    @Override
    public boolean equals(Object obj) {
        if (this == obj) return true;
        if (!(obj instanceof WindowMetrics other)) return false;

        if (!_bounds.equals(other._bounds)) return false;
        if (!_windowInsetsCompat.equals(other._windowInsetsCompat)) return false;

        return true;
    }

    @Override
    public int hashCode() {
        int result = _bounds.hashCode();
        result = 31 * result + _windowInsetsCompat.hashCode();
        return result;
    }

    /**
     * Returns the [WindowInsetsCompat] of the area associated with this window or visual context.
     */
    @ExperimentalWindowApi
    // TODO (b/238354685): Match interface style of Bounds after the API is fully backported
    public WindowInsetsCompat getWindowInsets() {
        return _windowInsetsCompat;
    }
}
