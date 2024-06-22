package androidx.window.core;

import android.graphics.Rect;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/**
 * A wrapper for [Rect] to handle compatibility issues with API 15. In API 15, equals and
 * hashCode operate on the reference as opposed to the attributes. This leads to test failures
 * because the data matches but the equals check fails.
 * <p>
 * Also useful in unit tests since you can instantiate [Bounds] in a JVM test but when you
 * instantiate [Rect] you are using the class from the mockable jar file. The mockable jar does
 * not contain any behavior or calculations.
 */
public class Bounds {
    private final int left;
    private final int top;
    private final int right;
    private final int bottom;

    public Bounds(int left, int top, int right, int bottom) {
        this.left = left;
        this.top = top;
        this.right = right;
        this.bottom = bottom;

        if (!(left <= right)) {
            throw new IllegalArgumentException("Left must be less than or equal to right, left: " + left + ", right: " + right);
        }

        if (!(top <= bottom)) {
            throw new IllegalArgumentException("top must be less than or equal to bottom, top: " + top + ", bottom: " + bottom);
        }
    }

    public Bounds(Rect rect) {
        this(rect.left, rect.top, rect.right, rect.bottom);
    }

    /**
     * Return the [Rect] representation of the bounds
     */
    public Rect toRect() {
        return new Rect(left, top, right, bottom);
    }

    /**
     * The width of the bounds, may be negative.
     */
    public int getWidth() {
        return right - left;
    }

    /**
     * The height of the bounds, may be negative.
     */
    public int getHeight() {
        return bottom - top;
    }

    /**
     * Determines if the bounds has empty area.
     */
    public boolean isEmpty() {
        return getHeight() == 0 || getWidth() == 0;
    }

    /**
     * Returns if the dimensions of the bounds is 0.
     */
    public boolean isZero() {
        return getHeight() == 0 && getWidth() == 0;
    }

    @NonNull
    @Override
    public String toString() {
        return Bounds.class.getSimpleName() + " { [" + left + "," + top + "," + right + "," + bottom + "] }";
    }

    @Override
    public boolean equals(@Nullable Object obj) {
        if (this == obj) return true;
        if (!(obj instanceof Bounds other)) return false;

        if (left != other.left) return false;
        if (top != other.top) return false;
        if (right != other.right) return false;
        if (bottom != other.bottom) return false;

        return true;
    }

    @Override
    public int hashCode() {
        int result = left;
        result = 31 * result + top;
        result = 31 * result + right;
        result = 31 * result + bottom;
        return result;
    }
}
