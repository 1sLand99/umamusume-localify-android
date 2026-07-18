#pragma once

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

inline jobject windowMetricsCalculator;

void dex_load_and_invoke(
        JNIEnv *env,
        const void *dex_block, uint32_t dex_length
);

void register_callback(JNIEnv *env, jobject activity);

jobject getCaptionBarInsets(JNIEnv *env, jobject activity);

bool isEdgeToEdgeEnabled(JNIEnv *env, jobject activity);

#ifdef __cplusplus
};
#endif
