#pragma once

#include <jni.h>
#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

inline jobject windowMetricsCalculator;

void dex_load_and_invoke(
        JNIEnv *env,
        const void *dex_block, uint32_t dex_length
);

void register_callback(JNIEnv *env, jobject activity);

jobject getDisplayCutoutInsets(JNIEnv *env, jobject activity);

jobject getCaptionBarInsets(JNIEnv *env, jobject activity);

jobject getCaptionBarInsetsIgnoringVisibility(JNIEnv *env, jobject activity);

bool isEdgeToEdgeEnabled(JNIEnv *env, jobject activity);

void showMediaNotification(JNIEnv *env, jobject context);

void updateMediaMetadata(JNIEnv* env, jobject context, const char* title, const char* artist, const std::vector<uint8_t>& png_data);

bool hasArtworkData(JNIEnv* env);

void updateMediaProgress(JNIEnv* env, jobject context, int64_t positionMs, int64_t durationMs);

void updateMediaPlaybackState(JNIEnv* env, jobject context, int state);

void updateMediaPlayWhenReady(JNIEnv* env, jobject context, bool playWhenReady);

void updateMediaNavigationButtons(JNIEnv* env, jobject context, bool next, bool previous);

void hideNotification(JNIEnv *env, jobject context);

JNIEnv* GetJNIEnv();

jobject GetActivity();

#ifdef __cplusplus
};
#endif
