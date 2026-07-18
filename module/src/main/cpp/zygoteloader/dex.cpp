#include "dex.hpp"

#include "log.h"

#include <jni.h>
#include <string>
#include <vector>
#include <cstdint>

#include "hook.h"

#define find_class(var_name, name) jclass var_name = env->FindClass(name);
#define find_static_method(var_name, clazz, name, signature) jmethodID var_name = env->GetStaticMethodID(clazz, name, signature);
#define find_method(var_name, clazz, name, signature) jmethodID var_name = env->GetMethodID(clazz, name, signature);
#define new_string(text) env->NewStringUTF(text)

#ifdef __cplusplus
extern "C" {
#endif

std::string error_msg;

static void _append_exception_trace_messages(
        JNIEnv &a_jni_env,
        std::string &a_error_msg,
        jthrowable a_exception,
        jmethodID a_mid_throwable_getCause,
        jmethodID a_mid_throwable_getStackTrace,
        jmethodID a_mid_throwable_toString,
        jmethodID a_mid_frame_toString) {
    // Get the array of StackTraceElements.
    auto frames =
            static_cast<jobjectArray>(a_jni_env.CallObjectMethod(
                    a_exception,
                    a_mid_throwable_getStackTrace));
    const auto frames_length = a_jni_env.GetArrayLength(frames);

    // Add Throwable.toString() before descending
    // stack trace messages.
    if (frames) {
        auto msg_obj =
                static_cast<jstring>(a_jni_env.CallObjectMethod(a_exception,
                                                                a_mid_throwable_toString));
        const char *msg_str = a_jni_env.GetStringUTFChars(msg_obj, nullptr);

        // If this is not the top-of-the-trace then
        // this is a cause.
        if (!a_error_msg.empty()) {
            a_error_msg += "\nCaused by: ";
            a_error_msg += msg_str;
        } else {
            a_error_msg = msg_str;
        }

        a_jni_env.ReleaseStringUTFChars(msg_obj, msg_str);
        a_jni_env.DeleteLocalRef(msg_obj);
    }

    // Append stack trace messages if there are any.
    if (frames_length > 0) {
        jsize i;
        for (i = 0; i < frames_length; i++) {
            // Get the string returned from the 'toString()'
            // method of the next frame and append it to
            // the error message.
            auto frame = a_jni_env.GetObjectArrayElement(frames, i);
            auto msg_obj =
                    static_cast<jstring>(a_jni_env.CallObjectMethod(frame,
                                                                    a_mid_frame_toString));

            const char *msg_str = a_jni_env.GetStringUTFChars(msg_obj, nullptr);

            a_error_msg += "\n    ";
            a_error_msg += msg_str;

            a_jni_env.ReleaseStringUTFChars(msg_obj, msg_str);
            a_jni_env.DeleteLocalRef(msg_obj);
            a_jni_env.DeleteLocalRef(frame);
        }
    }

    // If 'a_exception' has a cause then append the
    // stack trace messages from the cause.
    if (frames) {
        auto cause =
                static_cast<jthrowable>(a_jni_env.CallObjectMethod(
                        a_exception,
                        a_mid_throwable_getCause));
        if (cause) {
            _append_exception_trace_messages(a_jni_env,
                                             a_error_msg,
                                             cause,
                                             a_mid_throwable_getCause,
                                             a_mid_throwable_getStackTrace,
                                             a_mid_throwable_toString,
                                             a_mid_frame_toString);
            a_jni_env.DeleteLocalRef(cause);
        }
    }
}

jclass localify_class;

void dex_load_and_invoke(
        JNIEnv *env,
        const void *dex_block, uint32_t dex_length
) {
    find_class(c_class_loader, "java/lang/ClassLoader");
    find_static_method(m_get_system_class_loader, c_class_loader, "getSystemClassLoader",
                       "()Ljava/lang/ClassLoader;");
    auto o_system_class_loader = env->CallStaticObjectMethod(
            c_class_loader,
            m_get_system_class_loader
    );

    find_class(c_dex_class_loader, "dalvik/system/InMemoryDexClassLoader");
    find_method(m_dex_class_loader, c_dex_class_loader, "<init>",
                "(Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V");
    auto byteBuffer = env->NewDirectByteBuffer(const_cast<void *>(dex_block), dex_length);
    auto o_dex_class_loader = env->NewObject(
            c_dex_class_loader,
            m_dex_class_loader,
            byteBuffer,
            o_system_class_loader
    );
    env->DeleteLocalRef(byteBuffer);
    env->DeleteLocalRef(o_system_class_loader);
    env->DeleteLocalRef(c_dex_class_loader);

    find_method(m_load_class, c_class_loader, "loadClass",
                "(Ljava/lang/String;)Ljava/lang/Class;");

    auto windowMetricsCalculatorClassName = new_string(
            "androidx.window.layout.WindowMetricsCalculatorCompat");
    auto windowMetricsCalculatorClass = static_cast<jclass>(env->CallObjectMethod(
            o_dex_class_loader,
            m_load_class,
            windowMetricsCalculatorClassName
    ));
    env->DeleteLocalRef(windowMetricsCalculatorClassName);

    auto windowMetricsCalculatorInitId = env->GetMethodID(windowMetricsCalculatorClass,
                                                          "<init>",
                                                          "()V");

    jobject windowMetricsCalculatorLocal = env->NewObject(windowMetricsCalculatorClass,
                                                          windowMetricsCalculatorInitId);

    windowMetricsCalculator = env->NewGlobalRef(windowMetricsCalculatorLocal);
    env->DeleteLocalRef(windowMetricsCalculatorLocal);
    env->DeleteLocalRef(windowMetricsCalculatorClass);

    auto localifyClassName = new_string("com.kimjio.umamusumelocalify.UmamusumeLocalify");
    auto localify_class_local = static_cast<jclass>(env->CallObjectMethod(
            o_dex_class_loader,
            m_load_class,
            localifyClassName
    ));
    env->DeleteLocalRef(localifyClassName);

    localify_class = static_cast<jclass>(env->NewGlobalRef(localify_class_local));
    env->DeleteLocalRef(localify_class_local);

    std::vector<JNINativeMethod> methods = {
            {.name = "onLayoutChange_native",
                    .signature = "(Landroid/app/Activity;Landroid/view/View;IIIIIIII)V",
                    .fnPtr = reinterpret_cast<void *>(onLayoutChange_native)}
    };

    env->RegisterNatives(localify_class, methods.data(), static_cast<jint>(methods.size()));

    find_static_method(m_load, localify_class, "load", "()V");

    env->CallStaticVoidMethod(
            localify_class,
            m_load
    );

    env->DeleteLocalRef(o_dex_class_loader);
    env->DeleteLocalRef(c_class_loader);

    if (env->ExceptionCheck()) {
        LOGW("Dex exception!");

        jthrowable exception = env->ExceptionOccurred();
        env->ExceptionClear();

        error_msg.clear();

        jclass throwable_class = env->FindClass("java/lang/Throwable");
        jmethodID mid_throwable_getCause =
                env->GetMethodID(throwable_class,
                                 "getCause",
                                 "()Ljava/lang/Throwable;");
        jmethodID mid_throwable_getStackTrace =
                env->GetMethodID(throwable_class,
                                 "getStackTrace",
                                 "()[Ljava/lang/StackTraceElement;");
        jmethodID mid_throwable_toString =
                env->GetMethodID(throwable_class,
                                 "toString",
                                 "()Ljava/lang/String;");

        jclass frame_class = env->FindClass("java/lang/StackTraceElement");
        jmethodID mid_frame_toString =
                env->GetMethodID(frame_class,
                                 "toString",
                                 "()Ljava/lang/String;");
        _append_exception_trace_messages(*env,
                                         error_msg,
                                         exception,
                                         mid_throwable_getCause,
                                         mid_throwable_getStackTrace,
                                         mid_throwable_toString,
                                         mid_frame_toString);

        LOGE("Dex load exception: %s", error_msg.c_str());

        env->DeleteLocalRef(exception);
        env->DeleteLocalRef(throwable_class);
        env->DeleteLocalRef(frame_class);
    }
}

void register_callback(JNIEnv *env, jobject activity) {
    find_static_method(m_registerCallback, localify_class, "registerCallback",
                       "(Landroid/app/Activity;)V");

    env->CallStaticVoidMethod(
            localify_class,
            m_registerCallback,
            activity
    );

    if (env->ExceptionCheck()) {
        LOGW("Dex exception!");

        jthrowable exception = env->ExceptionOccurred();
        env->ExceptionClear();

        error_msg.clear();

        jclass throwable_class = env->FindClass("java/lang/Throwable");
        jmethodID mid_throwable_getCause =
                env->GetMethodID(throwable_class,
                                 "getCause",
                                 "()Ljava/lang/Throwable;");
        jmethodID mid_throwable_getStackTrace =
                env->GetMethodID(throwable_class,
                                 "getStackTrace",
                                 "()[Ljava/lang/StackTraceElement;");
        jmethodID mid_throwable_toString =
                env->GetMethodID(throwable_class,
                                 "toString",
                                 "()Ljava/lang/String;");

        jclass frame_class = env->FindClass("java/lang/StackTraceElement");
        jmethodID mid_frame_toString =
                env->GetMethodID(frame_class,
                                 "toString",
                                 "()Ljava/lang/String;");
        _append_exception_trace_messages(*env,
                                         error_msg,
                                         exception,
                                         mid_throwable_getCause,
                                         mid_throwable_getStackTrace,
                                         mid_throwable_toString,
                                         mid_frame_toString);

        LOGE("register_callback exception: %s", error_msg.c_str());

        env->DeleteLocalRef(exception);
        env->DeleteLocalRef(throwable_class);
        env->DeleteLocalRef(frame_class);
    }
}

jobject getCaptionBarInsets(JNIEnv *env, jobject activity) {
    find_static_method(m_getCaptionBarInsets, localify_class, "getCaptionBarInsets",
                       "(Landroid/app/Activity;)Landroidx/core/graphics/Insets;");
    return env->CallStaticObjectMethod(
            localify_class,
            m_getCaptionBarInsets,
            activity
    );
}

bool isEdgeToEdgeEnabled(JNIEnv *env, jobject activity) {
    find_static_method(m_isEdgeToEdgeEnabled, localify_class, "isEdgeToEdgeEnabled",
                       "(Landroid/app/Activity;)Z");
    return env->CallStaticBooleanMethod(
            localify_class,
            m_isEdgeToEdgeEnabled,
            activity
    );
}

#ifdef __cplusplus
}
#endif
