#pragma once

void hack_thread(void *arg);

extern "C" void onConfigurationChanged_native(JNIEnv *env, jobject /*this*/, jobject activity, jobject newConfig);

#define HOOK_DEF(ret, func, ...) \
  void* addr_##func; \
  ret (*orig_##func)(__VA_ARGS__); \
  ret new_##func(__VA_ARGS__)
