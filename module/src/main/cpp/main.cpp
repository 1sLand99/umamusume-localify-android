#include <jni.h>

#include <fcntl.h>

#include <sys/mman.h>

#include <unistd.h>

#include "stdinclude.hpp"

#include <pthread.h>
#include <dobby.h>

#include "hook.h"
#include "zygisk.hpp"

#include "zygoteloader/dex.hpp"
#include "zygoteloader/serializer.h"
#include "zygoteloader/zygoteloader.h"

enum FileCommand : int {
    INITIALIZE, IS_INITIALIZED, GET_RESOURCES
};

using zygisk::Api;
using zygisk::AppSpecializeArgs;
using zygisk::ServerSpecializeArgs;

void handleFileRequest(int client) {
    static pthread_mutex_t initializeLock = PTHREAD_MUTEX_INITIALIZER;
    static int classesDex = -1;
    static int moduleDirectory = -1;

    int command = -1;
    serializer_read_int(client, &command);

    switch (static_cast<FileCommand>(command)) {
        case INITIALIZE: {
            pthread_mutex_lock(&initializeLock);
            if (moduleDirectory == -1) {
                serializer_read_file_descriptor(client, &moduleDirectory);
                classesDex = openat(moduleDirectory, "classes.dex", O_RDONLY);
            }
            serializer_write_int(client, 1);
            pthread_mutex_unlock(&initializeLock);
            break;
        }
        case IS_INITIALIZED: {
            pthread_mutex_lock(&initializeLock);
            serializer_write_int(client, moduleDirectory != -1 ? 1 : 0);
            pthread_mutex_unlock(&initializeLock);
            break;
        }
        case GET_RESOURCES: {
            serializer_write_file_descriptor(client, classesDex);
            break;
        }
    }
}

class Module : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;

        if (!isInitialized()) {
            initialize();
        }
    }

    static bool isGame(const char *pkgNm) {
        if (!pkgNm)
            return false;
        if (Game::IsPackageNameEqualsByGameRegion(pkgNm, Game::Region::JPN) ||
            Game::IsPackageNameEqualsByGameRegion(pkgNm, Game::Region::KOR) ||
            Game::IsPackageNameEqualsByGameRegion(pkgNm, Game::Region::TWN) ||
            Game::IsPackageNameEqualsByGameRegion(pkgNm, Game::Region::CHN) ||
            Game::IsPackageNameEqualsByGameRegion(pkgNm, Game::Region::ENG)) {
            LOGI("detect package: %s", pkgNm);
            return true;
        }
        return false;
    }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        if (!args || !args->nice_name) {
            LOGW("Skip unknown process");
            return;
        }
        auto pkgNm = env->GetStringUTFChars(args->nice_name, nullptr);
        enable_hack = isGame(pkgNm);
        if (enable_hack) {
            fetchResources();
        } else {
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
        }
        env->ReleaseStringUTFChars(args->nice_name, pkgNm);
    }

    void postAppSpecialize(const AppSpecializeArgs *) override {
        if (enable_hack) {
            if (!IsABIRequiredNativeBridge())
            {
                if (classesDex != nullptr) {
                    dex_load_and_invoke(
                            env,
                            classesDex->base, classesDex->length
                    );
                }
            }

            int ret;
            pthread_t t;

            HookArgs* args = reinterpret_cast<HookArgs*>(malloc(sizeof(HookArgs)));
            args->env = env;
            args->classesDex = classesDex;
            ret = pthread_create(&t, nullptr, reinterpret_cast<void *(*)(void *)>(hack_thread), args);
            if (ret != 0) {
                LOGE("can't create thread: %s\n", strerror(ret));
            }
        }
    }

    void fetchResources() {
        const int remote = api->connectCompanion();
        serializer_write_int(remote, GET_RESOURCES);
        int classesDexFd = -1;
        serializer_read_file_descriptor(remote, &classesDexFd);
        classesDex = resource_map_fd(classesDexFd);
        close(remote);
        close(classesDexFd);
    }

private:
    JNIEnv *env{};
    Api *api{};
    Resource *classesDex{};
    bool enable_hack;

    bool isInitialized() {
        const int remote = api->connectCompanion();

        serializer_write_int(remote, IS_INITIALIZED);

        int initialized = -1;
        serializer_read_int(remote, &initialized);

        close(remote);

        return initialized != 0;
    }

    void initialize() {
        const int remote = api->connectCompanion();

        const int moduleDir = api->getModuleDir();

        serializer_write_int(remote, INITIALIZE);
        serializer_write_file_descriptor(remote, moduleDir);
    }
};

REGISTER_ZYGISK_MODULE(Module)

REGISTER_ZYGISK_COMPANION(handleFileRequest)

extern "C" {
[[gnu::visibility("default"), maybe_unused]]
void hook(JNIEnv *env, Resource *classesDex) {
    if (IsRunningOnNativeBridge()) {
        LOGD("Starting on NativeBridge...");
        Game::CurrentGameRegion = Game::CheckPackageNameByDataPath();
        if (Game::CurrentGameRegion == Game::Region::UNKNOWN) {
            LOGW("Region UNKNOWN...");
            return;
        }

        if (classesDex != nullptr) {
            dex_load_and_invoke(
                    env,
                    classesDex->base, classesDex->length
            );
        }

        int ret;
        pthread_t t;

        HookArgs* args = reinterpret_cast<HookArgs*>(malloc(sizeof(HookArgs)));
        args->env = env;
        args->classesDex = classesDex;
        ret = pthread_create(&t, nullptr,
                             reinterpret_cast<void *(*)(void *)>(hack_thread), args);
        if (ret != 0) {
            LOGE("can't create thread: %s\n", strerror(ret));
        }
    }
}
}
