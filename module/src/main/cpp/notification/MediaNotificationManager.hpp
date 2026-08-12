#pragma onc

#include <string>
#include <filesystem>

#include "il2cpp/il2cpp_symbols.hpp"
#include "il2cpp/il2cpp-api-functions.hpp"
#include "string_utils.hpp"
#include "masterdb/masterdb.hpp"
#include "zygoteloader/dex.hpp"

#include "scripts/UnityEngine.CoreModule/UnityEngine/Rect.hpp"
#include "scripts/UnityEngine.CoreModule/UnityEngine/RenderTexture.hpp"
#include "scripts/UnityEngine.AssetBundleModule/UnityEngine/AssetBundle.hpp"

#include "scripts/ScriptInternal.hpp"

using namespace std;

namespace MediaNotificationManager {
    inline int musicId = 0;

    inline void UpdateMetadata(int music_id = musicId) {
        if (music_id == 0) {
            return;
        }

        if (musicId == music_id && hasArtworkData(GetJNIEnv())) {
            return;
        }

        musicId = music_id;

        auto title = MasterDB::GetTextData(16, music_id);

        auto artist = MasterDB::GetTextData(17, music_id);
        replaceAll(artist, "\\n", ", ");

        auto music_id_str = to_string(music_id);
        size_t n_zero = 4;
        auto new_str =
                std::string(n_zero - std::min(n_zero, music_id_str.length()), '0') + music_id_str;

        auto jacket_icon = "jacket_icon_l_"s + new_str;
        auto path = "Live/Jacket/"s + jacket_icon;

        auto loader = il2cpp_symbols::get_method_pointer<Il2CppObject *(*)()>("umamusume.dll",
                                                                              "Gallop",
                                                                              "AssetManager",
                                                                              "get_Loader",
                                                                              IgnoreNumberOfArguments)();
        auto asset = il2cpp_class_get_method_from_name_type<
                Il2CppObject *(*)(Il2CppObject *, Il2CppString *, bool)>(loader->klass,
                                                                         "LoadAssetHandle",
                                                                         2)->methodPointer(
                loader, il2cpp_string_new(path.data()), false);
        if (!asset) {
            return;
        }

        UnityEngine::AssetBundle assetBundle = il2cpp_class_get_method_from_name_type<
                Il2CppObject *(*)(Il2CppObject *)>(asset->klass, "get_assetBundle",
                                                   0)->methodPointer(asset);
        if (!assetBundle) {
            return;
        }

        auto texture2D = assetBundle.LoadAsset(il2cpp_string_new(jacket_icon.data()),
                                               GetRuntimeType("UnityEngine.CoreModule.dll",
                                                              "UnityEngine", "Texture2D"));

        auto width = il2cpp_class_get_method_from_name_type<int (*)
                (Il2CppObject *)>(texture2D->klass, "get_width", 0)->methodPointer(texture2D);

        auto height = il2cpp_class_get_method_from_name_type<int (*)
                (Il2CppObject *)>(texture2D->klass, "get_height", 0)->methodPointer(texture2D);

        auto renderTexture = UnityEngine::RenderTexture::GetTemporary(width, height);

        il2cpp_symbols::get_method_pointer<void (*)
                (Il2CppObject *, Il2CppObject *)>
                ("UnityEngine.CoreModule.dll", "UnityEngine", "Graphics", "Blit", 2)(texture2D,
                                                                                     renderTexture);

        auto previous = UnityEngine::RenderTexture::GetActive();

        UnityEngine::RenderTexture::SetActive(renderTexture);

        auto readableTexture = il2cpp_object_new(
                il2cpp_symbols::get_class("UnityEngine.CoreModule.dll", "UnityEngine",
                                          "Texture2D"));
        il2cpp_class_get_method_from_name_type<void (*)
                (Il2CppObject *, int, int)>
                (readableTexture->klass, ".ctor", 2)->methodPointer(readableTexture, width, height);

        il2cpp_class_get_method_from_name_type<void (*)
                (Il2CppObject *, UnityEngine::Rect, int, int)>
                (readableTexture->klass, "ReadPixels", 3)->methodPointer(readableTexture,
                                                                         UnityEngine::Rect{0, 0,
                                                                                           static_cast<float>(width),
                                                                                           static_cast<float>(height)},
                                                                         0, 0);
        il2cpp_class_get_method_from_name_type<void (*)
                (Il2CppObject *)>(readableTexture->klass, "Apply", 0)->methodPointer(
                readableTexture);

        UnityEngine::RenderTexture::SetActive(previous);

        UnityEngine::RenderTexture::ReleaseTemporary(renderTexture);

        auto method = il2cpp_symbols::get_method("UnityEngine.ImageConversionModule.dll",
                                                 "UnityEngine", "ImageConversion", "EncodeToPNG",
                                                 1);

        void **params = new void *[1];
        params[0] = readableTexture;

        Il2CppException *exception;

        auto pngData = reinterpret_cast<Il2CppArraySize_t<uint8_t> *>(il2cpp_runtime_invoke(method,
                                                                                            nullptr,
                                                                                            params,
                                                                                            &exception));

        if (!exception) {
            updateMediaMetadata(GetJNIEnv(), GetActivity(), title.data(), artist.data(), vector<uint8_t>(pngData->vector, pngData->vector + pngData->max_length));
        } else {
            LOGW("%s", il2cpp_u8(exception->message->chars).data());
        }
    }
}
