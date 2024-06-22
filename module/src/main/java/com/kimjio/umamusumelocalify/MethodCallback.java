package com.kimjio.umamusumelocalify;

import java.lang.reflect.Method;

public final class MethodCallback {
    Method backup;
    Object[] args;

    MethodCallback(Method backup, Object[] args) {
        this.backup = backup;
        this.args = args;
    }
}
