#ifndef UMAMUSUMELOCALIFYANDROID_IL2CPP_HOOK_H
#define UMAMUSUMELOCALIFYANDROID_IL2CPP_HOOK_H

void il2cpp_hook_init(void *handle);

std::string get_application_version();

void il2cpp_hook();

void il2cpp_load_assetbundle();

extern "C" void onConfigurationChanged_native(JNIEnv *env, jobject /*this*/, jobject activity, jobject newConfig);

#endif //UMAMUSUMELOCALIFYANDROID_IL2CPP_HOOK_H
