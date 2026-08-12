#include "dex.hpp"

#include "log.h"

#include <jni.h>
#include <string>
#include <vector>
#include <cstdint>
#include <dlfcn.h>

#include "hook.h"

#define find_class(var_name, name) jclass var_name = env->FindClass(name)
#define find_static_method(var_name, clazz, name, signature) jmethodID var_name = env->GetStaticMethodID(clazz, name, signature)
#define find_method(var_name, clazz, name, signature) jmethodID var_name = env->GetMethodID(clazz, name, signature)
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

static void check_dex_exception(JNIEnv *env, const char *name) {
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

        LOGE("%s exception: %s", name, error_msg.c_str());

        env->DeleteLocalRef(exception);
        env->DeleteLocalRef(throwable_class);
        env->DeleteLocalRef(frame_class);
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
            {
                    .name = "onLayoutChange_native",
                    .signature = "(Landroid/app/Activity;Landroid/view/View;IIIIIIII)V",
                    .fnPtr = reinterpret_cast<void *>(onLayoutChange_native)
            },
            {
                    .name = "handleSetPlayWhenReady_native",
                    .signature = "(Z)V",
                    .fnPtr = reinterpret_cast<void *>(handleSetPlayWhenReady_native)
            },
            {
                    .name = "handleSeek_native",
                    .signature = "(IJI)V",
                    .fnPtr = reinterpret_cast<void *>(handleSeek_native)
            }
    };

    env->RegisterNatives(localify_class, methods.data(), static_cast<jint>(methods.size()));

    find_static_method(m_load, localify_class, "load", "()V");

    env->CallStaticVoidMethod(
            localify_class,
            m_load
    );

    env->DeleteLocalRef(o_dex_class_loader);
    env->DeleteLocalRef(c_class_loader);

    check_dex_exception(env, "Dex load");
}

void register_callback(JNIEnv *env, jobject activity) {
    find_static_method(m_registerCallback, localify_class, "registerCallback",
                       "(Landroid/app/Activity;)V");

    env->CallStaticVoidMethod(
            localify_class,
            m_registerCallback,
            activity
    );

    check_dex_exception(env, __FUNCTION__);
}

jobject getDisplayCutoutInsets(JNIEnv *env, jobject activity) {
    find_static_method(m_getDisplayCutoutInsets, localify_class, "getDisplayCutoutInsets",
                       "(Landroid/app/Activity;)Landroidx/core/graphics/Insets;");

    auto insets = env->CallStaticObjectMethod(
            localify_class,
            m_getDisplayCutoutInsets,
            activity
    );
    check_dex_exception(env, __FUNCTION__);
    return insets;
}

jobject getCaptionBarInsets(JNIEnv *env, jobject activity) {
    find_static_method(m_getCaptionBarInsets, localify_class, "getCaptionBarInsets",
                       "(Landroid/app/Activity;)Landroidx/core/graphics/Insets;");

    auto insets = env->CallStaticObjectMethod(
            localify_class,
            m_getCaptionBarInsets,
            activity
    );
    check_dex_exception(env, __FUNCTION__);
    return insets;
}

jobject getCaptionBarInsetsIgnoringVisibility(JNIEnv *env, jobject activity) {
    find_static_method(m_getCaptionBarInsets, localify_class,
                       "getCaptionBarInsetsIgnoringVisibility",
                       "(Landroid/app/Activity;)Landroidx/core/graphics/Insets;");

    auto insets = env->CallStaticObjectMethod(
            localify_class,
            m_getCaptionBarInsets,
            activity
    );
    check_dex_exception(env, __FUNCTION__);
    return insets;
}

bool isEdgeToEdgeEnabled(JNIEnv *env, jobject activity) {
    find_static_method(m_isEdgeToEdgeEnabled, localify_class, "isEdgeToEdgeEnabled",
                       "(Landroid/app/Activity;)Z");
    auto isEdgeToEdge = static_cast<bool>(env->CallStaticBooleanMethod(
            localify_class,
            m_isEdgeToEdgeEnabled,
            activity
    ));
    check_dex_exception(env, __FUNCTION__);
    return isEdgeToEdge;
}

void showMediaNotification(JNIEnv *env, jobject context) {
    find_static_method(m_showMediaNotification, localify_class, "showMediaNotification",
                       "(Landroid/content/Context;)V");
    env->CallStaticVoidMethod(
            localify_class,
            m_showMediaNotification,
            context
    );
    check_dex_exception(env, __FUNCTION__);
}

void updateMediaMetadata(JNIEnv *env, jobject context, const char *title, const char *artist,
                         const std::vector<uint8_t> &png_data) {
    jstring j_title = env->NewStringUTF(title);
    jstring j_artist = env->NewStringUTF(artist);

    jbyteArray j_artwork = env->NewByteArray(png_data.size());
    env->SetByteArrayRegion(j_artwork, 0, png_data.size(),
                            reinterpret_cast<const jbyte *>(png_data.data()));

    find_static_method(m_updateMetadata, localify_class, "updateMetadata",
                       "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;[B)V");
    env->CallStaticVoidMethod(localify_class, m_updateMetadata, context, j_title, j_artist,
                              j_artwork);
    check_dex_exception(env, __FUNCTION__);

    env->DeleteLocalRef(j_title);
    env->DeleteLocalRef(j_artist);
    env->DeleteLocalRef(j_artwork);
}

bool hasArtworkData(JNIEnv *env) {
    find_static_method(m_hasArtworkData, localify_class, "hasArtworkData", "()Z");
    bool hasArtworkData = env->CallStaticBooleanMethod(localify_class, m_hasArtworkData);
    check_dex_exception(env, __FUNCTION__);
    return hasArtworkData;
}

void updateMediaProgress(JNIEnv *env, jobject context, int64_t positionMs, int64_t durationMs) {
    find_static_method(m_updateProgress, localify_class, "updateProgress",
                       "(Landroid/content/Context;JJ)V");
    env->CallStaticVoidMethod(localify_class, m_updateProgress, context,
                              static_cast<jlong>(positionMs), static_cast<jlong>(durationMs));
    check_dex_exception(env, __FUNCTION__);
}

void updateMediaPlaybackState(JNIEnv *env, jobject context, int state) {
    find_static_method(m_updatePlaybackState, localify_class, "updatePlaybackState",
                       "(Landroid/content/Context;I)V");
    env->CallStaticVoidMethod(localify_class, m_updatePlaybackState, context,
                              static_cast<jint>(state));
    check_dex_exception(env, __FUNCTION__);
}

void updateMediaPlayWhenReady(JNIEnv *env, jobject context, bool playWhenReady) {
    find_static_method(m_updatePlayWhenReady, localify_class, "updatePlayWhenReady",
                       "(Landroid/content/Context;Z)V");
    env->CallStaticVoidMethod(localify_class, m_updatePlayWhenReady, context,
                              static_cast<jboolean>(playWhenReady));
    check_dex_exception(env, __FUNCTION__);
}

void updateMediaNavigationButtons(JNIEnv *env, jobject context, bool next, bool previous) {
    find_static_method(m_updateNavigationButtons, localify_class, "updateNavigationButtons",
                       "(Landroid/content/Context;ZZ)V");
    env->CallStaticVoidMethod(localify_class, m_updateNavigationButtons, context,
                              static_cast<jboolean>(next), static_cast<jboolean>(previous));
    check_dex_exception(env, __FUNCTION__);
}

void hideNotification(JNIEnv *env, jobject context) {
    find_static_method(m_hideNotification, localify_class, "hideNotification",
                       "(Landroid/content/Context;)V");
    env->CallStaticVoidMethod(
            localify_class,
            m_hideNotification,
            context
    );
    check_dex_exception(env, __FUNCTION__);
}

JNIEnv *GetJNIEnv() {
    static JNIEnv *env;
    if (env) {
        return env;
    }

    void *handle = dlopen("libnativehelper.so", RTLD_NOW);

    auto JNI_GetCreatedJavaVMs_fn = reinterpret_cast<decltype(JNI_GetCreatedJavaVMs) *>(dlsym(
            handle, "JNI_GetCreatedJavaVMs"));

    if (JNI_GetCreatedJavaVMs_fn) {
        JavaVM *javaVM;
        jsize numVMs = 0;
        JNI_GetCreatedJavaVMs_fn(&javaVM, 1, &numVMs);

        javaVM->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
        dlclose(handle);

        return env;
    }

    dlclose(handle);
    return nullptr;
}

jobject GetActivity() {
    static jobject activity;
    if (activity) {
        return activity;
    }

    auto env = GetJNIEnv();
    auto UnityPlayerClass = env->FindClass(
            "com/unity3d/player/UnityPlayer");
    auto currentActivityID = env->GetStaticFieldID(UnityPlayerClass,
                                                   "currentActivity",
                                                   "Landroid/app/Activity;");
    activity = env->NewGlobalRef(env->GetStaticObjectField(UnityPlayerClass,
                                                           currentActivityID));
    env->DeleteLocalRef(UnityPlayerClass);

    return activity;
}

#ifdef __cplusplus
}
#endif
