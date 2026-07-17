#pragma once
#include <jni.h>
#include "zygoteloader/zygoteloader.h"

struct HookArgs {
    JNIEnv *env;
    Resource *classesDex;
};

void hack_thread(HookArgs *args);

extern "C" void onConfigurationChanged_native(JNIEnv *env, jclass clazz, jobject activity, jobject newConfig);

#define HOOK_DEF(ret, func, ...) \
  void* addr_##func; \
  ret (*orig_##func)(__VA_ARGS__); \
  ret new_##func(__VA_ARGS__)
