#pragma once
#include <jni.h>
#include "zygoteloader/zygoteloader.h"

struct HookArgs {
    JNIEnv *env;
    Resource *classesDex;
};

void hack_thread(HookArgs *args);

extern "C" void onLayoutChange_native(JNIEnv *env, jclass clazz, jobject activity, jobject view, jint left, jint top, jint right, jint bottom, jint oldLeft, jint oldTop, jint oldRight, jint oldBottom);

#define HOOK_DEF(ret, func, ...) \
  void* addr_##func; \
  ret (*orig_##func)(__VA_ARGS__); \
  ret new_##func(__VA_ARGS__)
