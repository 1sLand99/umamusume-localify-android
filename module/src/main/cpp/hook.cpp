#include <jni.h>
#include <cstring>
#include <sstream>
#include <thread>
#include <dlfcn.h>

#include "game.hpp"
#include "local.hpp"
#include "stdinclude.hpp"

#include <dobby.h>

#include "hook.h"

#include "log.h"

#include "logger/logger.hpp"
#include "config.hpp"
#include "native_bridge_itf.h"

#include "il2cpp_dump.h"

#include "il2cpp/il2cpp_symbols.hpp"

#include "msgpack/msgpack_modify.hpp"

#include "localify/NotificationManager.hpp"
#include "localify/UIParts.hpp"
#include "localify/LiveUtils.hpp"

#include "scripts/ScriptInternal.hpp"

#include "scripts/mscorlib/System/Collections/Generic/Dictionary.hpp"
#include "scripts/mscorlib/System/ValueTuple.hpp"

#include "scripts/Cute.Cri.Assembly/Cute/Cri/MoviePlayerHandle.hpp"
#include "scripts/Cute.Cri.Assembly/Cute/Cri/MoviePlayerForUI.hpp"

#include "scripts/UnityEngine.CoreModule/UnityEngine/RectTransform.hpp"
#include "scripts/UnityEngine.CoreModule/UnityEngine/Screen.hpp"
#include "scripts/UnityEngine.CoreModule/UnityEngine/SceneManagement/Scene.hpp"
#include "scripts/UnityEngine.CoreModule/UnityEngine/Vector2Int.hpp"

#include "scripts/umamusume/Gallop/GameSystem.hpp"
#include "scripts/umamusume/Gallop/GraphicSettings.hpp"
#include "scripts/umamusume/Gallop/UIManager.hpp"
#include "scripts/umamusume/Gallop/SceneManager.hpp"
#include "scripts/umamusume/Gallop/Screen.hpp"
#include "scripts/umamusume/Gallop/StoryViewController.hpp"
#include "scripts/umamusume/Gallop/RaceCameraManager.hpp"
#include "scripts/umamusume/Gallop/Localize.hpp"
#include "scripts/umamusume/Gallop/LowResolutionCameraUtil.hpp"

#include "scripts/Plugins/AnimateToUnity/AnRootManager.hpp"

using namespace std;
using namespace logger;

namespace {
    void patch_game_assembly();

    void init_il2cpp() {
        if (config::dump_il2cpp) {
            il2cpp_dump();
        }

        il2cpp_symbols::init_defaults();
        il2cpp_symbols::call_init_callbacks();

        il2cpp_symbols::late_init_callbacks.emplace_back(patch_game_assembly);
    }

    void *il2cpp_init_addr = nullptr;
    void *il2cpp_init_orig = nullptr;

    static bool il2cpp_init_hook(const char *domain_name) {
        const auto result = reinterpret_cast<decltype(il2cpp_init_hook) *>(il2cpp_init_orig)(
                domain_name);

        auto unityVersion = il2cpp_resolve_icall_type<Il2CppString *(*)()>(
                "UnityEngine.Application::get_unityVersion")();

        if (IL2CPP_BASIC_STRING(unityVersion->chars).contains(IL2CPP_STRING("2020"))) {
            Game::CurrentUnityVersion = Game::UnityVersion::Unity20;
        } else {
            Game::CurrentUnityVersion = Game::UnityVersion::Unity22;
        }

        if (result) {
            il2cpp_symbols::il2cpp_domain = il2cpp_domain_get();
            if (Game::CurrentUnityVersion != Game::UnityVersion::Unity20) {
                init_il2cpp();
            }

            DobbyDestroy(il2cpp_init_addr);
        }
        return result;
    }

    void StartTickFrame();

    void SetBGCanvasScalerSize() {
        auto bgManager = il2cpp_symbols::get_method_pointer<Il2CppObject *(*)()>("umamusume.dll",
                                                                                 "Gallop",
                                                                                 "BGManager",
                                                                                 "get_Instance",
                                                                                 0)();
        if (bgManager) {
            auto _mainBgField = il2cpp_class_get_field_from_name(bgManager->klass, "_mainBg");
            Il2CppObject *_mainBg;
            il2cpp_field_get_value(bgManager, _mainBgField, &_mainBg);

            if (_mainBg) {
                auto _currentBgWidthField = il2cpp_class_get_field_from_name(bgManager->klass,
                                                                             "_currentBgWidth");
                int _currentBgWidth;
                il2cpp_field_get_value(bgManager, _currentBgWidthField, &_currentBgWidth);

                auto _currentBgHeightField = il2cpp_class_get_field_from_name(bgManager->klass,
                                                                              "_currentBgHeight");
                int _currentBgHeight;
                il2cpp_field_get_value(bgManager, _currentBgHeightField, &_currentBgHeight);

                if (!_currentBgWidth || !_currentBgHeight) {
                    return;
                }

                float ratio =
                        static_cast<float>(_currentBgWidth) / static_cast<float>(_currentBgHeight);

                int width = Gallop::Screen::Width();
                int height = Gallop::Screen::Height();

                if (_currentBgWidth < _currentBgHeight) {
                    _currentBgHeight = height;
                    _currentBgWidth = static_cast<int>(_currentBgHeight * ratio);
                } else {
                    _currentBgWidth = width;
                    _currentBgHeight = static_cast<int>(_currentBgWidth / ratio);
                }

                il2cpp_field_set_value(bgManager, _currentBgWidthField, &_currentBgWidth);
                il2cpp_field_set_value(bgManager, _currentBgHeightField, &_currentBgHeight);

                il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *)>(bgManager->klass,
                                                                             "RecalcBgSize",
                                                                             0)(bgManager);
            }
        }
    }

    void ResizeMiniDirector() {
        Il2CppArraySize_t<Il2CppObject *> *miniDirectors;
        miniDirectors = UnityEngine::Object::FindObjectsByType(
                GetRuntimeType("umamusume.dll", "Gallop", "MiniDirector"),
                UnityEngine::FindObjectsInactive::Include, UnityEngine::FindObjectsSortMode::None);

        if (miniDirectors) {
            for (int i = 0; i < miniDirectors->max_length; i++) {
                auto obj = miniDirectors->vector[i];

                if (obj) {
                    auto state = il2cpp_symbols::get_method_pointer<int (*)(Il2CppObject *)>(
                            obj->klass, "get_State", 0)(obj);

                    if (state > 0) {
                        auto DirectorUI = il2cpp_symbols::get_method_pointer<Il2CppObject *(*)(
                                Il2CppObject *)>(obj->klass, "get_DirectorUI", 0)(
                                obj);
                        auto cameraController = il2cpp_symbols::get_method_pointer<Il2CppObject *(*)(
                                Il2CppObject *)>(obj->klass, "get_CameraController",
                                                 0)(obj);

                        if (DirectorUI && cameraController) {
                            il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *)>(
                                    DirectorUI->klass, "ResetTextureSize", 0)(
                                    DirectorUI);

                            auto TextureResolution = il2cpp_symbols::get_method_pointer<UnityEngine::Vector2Int(*)(
                                    Il2CppObject *)>(DirectorUI->klass, "get_TextureResolution",
                                                     0)(DirectorUI);

                            auto _cameraField = il2cpp_class_get_field_from_name(
                                    cameraController->klass, "_camera");
                            Il2CppObject *_camera;
                            il2cpp_field_get_value(cameraController, _cameraField, &_camera);

                            if (_camera) {
                                il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *,
                                                                            UnityEngine::Vector2Int)>(
                                        cameraController->klass, "ResizeRenderTexture",
                                        1)(cameraController, TextureResolution);

                                auto _renderTextureField = il2cpp_class_get_field_from_name(
                                        cameraController->klass, "_renderTexture");
                                Il2CppObject *_renderTexture;
                                il2cpp_field_get_value(cameraController, _renderTextureField,
                                                       &_renderTexture);

                                il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *,
                                                                            Il2CppObject *)>(
                                        DirectorUI->klass, "SetRenderTexture", 1)(
                                        DirectorUI, _renderTexture);
                            }
                        }
                    }
                }
            }
        }
    }


    Il2CppObject *delayTweener;

    void RemakeTextures() {
        auto uiManager = Gallop::UIManager::Instance();

        auto graphicSettings = Gallop::GraphicSettings::Instance();
        if (!graphicSettings) {
            return;
        }

        graphicSettings.Update3DRenderTexture();

        Il2CppArraySize_t<Il2CppObject *> *renders;
        renders = UnityEngine::Object::FindObjectsByType(
                GetRuntimeType("umamusume.dll", "Gallop", "CutInImageEffectPostRender"),
                UnityEngine::FindObjectsInactive::Include, UnityEngine::FindObjectsSortMode::None);

        if (renders) {
            for (int i = 0; i < renders->max_length; i++) {
                auto obj = renders->vector[i];

                if (obj) {
                    auto buffer = il2cpp_symbols::get_method_pointer<Il2CppObject *(*)(
                            Il2CppObject *)>(obj->klass, "get_FrameBuffer", 0)(obj);
                    if (buffer) {
                        if (il2cpp_symbols::get_method_pointer<Il2CppObject *(*)(
                                Il2CppObject *)>(buffer->klass, "get_ColorBuffer",
                                                 0)(buffer)) {
                            il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *)>(
                                    buffer->klass, "RemakeRenderTexture", 0)(buffer);
                        }
                    }
                }
            }
        }

        Il2CppArraySize_t<Il2CppObject *> *cuts;
        cuts = UnityEngine::Object::FindObjectsByType(
                GetRuntimeType("umamusume.dll", "Gallop", "LimitBreakCut"),
                UnityEngine::FindObjectsInactive::Include, UnityEngine::FindObjectsSortMode::None);

        if (cuts) {
            for (int i = 0; i < cuts->max_length; i++) {
                auto obj = cuts->vector[i];

                if (obj) {
                    auto _frameBufferField = il2cpp_class_get_field_from_name(obj->klass,
                                                                              "_frameBuffer");
                    Il2CppObject *_frameBuffer;
                    il2cpp_field_get_value(obj, _frameBufferField, &_frameBuffer);

                    if (_frameBuffer) {
                        if (il2cpp_symbols::get_method_pointer<Il2CppObject *(*)(
                                Il2CppObject *)>(_frameBuffer->klass, "get_ColorBuffer",
                                                 0)(_frameBuffer)) {
                            il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *)>(
                                    _frameBuffer->klass, "RemakeRenderTexture", 0)(
                                    _frameBuffer);
                        }
                    }
                }
            }
        }

        Il2CppArraySize_t<Il2CppObject *> *raceEffect;
        raceEffect = UnityEngine::Object::FindObjectsByType(
                GetRuntimeType("umamusume.dll", "Gallop", "RaceImageEffect"),
                UnityEngine::FindObjectsInactive::Include, UnityEngine::FindObjectsSortMode::None);

        if (raceEffect) {
            for (int i = 0; i < raceEffect->max_length; i++) {
                auto obj = raceEffect->vector[i];

                if (obj) {
                    auto get_FrameBuffer = il2cpp_symbols::get_method_pointer<Il2CppObject *(*)(
                            Il2CppObject *)>(obj->klass, "get_FrameBuffer", 0);
                    if (get_FrameBuffer) {
                        auto buffer = get_FrameBuffer(obj);
                        if (buffer) {
                            auto _drawPassField = il2cpp_class_get_field_from_name(buffer->klass,
                                                                                   "_drawPass");
                            uint64_t *_drawPass;
                            il2cpp_field_get_value(buffer, _drawPassField, &_drawPass);

                            if (!_drawPass) {
                                uint64_t defPass = 0;
                                il2cpp_field_set_value(buffer, _drawPassField, &defPass);
                            }


                            if (il2cpp_symbols::get_method_pointer<Il2CppObject *(*)(
                                    Il2CppObject *)>(buffer->klass, "get_ColorBuffer",
                                                     0)(buffer)) {
                                il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *)>(
                                        buffer->klass, "RemakeRenderTexture", 0)(
                                        buffer);
                            }
                        }
                    } else {
                        break;
                    }
                }
            }
        }

        /*Il2CppArraySize_t<Il2CppObject*>* storyEffect;
        storyEffect = UnityEngine::Object::FindObjectsByType(
            GetRuntimeType("umamusume.dll", "Gallop", "StoryImageEffect"), UnityEngine::FindObjectsInactive::Include, UnityEngine::FindObjectsSortMode::None);

        if (storyEffect)
        {
            for (int i = 0; i < storyEffect->max_length; i++)
            {
                auto obj = storyEffect->vector[i];

                if (obj)
                {
                    auto buffer = il2cpp_symbols::get_method_pointer<Il2CppObject * (*)(Il2CppObject*)>(obj->klass, "get_FrameBuffer", 0)(obj);
                    if (buffer)
                    {
                        il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject*)>(buffer->klass, "RemakeRenderTexture", 0)(buffer);
                    }
                }
            }
        }*/

        Il2CppArraySize_t<Il2CppObject *> *lowResCameras;
        lowResCameras = UnityEngine::Object::FindObjectsByType(
                GetRuntimeType("umamusume.dll", "Gallop", "LowResolutionCameraBase"),
                UnityEngine::FindObjectsInactive::Include, UnityEngine::FindObjectsSortMode::None);

        if (lowResCameras) {
            for (int i = 0; i < lowResCameras->max_length; i++) {
                auto obj = lowResCameras->vector[i];

                if (obj) {
                    auto method = il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *)>(
                            obj->klass, "ResizeRenderTextureSize", 0);
                    if (method) {
                        method(obj);
                    }
                }
            }
        }

        Il2CppArraySize_t<Il2CppObject *> *liveTheaterCharaSelects;
        liveTheaterCharaSelects = UnityEngine::Object::FindObjectsByType(
                GetRuntimeType("umamusume.dll", "Gallop", "LiveTheaterCharaSelect"),
                UnityEngine::FindObjectsInactive::Include, UnityEngine::FindObjectsSortMode::None);

        if (liveTheaterCharaSelects) {
            for (int i = 0; i < liveTheaterCharaSelects->max_length; i++) {
                auto obj = liveTheaterCharaSelects->vector[i];

                if (obj) {
                    auto _sceneField = il2cpp_class_get_field_from_name(obj->klass, "_scene");
                    Il2CppObject *_scene;
                    il2cpp_field_get_value(obj, _sceneField, &_scene);

                    if (_scene) {
                        auto camera = il2cpp_symbols::get_method_pointer<Il2CppObject *(*)(
                                Il2CppObject *)>(_scene->klass, "GetCamera", 0)(
                                _scene);
                        auto texture = il2cpp_symbols::get_method_pointer<Il2CppObject *(*)(
                                Il2CppObject *)>(camera->klass, "get_RenderTexture",
                                                 0)(camera);

                        auto _formationAllField = il2cpp_class_get_field_from_name(obj->klass,
                                                                                   "_formationAll");
                        Il2CppObject *_formationAll;
                        il2cpp_field_get_value(obj, _formationAllField, &_formationAll);

                        if (_formationAll) {
                            il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *,
                                                                        Il2CppObject *)>(
                                    _formationAll->klass, "SetRenderTex", 1)(
                                    _formationAll, texture);
                        }

                        auto _formationMainField = il2cpp_class_get_field_from_name(obj->klass,
                                                                                    "_formationMain");
                        Il2CppObject *_formationMain;
                        il2cpp_field_get_value(obj, _formationMainField, &_formationMain);

                        if (_formationMain) {
                            il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *,
                                                                        Il2CppObject *)>(
                                    _formationMain->klass, "SetRenderTex", 1)(
                                    _formationMain, texture);
                        }

                        // TODO: reposition
                    }
                }
            }
        }

        Il2CppArraySize_t<Il2CppObject *> *miniDirectors;
        miniDirectors = UnityEngine::Object::FindObjectsByType(
                GetRuntimeType("umamusume.dll", "Gallop", "MiniDirector"),
                UnityEngine::FindObjectsInactive::Include, UnityEngine::FindObjectsSortMode::None);

        if (miniDirectors && miniDirectors->max_length) {

            if (delayTweener) {
                il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *, bool)>("DOTween.dll",
                                                                                   "DG.Tweening",
                                                                                   "TweenExtensions",
                                                                                   "Complete", 2)(
                        delayTweener, true);
            }

            auto callback = CreateDelegateWithClass(
                    il2cpp_symbols::get_class("DOTween.dll", "DG.Tweening", "TweenCallback"),
                    uiManager, *([](Il2CppObject *self) {
                        ResizeMiniDirector();
                        delayTweener = nullptr;
                    }));

            // Delay 50ms
            delayTweener = il2cpp_symbols::get_method_pointer<Il2CppObject *(*)(float,
                                                                                Il2CppDelegate *,
                                                                                bool)>(
                    "DOTween.dll", "DG.Tweening", "DOVirtual", "DelayedCall", 3)(0.05,
                                                                                 &callback->delegate,
                                                                                 true);
        }

        auto controller = Gallop::SceneManager::Instance().GetCurrentViewController();

        if (controller) {
            if (controller->klass->name == "SingleModeMainViewController"s) {
                auto ScenarioController = il2cpp_symbols::get_method_pointer<Il2CppObject *(*)(
                        Il2CppObject *)>(controller->klass, "get_ScenarioController",
                                         0)(controller);

                if (ScenarioController && ScenarioController->klass->name ==
                                          "SingleModeMainViewScenarioBreedersController"s) {
                    auto IsStoryActive = il2cpp_symbols::get_method_pointer<bool (*)(
                            Il2CppObject *)>(controller->klass, "get_IsStoryActive", 0)(controller);

                    if (!IsStoryActive) {
                        auto trainingController = il2cpp_symbols::get_method_pointer<Il2CppObject *(*)(
                                Il2CppObject *)>(controller->klass, "get_TrainingController",
                                                 0)(controller);
                        if (!il2cpp_symbols::get_method_pointer<bool (*)(Il2CppObject *)>(
                                trainingController->klass, "get_IsInTraining", 0)(
                                trainingController)) {
                            il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *)>(
                                    ScenarioController->klass, "PlayCutIn", 0)(
                                    ScenarioController);
                        }
                    }
                }
            }

            if (controller->klass->name == "PhotoStudioViewController"s) {
                auto _photoStudioTopCharaViewerField = il2cpp_class_get_field_from_name(
                        controller->klass, "_photoStudioTopCharaViewer");
                Il2CppObject *_photoStudioTopCharaViewer;
                il2cpp_field_get_value(controller, _photoStudioTopCharaViewerField,
                                       &_photoStudioTopCharaViewer);

                if (_photoStudioTopCharaViewer) {
                    auto _lowResolutionCameraField = il2cpp_class_get_field_from_name(
                            _photoStudioTopCharaViewer->klass, "_lowResolutionCamera");
                    Il2CppObject *_lowResolutionCamera;
                    il2cpp_field_get_value(_photoStudioTopCharaViewer, _lowResolutionCameraField,
                                           &_lowResolutionCamera);

                    if (_lowResolutionCamera) {
                        il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *,
                                                                    Il2CppObject *)>(
                                _photoStudioTopCharaViewer->klass, "OnCreateTexture",
                                1)(_photoStudioTopCharaViewer, _lowResolutionCamera);
                    }
                }
            }

            if (controller->klass->name == "FanRaidViewController"s) {
                auto _fanRaidTopSequenceField = il2cpp_class_get_field_from_name(controller->klass,
                                                                                 "_fanRaidTopSequence");
                Il2CppObject *_fanRaidTopSequence;
                il2cpp_field_get_value(controller, _fanRaidTopSequenceField, &_fanRaidTopSequence);

                if (_fanRaidTopSequence) {
                    auto _frameBufferField = il2cpp_class_get_field_from_name(
                            _fanRaidTopSequence->klass, "_frameBuffer");
                    Il2CppObject *_frameBuffer;
                    il2cpp_field_get_value(_fanRaidTopSequence, _frameBufferField, &_frameBuffer);

                    if (_frameBuffer) {
                        if (il2cpp_symbols::get_method_pointer<Il2CppObject *(*)(
                                Il2CppObject *)>(_frameBuffer->klass, "get_ColorBuffer",
                                                 0)(_frameBuffer)) {
                            il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *)>(
                                    _frameBuffer->klass, "RemakeRenderTexture", 0)(
                                    _frameBuffer);
                        }
                    }
                }
            }

            if (controller->klass->name == "GachaMainViewController"s) {
                auto _contextField = il2cpp_class_get_field_from_name(controller->klass,
                                                                      "_context");
                Il2CppObject *_context;
                il2cpp_field_get_value(controller, _contextField, &_context);

                if (_context) {
                    auto FrameBufferField = il2cpp_class_get_field_from_name(_context->klass,
                                                                             "FrameBuffer");
                    Il2CppObject *FrameBuffer;
                    il2cpp_field_get_value(_context, FrameBufferField, &FrameBuffer);

                    if (FrameBuffer) {
                        if (il2cpp_symbols::get_method_pointer<Il2CppObject *(*)(
                                Il2CppObject *)>(FrameBuffer->klass, "get_ColorBuffer",
                                                 0)(FrameBuffer)) {
                            il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *)>(
                                    FrameBuffer->klass, "RemakeRenderTexture", 0)(
                                    FrameBuffer);
                        }
                    }
                }
            }

            if (controller->klass->name == "SingleModeSuccessionCutViewController"s ||
                controller->klass->name == "EpisodeMainUnlockRaceCutinViewController"s ||
                controller->klass->name == "SingleModeSuccessionEventViewController"s) {
                auto _resultField = il2cpp_class_get_field_from_name(controller->klass, "_result");
                Il2CppObject *_result;
                il2cpp_field_get_value(controller, _resultField, &_result);

                if (_result) {
                    auto _resultCameraField = il2cpp_class_get_field_from_name(_result->klass,
                                                                               "_resultCamera");
                    Il2CppObject *_resultCamera;
                    il2cpp_field_get_value(_result, _resultCameraField, &_resultCamera);

                    if (_resultCamera) {
                        auto texture = uiManager.UITexture();
                        il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *,
                                                                    Il2CppObject *)>(
                                _resultCamera->klass, "set_targetTexture", 1)(
                                _resultCamera, texture);
                    }
                }
            }

            if (string(controller->klass->name).ends_with("PaddockViewController")) {
                auto _frameBufferField = il2cpp_class_get_field_from_name(controller->klass,
                                                                          "_frameBuffer");
                Il2CppObject *_frameBuffer;
                il2cpp_field_get_value(controller, _frameBufferField, &_frameBuffer);

                if (_frameBuffer) {
                    if (il2cpp_symbols::get_method_pointer<Il2CppObject *(*)(Il2CppObject *)>(
                            _frameBuffer->klass, "get_ColorBuffer", 0)(
                            _frameBuffer)) {
                        il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *)>(
                                _frameBuffer->klass, "RemakeRenderTexture", 0)(
                                _frameBuffer);
                    }
                }
            }

            if (config::freeform_window) {
                if (string(controller->klass->name).ends_with("LiveViewController")) {
                    auto view = il2cpp_symbols::get_method_pointer<Il2CppObject *(*)(
                            Il2CppObject *)>(controller->klass, "GetViewBase", 0)(
                            controller);
                    auto _fullPortraitRootField = il2cpp_class_get_field_from_name(view->klass,
                                                                                   "_fullPortraitRoot");

                    if (_fullPortraitRootField) {
                        Il2CppObject *_fullPortraitRoot;
                        il2cpp_field_get_value(view, _fullPortraitRootField, &_fullPortraitRoot);

                        if (_fullPortraitRoot) {
                            UnityEngine::GameObject(_fullPortraitRoot).SetActive(false);
                        }
                    }
                }
            }
        }

        auto storyManager = GetSingletonInstance(
                il2cpp_symbols::get_class("umamusume.dll", "Gallop", "StoryManager"));
        if (storyManager) {
            auto storySceneController = il2cpp_symbols::get_method_pointer<Il2CppObject *(*)()>(
                    "umamusume.dll", "Gallop", "StoryManager", "get_StorySceneController",
                    IgnoreNumberOfArguments)();
            if (storySceneController) {
                auto DisplayMode = il2cpp_symbols::get_method_pointer<uint64_t(*)(
                        Il2CppObject *)>(storySceneController->klass, "get_DisplayMode",
                                         0)(storySceneController);

                Gallop::StoryViewController storyViewController = il2cpp_symbols::get_method_pointer<Il2CppObject *(*)(
                        Il2CppObject *)>("umamusume.dll", "Gallop", "StoryManager",
                                         "get_ViewController", 0)(storyManager);

                auto IsSingleModeOrGallery = storyViewController.IsSingleModeOrGallery();

                auto scene = il2cpp_symbols::get_method_pointer<Il2CppObject *(*)(
                        Il2CppObject *)>(storySceneController->klass, "GetSceneBase",
                                         0)(storySceneController);

                if (!IsSingleModeOrGallery) {
                    storyViewController.SetDisplayMode(DisplayMode);
                } else {
                    if (DisplayMode == 1) {
                        il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *)>(
                                storySceneController->klass, "SetDisplayAreaPortrait",
                                0)(storySceneController);
                    } else {
                        il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *)>(
                                storySceneController->klass, "SetDisplayAreaFullScreen",
                                0)(storySceneController);
                    }

                    uint64_t drawDirection = 6;

                    if (DisplayMode == 0 || DisplayMode == 3) {
                        drawDirection = 0;
                    } else if (DisplayMode == 1) {
                        drawDirection = 7;
                    }

                    auto _lowResolutionCameraListField = il2cpp_class_get_field_from_name(
                            storySceneController->klass, "_lowResolutionCameraList");
                    Il2CppObject *_lowResolutionCameraList;
                    il2cpp_field_get_value(storySceneController, _lowResolutionCameraListField,
                                           &_lowResolutionCameraList);

                    if (_lowResolutionCameraList) {
                        FieldInfo *_itemsField = il2cpp_class_get_field_from_name(
                                _lowResolutionCameraList->klass, "_items");
                        Il2CppArraySize_t<Il2CppObject *> *_items;
                        il2cpp_field_get_value(_lowResolutionCameraList, _itemsField, &_items);

                        for (int i = 0; i < _items->max_length; i++) {
                            auto lowResolutionCamera = _items->vector[i];

                            if (lowResolutionCamera) {
                                il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *,
                                                                            uint64_t)>(
                                        lowResolutionCamera->klass, "ChangeDirection",
                                        1)(lowResolutionCamera, drawDirection);
                            }
                        }
                    }

                    auto FrameBufferDisplayMode = il2cpp_symbols::get_method_pointer<uint64_t(*)(
                            uint64_t)>("umamusume.dll", "Gallop", "LowResolutionCameraUtil",
                                       "GetDrawPass", 1)(DisplayMode);

                    auto FrameBuffer = il2cpp_symbols::get_method_pointer<Il2CppObject *(*)(
                            Il2CppObject *)>(storySceneController->klass, "get_FrameBuffer",
                                             0)(storySceneController);

                    if (il2cpp_symbols::get_method_pointer<Il2CppObject *(*)(Il2CppObject *)>(
                            FrameBuffer->klass, "get_ColorBuffer", 0)(FrameBuffer)) {
                        il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *, uint64_t)>(
                                FrameBuffer->klass, "RemakeRenderTexture", 1)(
                                FrameBuffer, FrameBufferDisplayMode);
                    }

                    // il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject*, uint64_t)>(storySceneController->klass, "UpdateFovFactor", 1)(storySceneController, DisplayMode);

                    auto FullScreenImageRendererField = il2cpp_class_get_field_from_name(
                            scene->klass, "FullScreenImageRenderer");
                    Il2CppObject *FullScreenImageRenderer;
                    il2cpp_field_get_value(scene, FullScreenImageRendererField,
                                           &FullScreenImageRenderer);

                    il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *)>(
                            FullScreenImageRenderer->klass, "ForceRender", 0)(
                            FullScreenImageRenderer);
                }

                storyViewController.SetupUIOnChangeOrientation();
            }
        }
    }

    void ResizeMoviePlayer() {
        auto movieManager = il2cpp_symbols::get_method_pointer<Il2CppObject *(*)()>(
                "Cute.Cri.Assembly.dll", "Cute.Cri", "MovieManager", "get_Instance",
                IgnoreNumberOfArguments)();

        if (movieManager) {
            auto playerDicField = il2cpp_class_get_field_from_name(movieManager->klass,
                                                                   "playerDic");
            Il2CppObject *playerDic;
            il2cpp_field_get_value(movieManager, playerDicField, &playerDic);

            if (playerDic) {
                auto entriesField = il2cpp_class_get_field_from_name(playerDic->klass, "_entries");
                if (!entriesField) {
                    entriesField = il2cpp_class_get_field_from_name(playerDic->klass, "entries");
                }

                Il2CppArraySize_t<System::Collections::Generic::Dictionary<Cute::Cri::MoviePlayerHandle, Il2CppObject *>::Entry> *entries;
                il2cpp_field_get_value(playerDic, entriesField, &entries);

                if (entries) {
                    for (int i = 0; i < entries->max_length; i++) {
                        auto entry = entries->vector[i];

                        auto player = entry.value;

                        if (player) {
                            auto gameObject = il2cpp_symbols::get_method_pointer<Il2CppObject *(*)(
                                    Il2CppObject *)>(player->klass, "get_gameObject",
                                                     0)(player);

                            if (gameObject) {
                                auto transform = il2cpp_symbols::get_method_pointer<Il2CppObject *(*)(
                                        Il2CppObject *)>(gameObject->klass, "get_transform",
                                                         0)(gameObject);

                                if (transform) {
                                    auto parent = il2cpp_symbols::get_method_pointer<Il2CppObject *(*)(
                                            Il2CppObject *)>(transform->klass, "get_parent",
                                                             0)(transform);

                                    if (parent) {
                                        auto parentGameObject = il2cpp_symbols::get_method_pointer<Il2CppObject *(*)(
                                                Il2CppObject *)>(parent->klass, "get_gameObject",
                                                                 0)(parent);
                                        auto getComponents = il2cpp_symbols::get_method_pointer<Il2CppArraySize *(*)(
                                                Il2CppObject *, Il2CppType *, bool, bool, bool,
                                                bool, Il2CppObject *)>(parentGameObject->klass,
                                                                       "GetComponentsInternal",
                                                                       6);

                                        if (UnityEngine::Object::Name(parent)->chars ==
                                            il2cppstring(IL2CPP_STRING("MainCanvas"))) {
                                            if (auto klass = il2cpp_symbols::get_class(
                                                    "umamusume.dll", "Gallop", "StoryMovieView")) {
                                                auto array1 = getComponents(parentGameObject,
                                                                            reinterpret_cast<Il2CppType *>(GetRuntimeType(
                                                                                    klass)), true,
                                                                            true, false, false,
                                                                            nullptr);

                                                if (array1) {
                                                    if (array1->max_length > 0) {
                                                        auto fullPlayer = il2cpp_object_new(
                                                                il2cpp_symbols::get_class(
                                                                        "umamusume.dll", "Gallop",
                                                                        "StoryFullMoviePlayer"));
                                                        auto _handleField = il2cpp_class_get_field_from_name(
                                                                fullPlayer->klass, "_handle");
                                                        il2cpp_field_set_value(fullPlayer,
                                                                               _handleField,
                                                                               &entry.key);

                                                        il2cpp_symbols::get_method_pointer<void (*)(
                                                                Il2CppObject *, int)>(
                                                                fullPlayer->klass,
                                                                "AdjustMovieSize",
                                                                1)(fullPlayer,
                                                                   Gallop::Screen::IsVertical()
                                                                   ? 0 : 1);

                                                        return;
                                                    }
                                                }
                                            }

                                            auto array2 = getComponents(parentGameObject,
                                                                        reinterpret_cast<Il2CppType *>(GetRuntimeType(
                                                                                "umamusume.dll",
                                                                                "Gallop",
                                                                                "StoryView")), true,
                                                                        true, false, false,
                                                                        nullptr);

                                            if (array2) {
                                                if (array2->max_length > 0) {
                                                    auto controller = Gallop::SceneManager::Instance().GetCurrentViewController();

                                                    auto _wipeControllerField = il2cpp_class_get_field_from_name(
                                                            controller->klass, "_wipeController");
                                                    Il2CppObject *_wipeController;
                                                    il2cpp_field_get_value(controller,
                                                                           _wipeControllerField,
                                                                           &_wipeController);

                                                    if (_wipeController) {
                                                        auto _moviePlayerField = il2cpp_class_get_field_from_name(
                                                                _wipeController->klass,
                                                                "_moviePlayer");
                                                        Il2CppObject *_moviePlayer;
                                                        il2cpp_field_get_value(_wipeController,
                                                                               _moviePlayerField,
                                                                               &_moviePlayer);

                                                        if (_moviePlayer) {
                                                            auto StoryTimelineController = il2cpp_symbols::get_class(
                                                                    "umamusume.dll", "Gallop",
                                                                    "StoryTimelineController");
                                                            auto CurrentDisplayModeField = il2cpp_class_get_field_from_name(
                                                                    StoryTimelineController->klass,
                                                                    "CurrentDisplayMode");
                                                            int CurrentDisplayMode;
                                                            il2cpp_field_static_get_value(
                                                                    CurrentDisplayModeField,
                                                                    &CurrentDisplayMode);

                                                            if (CurrentDisplayMode == 3 &&
                                                                !Gallop::Screen::IsVertical()) {
                                                                int tmpMode = 2;
                                                                il2cpp_field_static_get_value(
                                                                        CurrentDisplayModeField,
                                                                        &tmpMode);
                                                            }

                                                            il2cpp_symbols::get_method_pointer<void (*)(
                                                                    Il2CppObject *)>(
                                                                    _moviePlayer->klass,
                                                                    "AdjustScreenSize",
                                                                    0)(_moviePlayer);

                                                            il2cpp_field_static_set_value(
                                                                    CurrentDisplayModeField,
                                                                    &CurrentDisplayMode);
                                                        }
                                                    }
                                                    return;
                                                }
                                            }

                                            auto newSize = il2cpp_symbols::get_method_pointer<UnityEngine::Vector2(*)()>(
                                                    "umamusume.dll", "Gallop",
                                                    "MovieScreenSizeHelper",
                                                    "GetMovieTargetCanvasSize",
                                                    IgnoreNumberOfArguments)();

                                            auto criPlayer = il2cpp_symbols::get_method_pointer<Il2CppObject *(*)(
                                                    Il2CppObject *)>(player->klass, "get_Player",
                                                                     0)(player);
                                            if (criPlayer) {
                                                auto status = il2cpp_symbols::get_method_pointer<int (*)(
                                                        Il2CppObject *)>(criPlayer->klass,
                                                                         "get_status",
                                                                         0)(
                                                        criPlayer);
                                                if (status == 5) {
                                                    Cute::Cri::MoviePlayerForUI(
                                                            player).AdjustScreenSize(newSize, true);
                                                }
                                            }

                                        } else if (parent->klass->name == "RectTransform"s) {
                                            auto parentGameObject = UnityEngine::RectTransform(
                                                    parent).gameObject();
                                            auto array = parentGameObject.GetComponentsInChildren(
                                                    GetRuntimeType("umamusume.dll", "Gallop",
                                                                   "PartsEpisodeList"), false);

                                            if (array) {
                                                for (int j = 0; j < array->max_length; j++) {
                                                    auto obj =
                                                            il2cpp_symbols::get_method_pointer<Il2CppObject *(*)(
                                                                    Il2CppObject *, long index)>(
                                                                    "mscorlib.dll", "System",
                                                                    "Array", "GetValue", 1)(array,
                                                                                            j);
                                                    if (!obj) continue;

                                                    auto newSize = il2cpp_symbols::get_method_pointer<UnityEngine::Vector2(*)(
                                                            Il2CppObject *)>(obj->klass,
                                                                             "CalcMovieRectSize",
                                                                             0)(obj);

                                                    il2cpp_symbols::get_method_pointer<void (*)(
                                                            Il2CppObject *, UnityEngine::Vector2)>(
                                                            parent->klass, "set_sizeDelta",
                                                            1)(parent, newSize);

                                                    auto criPlayer = il2cpp_symbols::get_method_pointer<Il2CppObject *(*)(
                                                            Il2CppObject *)>(player->klass,
                                                                             "get_Player",
                                                                             0)(
                                                            player);

                                                    if (criPlayer) {
                                                        auto status = il2cpp_symbols::get_method_pointer<int (*)(
                                                                Il2CppObject *)>(criPlayer->klass,
                                                                                 "get_status",
                                                                                 0)(
                                                                criPlayer);
                                                        if (status == 5) {
                                                            Cute::Cri::MoviePlayerForUI(
                                                                    player).AdjustScreenSize(
                                                                    newSize, true);
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    void WaitForEndOfFrame(void (*fn)()) {
        try {
            auto gameSystem = Gallop::GameSystem::Instance();
            il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *, Il2CppDelegate *)>(
                    "umamusume.dll", "Gallop", "MonoBehaviourExtension", "WaitForEndFrame", 2)(
                    gameSystem, CreateDelegateStatic(fn));
        }
        catch (const Il2CppExceptionWrapper &ex) {
            LOGW("WaitForEndOfFrame error: %s", il2cpp_u8(ex.ex->message->chars).data());
            PrintStackTrace();
        }
    }

    void WaitForEndOfFrame(Il2CppObject *target, void (*fn)(Il2CppObject *self)) {
        try {
            auto delegate = &CreateUnityAction(target, fn)->delegate;
            auto gameSystem = Gallop::GameSystem::Instance();
            il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *, Il2CppDelegate *)>(
                    "umamusume.dll", "Gallop", "MonoBehaviourExtension", "WaitForEndFrame", 2)(
                    gameSystem, delegate);
        }
        catch (const Il2CppExceptionWrapper &ex) {
            LOGW("WaitForEndOfFrame error: %s", il2cpp_u8(ex.ex->message->chars).data());
            PrintStackTrace();
        }
    }

    void ResizeWindow(int _updateWidth, int _updateHeight) {
        if (_updateWidth < 72 || _updateHeight < 72) {
            return;
        }

        static int updateWidth;
        static int updateHeight;

        updateWidth = _updateWidth;
        updateHeight = _updateHeight;

        WaitForEndOfFrame(*[]() {
            auto refreshRate = UnityEngine::RefreshRate{0, 0};
            UnityEngine::Screen::SetResolution_Injected(updateWidth, updateHeight,
                                                        UnityEngine::FullScreenMode::FullScreenWindow,
                                                        &refreshRate);

            WaitForEndOfFrame(*[]() {
                const auto contentWidth = UnityEngine::Screen::width();
                const auto contentHeight = UnityEngine::Screen::height();

                auto ratio = static_cast<float>(contentWidth) / static_cast<float>(contentHeight);

                auto lastWidth = updateWidth;
                auto lastHeight = updateHeight;

                const auto _aspectRatio = contentWidth / contentHeight;

                const auto isPortrait = contentWidth < contentHeight;

                const auto unityWidth = UnityEngine::Screen::width();
                const auto unityHeight = UnityEngine::Screen::height();

                const auto isUnityPortrait = unityWidth < unityHeight;

                Gallop::Screen::OriginalScreenWidth(isUnityPortrait ? contentHeight : contentWidth);
                Gallop::Screen::OriginalScreenHeight(
                        isUnityPortrait ? contentWidth : contentHeight);

                auto tapEffectController = GetSingletonInstance(
                        il2cpp_symbols::get_class("umamusume.dll", "Gallop",
                                                  "TapEffectController"));

                auto uiManager = Gallop::UIManager::Instance();

                if (uiManager) {
                    il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *)>(
                            tapEffectController->klass, "Disable", 0)(
                            tapEffectController);

                    uiManager.SetCameraSizeByOrientation(UnityEngine::ScreenOrientation::Portrait);
                }

                auto anRootManager = AnimateToUnity::AnRootManager::Instance();

                if (anRootManager) {
                    anRootManager.ScreenRate(_aspectRatio);
                }

                if (uiManager) {
                    auto gameObject = uiManager.gameObject();

                    auto transform = il2cpp_symbols::get_method_pointer<Il2CppObject *(*)(
                            Il2CppObject *)>(gameObject, "get_transform", 0)(
                            gameObject);

                    il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *,
                                                                UnityEngine::Vector3)>(
                            transform->klass, "set_localScale", 1)(transform,
                                                                   UnityEngine::Vector3{
                                                                           1, 1, 1});

                    if (tapEffectController) {
                        il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *)>(
                                tapEffectController->klass, "Enable", 0)(
                                tapEffectController);
                    }

                    Il2CppArraySize_t<Il2CppObject *> *canvasScalerList;
                    canvasScalerList = UnityEngine::Object::FindObjectsByType(
                            GetRuntimeType("UnityEngine.UI.dll", "UnityEngine.UI", "CanvasScaler"),
                            UnityEngine::FindObjectsInactive::Include,
                            UnityEngine::FindObjectsSortMode::None);

                    for (int i = 0; i < canvasScalerList->max_length; i++) {
                        auto canvasScaler = canvasScalerList->vector[i];
                        if (canvasScaler) {
                            auto gameObject = il2cpp_symbols::get_method_pointer<Il2CppObject *(*)(
                                    Il2CppObject *)>(canvasScaler->klass, "get_gameObject",
                                                     0)(canvasScaler);

                            const auto keepActive = il2cpp_symbols::get_method_pointer<bool (*)(
                                    Il2CppObject *)>(gameObject->klass, "get_activeSelf",
                                                     0)(gameObject);

                            il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *, bool)>(
                                    gameObject->klass, "SetActive", 1)(gameObject,
                                                                       true);

                            il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *, bool)>(
                                    gameObject->klass, "SetActive", 1)(gameObject,
                                                                       keepActive);

                            auto scaleMode = il2cpp_symbols::get_method_pointer<int (*)(
                                    Il2CppObject *)>(canvasScaler->klass, "get_uiScaleMode",
                                                     0)(canvasScaler);

                            if (scaleMode == 1) {
                                if (isPortrait) {
                                    const auto scale = min(config::freeform_ui_scale_portrait,
                                                           max(1.0f,
                                                               static_cast<float>(contentHeight) *
                                                               config::runtime::ratioVertical) *
                                                           config::freeform_ui_scale_portrait);
                                    il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *,
                                                                                UnityEngine::Vector2)>(
                                            canvasScaler->klass, "set_referenceResolution",
                                            1)(canvasScaler, UnityEngine::Vector2{
                                            static_cast<float>(contentWidth / scale),
                                            static_cast<float>(contentHeight / scale)});
                                } else {
                                    const auto scale = min(config::freeform_ui_scale_landscape,
                                                           max(1.0f,
                                                               static_cast<float>(contentWidth) /
                                                               config::runtime::ratioHorizontal) *
                                                           config::freeform_ui_scale_landscape);
                                    il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *,
                                                                                UnityEngine::Vector2)>(
                                            canvasScaler->klass, "set_referenceResolution",
                                            1)(canvasScaler, UnityEngine::Vector2{
                                            static_cast<float>(contentWidth / scale),
                                            static_cast<float>(contentHeight / scale)});
                                }
                            }

                            if (scaleMode == 0) {
                                if (isPortrait) {
                                    const auto scale = min(config::freeform_ui_scale_portrait,
                                                           max(1.0f,
                                                               static_cast<float>(contentHeight) *
                                                               config::runtime::ratioVertical) *
                                                           config::freeform_ui_scale_portrait);
                                    il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *,
                                                                                float)>(
                                            canvasScaler->klass, "set_scaleFactor",
                                            1)(canvasScaler, scale);
                                } else {
                                    const auto scale = min(config::freeform_ui_scale_landscape,
                                                           max(1.0f,
                                                               static_cast<float>(contentWidth) /
                                                               config::runtime::ratioHorizontal) *
                                                           config::freeform_ui_scale_landscape);
                                    il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *,
                                                                                float)>(
                                            canvasScaler->klass, "set_scaleFactor",
                                            1)(canvasScaler, scale);
                                }
                            }
                        }
                    }

                    SetBGCanvasScalerSize();
                }

                static int _contentWidth;
                static int _contentHeight;
                _contentWidth = contentWidth;
                _contentHeight = contentHeight;

                WaitForEndOfFrame(*[]() {
                    auto tapEffectController = GetSingletonInstance(
                            il2cpp_symbols::get_class("umamusume.dll", "Gallop",
                                                      "TapEffectController"));

                    const auto isPortrait = _contentWidth < _contentHeight;

                    auto uiManager = Gallop::UIManager::Instance();

                    if (uiManager) {
                        // uiManager.SetupSafeArea();
                        uiManager.AdjustSafeArea();
                        auto _bgManager = uiManager._bgManager();
                        if (_bgManager) {
                            _bgManager.OnChangeResolutionByGraphicsSettings();
                        }

                        uiManager.CreateRenderTextureFromScreen();
                    }

                    RemakeTextures();

                    auto raceCameraManager = Gallop::RaceCameraManager::Instance();
                    if (raceCameraManager) {
                        raceCameraManager.SetupOrientation(isPortrait
                                                           ? Gallop::LowResolutionCameraUtil::DrawDirection::Portrait
                                                           : Gallop::LowResolutionCameraUtil::DrawDirection::Landscape);
                    }

                    auto director = GetSingletonInstance(
                            il2cpp_symbols::get_class("umamusume.dll", "Gallop.Live", "Director"));
                    if (director) {
                        il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *, int)>(
                                director->klass, "SetupOrientation", 1)(director,
                                                                        isPortrait
                                                                        ? 2 : 1);

                        auto ChampionsTextControllerField = il2cpp_class_get_field_from_name(
                                director->klass, "ChampionsTextController");
                        Il2CppObject *ChampionsTextController;
                        il2cpp_field_get_value(director, ChampionsTextControllerField,
                                               &ChampionsTextController);

                        if (ChampionsTextController) {
                            auto _flashPlayerField = il2cpp_class_get_field_from_name(
                                    ChampionsTextController->klass, "_flashPlayer");
                            Il2CppObject *_flashPlayer;
                            il2cpp_field_get_value(ChampionsTextController, _flashPlayerField,
                                                   &_flashPlayer);

                            auto root = il2cpp_symbols::get_method_pointer<Il2CppObject *(*)(
                                    Il2CppObject *)>(_flashPlayer->klass, "get_Root",
                                                     0)(_flashPlayer);

                            float scale = 1.0f;

                            if (_contentWidth < _contentHeight) {
                                scale = min(config::freeform_ui_scale_portrait, max(1.0f,
                                                                                    static_cast<float>(_contentHeight) *
                                                                                    config::runtime::ratioVertical) *
                                                                                config::freeform_ui_scale_portrait);
                            } else {
                                scale = min(config::freeform_ui_scale_landscape, max(1.0f,
                                                                                     static_cast<float>(_contentWidth) /
                                                                                     config::runtime::ratioHorizontal) *
                                                                                 config::freeform_ui_scale_landscape);
                            }

                            const auto availableWidth = static_cast<float>(_contentWidth) / scale;
                            const auto availableHeight = static_cast<float>(_contentHeight) / scale;

#ifdef _MSC_VER
                            auto width = ratio_16_9 * availableHeight;
#else
                            auto width = availableWidth;
#endif
                            auto height = availableHeight;

#ifdef _MSC_VER
                            if (width > availableWidth)
                            {
                                width = availableWidth;
                                height = width / ratio_16_9;
                            }
#endif

                            il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *,
                                                                        UnityEngine::Vector2)>(
                                    root->klass, "SetScreenReferenceSize", 1)(root,
                                                                              UnityEngine::Vector2{
                                                                                      width,
                                                                                      height});
                        }


                        auto liveFlashController = il2cpp_symbols::get_method_pointer<Il2CppObject *(*)(
                                Il2CppObject *)>(director->klass, "get_LiveFlashController",
                                                 0)(director);

                        if (liveFlashController) {
                            auto _flashPlayerField = il2cpp_class_get_field_from_name(
                                    liveFlashController->klass, "_flashPlayer");

                            if (_flashPlayerField) {
                                Il2CppObject *_flashPlayer;
                                il2cpp_field_get_value(liveFlashController, _flashPlayerField,
                                                       &_flashPlayer);

                                auto root = il2cpp_symbols::get_method_pointer<Il2CppObject *(*)(
                                        Il2CppObject *)>(_flashPlayer->klass, "get_Root",
                                                         0)(_flashPlayer);

                                float scale = 1.0f;

                                if (_contentWidth < _contentHeight) {
                                    scale = min(config::freeform_ui_scale_portrait, max(1.0f,
                                                                                        static_cast<float>(_contentHeight) *
                                                                                        config::runtime::ratioVertical) *
                                                                                    config::freeform_ui_scale_portrait);
                                } else {
                                    scale = min(config::freeform_ui_scale_landscape, max(1.0f,
                                                                                         static_cast<float>(_contentWidth) /
                                                                                         config::runtime::ratioHorizontal) *
                                                                                     config::freeform_ui_scale_landscape);
                                }

                                const auto availableWidth =
                                        static_cast<float>(_contentWidth) / scale;
                                const auto availableHeight =
                                        static_cast<float>(_contentHeight) / scale;

#ifdef _MSC_VER
                                auto width = ratio_16_9 * availableHeight;
#else
                                auto width = availableWidth;
#endif
                                auto height = availableHeight;

#ifdef _MSC_VER
                                if (width > availableWidth)
                                {
                                    width = availableWidth;
                                    height = width / ratio_16_9;
                                }
#endif

                                il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *,
                                                                            UnityEngine::Vector2)>(
                                        root->klass, "SetScreenReferenceSize", 1)(
                                        root, UnityEngine::Vector2{width, height});
                            }
                        }
                    }

                    if (tapEffectController) {
                        il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *)>(
                                tapEffectController->klass, "RefreshAll", 0)(
                                tapEffectController);
                    }

                    if (uiManager) {
                        uiManager.AdjustMissionClearContentsRootRect();
                        uiManager.AdjustSafeAreaToAnnounceRect();

                        /*Il2CppObject* _bgCamera = uiManager._bgCamera();

                        if (_bgCamera)
                        {
                            il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject*, Color_t)>(_bgCamera->klass, "set_backgroundColor", 1)(_bgCamera,
                                il2cpp_symbols::get_method_pointer<Color_t(*)()>("UnityEngine.CoreModule.dll", "UnityEngine", "Color", "get_clear", IgnoreNumberOfArguments)());
                        }*/
                    }

                    Gallop::Screen::OriginalScreenWidth(
                            isPortrait ? _contentHeight : _contentWidth);
                    Gallop::Screen::OriginalScreenHeight(
                            isPortrait ? _contentWidth : _contentHeight);
                });
            });
        });
    }

    void TickFrame() {
        try {
            if (config::unlock_size || config::freeform_window) {
                SetBGCanvasScalerSize();
            }

            if (config::freeform_window) {
                ResizeMoviePlayer();
            }

            auto sceneManager = Gallop::SceneManager::Instance();

            if (!sceneManager) {
                return;
            }

            il2cppstring sceneName = sceneManager.GetCurrentSceneIdName()->chars;

            if (sceneName == IL2CPP_STRING("Live")) {
                auto controller = Gallop::SceneManager::Instance().GetCurrentViewController();

                if (controller) {
                    LOGD("CONTROLLER: %s", controller->klass->name);
                }

                if (controller && controller->klass->name == "LiveViewController"s) {
                    auto director = GetSingletonInstance(
                            il2cpp_symbols::get_class("umamusume.dll", "Gallop.Live",
                                                      "Director"));
                    if (director) {
                        auto LiveCurrentTime = il2cpp_symbols::get_method_pointer<float (*)(
                                Il2CppObject *)>(director->klass, "get_LiveCurrentTime",
                                                 0)(director);
                        auto LiveTotalTime = il2cpp_symbols::get_method_pointer<float (*)(
                                Il2CppObject *)>(director->klass, "get_LiveTotalTime",
                                                 0)(director);

                        auto sliderCommon = Localify::UIParts::GetOptionSlider(
                                "live_slider");

                        auto textCommon = Localify::UIParts::GetTextCommon("live_slider");

                        if (textCommon) {
                            auto timeMin = static_cast<int>(LiveCurrentTime / 60);
                            auto timeSec = static_cast<int>(fmodf(LiveCurrentTime, 60));

                            auto timeMinIl2Cpp = to_string(timeMin);
                            auto timeSecIl2Cpp = to_string(timeSec);

                            stringstream str;
                            str << setw(2) << setfill('0') << timeSecIl2Cpp;

                            textCommon.text(il2cpp_string_new(
                                    (timeMinIl2Cpp + ":" + str.str()).data()));
                        }

                        auto textCommonTotal = Localify::UIParts::GetTextCommon(
                                "live_slider_total");

                        if (textCommonTotal) {
                            auto timeMin = static_cast<int>(LiveTotalTime / 60);
                            auto timeSec = static_cast<int>(fmodf(LiveTotalTime, 60));

                            auto timeMinIl2Cpp = to_string(timeMin);
                            auto timeSecIl2Cpp = to_string(timeSec);

                            stringstream str;
                            str << setw(2) << setfill('0') << timeSecIl2Cpp;

                            textCommonTotal.text(il2cpp_string_new(
                                    (timeMinIl2Cpp + ":" + str.str()).data()));
                        }

                        if (config::live_playback_loop) {
                            if (LiveCurrentTime >= LiveTotalTime - 0.1f) {
                                LiveCurrentTime = 0;
                                Localify::LiveUtils::MoveLivePlayback(LiveCurrentTime);
                            }
                        }

                        try {
                            if (sliderCommon) {
                                il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *,
                                                                            float)>(
                                        sliderCommon->klass, "set_maxValue", 1)(
                                        sliderCommon, LiveTotalTime);
                                il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *,
                                                                            float)>(
                                        sliderCommon->klass, "SetValueWithoutNotify",
                                        1)(sliderCommon, LiveCurrentTime);
                            }
                        }
                        catch (const Il2CppExceptionWrapper &ex) {
                            cout << ex.ex->klass->name << ": ";
                            wcout << ex.ex->message << endl;
                        }
                    }
                }
            }

            StartTickFrame();
        }
        catch (const Il2CppExceptionWrapper &ex) {
            LOGW("TickFrame error: %s", il2cpp_u8(ex.ex->message->chars).data());
        }
    }

    void StartTickFrame() {
        static auto tickFrameDelegate = CreateDelegateStatic(TickFrame);

        try {
            auto GameSystem = Gallop::GameSystem::Instance();
            if (GameSystem) {
                il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *, Il2CppDelegate *)>(
                        "umamusume.dll", "Gallop", "MonoBehaviourExtension", "WaitForEndFrame", 2)(
                        GameSystem, tickFrameDelegate);
            }
        }
        catch (const Il2CppExceptionWrapper &ex) {
            LOGW("StartTickFrame error: %s", il2cpp_u8(ex.ex->message->chars).data());
        }
    }

    void patch_game_assembly() {
        LOGI("patch_game_assembly");

        if (config::dump_entries) {
            Gallop::Localize::DumpAllEntries();
        }

        if (config::freeform_window) {
            auto javaVM = il2cpp_symbols::get_method_pointer<JavaVM *(*)()>(
                    "UnityEngine.AndroidJNIModule.dll", "UnityEngine", "AndroidJNI", "GetJavaVM",
                    0)();

            JNIEnv *env;
            jint res = javaVM->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);

            if (res == JNI_OK) {
                auto il2cppActivity = il2cpp_symbols::get_method_pointer<Il2CppObject *(*)()>(
                        "UnityEngine.AndroidJNIModule.dll", "UnityEngine.Android", "AndroidApp",
                        "get_Activity", 0)();
                auto activity = il2cpp_symbols::get_method_pointer<jobject (*)(Il2CppObject *)>(
                        il2cppActivity->klass, "GetRawObject", 0)(il2cppActivity);

                jclass activityClass = env->GetObjectClass(activity);
                jmethodID getWindowManagerMethod = env->GetMethodID(activityClass,
                                                                    "getWindowManager",
                                                                    "()Landroid/view/WindowManager;");
                jobject windowManager = env->CallObjectMethod(activity, getWindowManagerMethod);

                jclass windowManagerClass = env->GetObjectClass(windowManager);
                jmethodID getCurrentWindowMetricsMethod = env->GetMethodID(windowManagerClass,
                                                                           "getCurrentWindowMetrics",
                                                                           "()Landroid/view/WindowMetrics;");

                auto metrics = env->CallObjectMethod(windowManager, getCurrentWindowMetricsMethod);

                auto metricsClass = env->GetObjectClass(metrics);

                auto getRectId = env->GetMethodID(metricsClass, "getBounds",
                                                  "()Landroid/graphics/Rect;");
                auto rect = env->CallObjectMethod(metrics, getRectId);

                auto rectClass = env->GetObjectClass(rect);

                auto widthId = env->GetMethodID(rectClass, "width", "()I");
                const jint width = env->CallIntMethod(rect, widthId);

                auto heightId = env->GetMethodID(rectClass, "height", "()I");
                const jint height = env->CallIntMethod(rect, heightId);

                const auto isPortrait = width <= height;
                Gallop::Screen::OriginalScreenWidth(isPortrait ? height : width);
                Gallop::Screen::OriginalScreenHeight(isPortrait ? width : height);

                ResizeWindow(width, height);

                jclass activityInfoClass = env->FindClass("android/content/pm/ActivityInfo");
                jfieldID SCREEN_ORIENTATION_FieldID = env->GetStaticFieldID(activityInfoClass,
                                                                            "SCREEN_ORIENTATION_FULL_USER",
                                                                            "I");
                jint SCREEN_ORIENTATION = env->GetStaticIntField(activityInfoClass,
                                                                 SCREEN_ORIENTATION_FieldID);

                jmethodID setRequestedOrientation = env->GetMethodID(activityClass,
                                                                     "setRequestedOrientation",
                                                                     "(I)V");

                env->CallVoidMethod(activity, setRequestedOrientation, SCREEN_ORIENTATION);

                env->DeleteLocalRef(activityClass);
                env->DeleteLocalRef(windowManager);
                env->DeleteLocalRef(windowManagerClass);
                env->DeleteLocalRef(metrics);
                env->DeleteLocalRef(metricsClass);
                env->DeleteLocalRef(rect);
                env->DeleteLocalRef(rectClass);
                env->DeleteLocalRef(activityInfoClass);
            }
        }

        if (!config::unlock_live_chara) {
            try {
                auto path = il2cpp_symbols::get_method_pointer<Il2CppString *(*)()>(
                        "Cute.Core.Assembly.dll", "Cute.Core", "Device", "GetPersistentDataPath",
                        0)()->chars;

                if (filesystem::exists(
                        path + il2cppstring(IL2CPP_STRING(R"(\master\master_orig.mdb)")))) {
                    filesystem::remove_all(path + il2cppstring(IL2CPP_STRING(R"(\master)")));
                }
            }
            catch (const exception &ex) {
                wcerr << L"Failed to remove master_orig.mdb: " << ex.what() << endl;
            }
        }

        StartTickFrame();

        auto sceneManagerClass = il2cpp_symbols::get_class("UnityEngine.CoreModule.dll",
                                                           "UnityEngine.SceneManagement",
                                                           "SceneManager");

        auto activeSceneChangedField = il2cpp_class_get_field_from_name(sceneManagerClass,
                                                                        "activeSceneChanged");

        auto action = CreateDelegateWithClassStatic(
                il2cpp_class_from_type(activeSceneChangedField->type),
                *([](void *, UnityEngine::SceneManagement::Scene scene,
                     UnityEngine::SceneManagement::Scene scene1) {

                    auto sceneManager = Gallop::SceneManager::Instance();

                    if (!sceneManager) {
                        return;
                    }

                    il2cppstring sceneName = sceneManager.GetCurrentSceneIdName()->chars;

                    auto uiManager = Gallop::UIManager::Instance();

                    if (sceneName == IL2CPP_STRING("Title")) {
                        if (config::character_system_text_caption) {
                            Localify::NotificationManager::Reset();
                        }

                        if (config::freeform_window) {
                            int width = UnityEngine::Screen::width();
                            int height = UnityEngine::Screen::height();

                            bool isVirt = width < height;
                            Gallop::Screen::OriginalScreenWidth(width);
                            Gallop::Screen::OriginalScreenHeight(height);
//                            Gallop::StandaloneWindowResize::IsVirt(isVirt);
                        }
                    }

                    if (sceneName == IL2CPP_STRING("Home")) {
                        if (config::character_system_text_caption) {
                            Localify::NotificationManager::Init();
                        }

                        if (config::unlock_live_chara) {
                            auto charaList = MsgPackModify::GetCharaList();

                            auto workDataManager = GetSingletonInstance(
                                    il2cpp_symbols::get_class("umamusume.dll", "Gallop",
                                                              "WorkDataManager"));

                            auto workCharaData = il2cpp_symbols::get_method_pointer<Il2CppObject *(*)(
                                    Il2CppObject *)>(workDataManager->klass, "get_CharaData",
                                                     0)(workDataManager);

                            auto UserCharaClass = il2cpp_symbols::get_class("umamusume.Http.dll",
                                                                            "Gallop", "UserChara");

                            if (!UserCharaClass) {
                                UserCharaClass = il2cpp_symbols::get_class("umamusume.dll",
                                                                           "Gallop", "UserChara");
                            }

                            for (auto &chara: charaList) {
                                auto userChara = il2cpp_object_new(UserCharaClass);

                                auto chara_id_field = il2cpp_class_get_field_from_name(
                                        userChara->klass, "chara_id");
                                int chara_id = chara["chara_id"].int32_value();
                                il2cpp_field_set_value(userChara, chara_id_field, &chara_id);

                                auto training_num_field = il2cpp_class_get_field_from_name(
                                        userChara->klass, "training_num");
                                int training_num = chara["training_num"].int32_value();
                                il2cpp_field_set_value(userChara, training_num_field,
                                                       &training_num);

                                auto love_point_field = il2cpp_class_get_field_from_name(
                                        userChara->klass, "love_point");
                                int love_point = chara["love_point"].int32_value();
                                il2cpp_field_set_value(userChara, love_point_field, &love_point);

                                auto love_point_pool_field = il2cpp_class_get_field_from_name(
                                        userChara->klass, "love_point_pool");
                                if (love_point_pool_field) {
                                    int love_point_pool = chara["love_point_pool"].int32_value();
                                    il2cpp_field_set_value(userChara, love_point_pool_field,
                                                           &love_point_pool);
                                }

                                auto fan_field = il2cpp_class_get_field_from_name(userChara->klass,
                                                                                  "fan");
                                uint64_t fan = chara["fan"].uint64_value();
                                il2cpp_field_set_value(userChara, fan_field, &fan);

                                il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *,
                                                                            Il2CppObject *)>(
                                        workCharaData->klass, "UpdateCharaData", 1)(
                                        workCharaData, userChara);
                            }
                        }
                    }

                    if (sceneName == IL2CPP_STRING("Live") && config::champions_live_show_text) {
                        auto loadSettings = il2cpp_symbols::get_method_pointer<Il2CppObject *(*)()>(
                                "umamusume.dll", "Gallop.Live", "Director", "get_LoadSettings",
                                IgnoreNumberOfArguments)();
                        auto musicId = il2cpp_symbols::get_method_pointer<int (*)(
                                Il2CppObject *)>(loadSettings->klass, "get_MusicId",
                                                 0)(loadSettings);

                        if (musicId == 1054) {
                            auto raceInfo = il2cpp_symbols::get_method_pointer<Il2CppObject *(*)(
                                    Il2CppObject *)>(loadSettings->klass, "get_raceInfo",
                                                     0)(loadSettings);

                            auto resourceId = il2cpp_symbols::get_method_pointer<int (*)(
                                    Il2CppObject *)>(raceInfo->klass,
                                                     "get_ChampionsMeetingResourceId",
                                                     0)(raceInfo);

                            if (resourceId == 0) {
                                auto charaNameArray = il2cpp_array_new_type<Il2CppString *>(
                                        il2cpp_defaults.string_class, 9);
                                auto trainerNameArray = il2cpp_array_new_type<Il2CppString *>(
                                        il2cpp_defaults.string_class, 9);

                                il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *,
                                                                            Il2CppArraySize_t<Il2CppString *> *)>(
                                        raceInfo->klass, "set_CharacterNameArray",
                                        1)(raceInfo, charaNameArray);
                                il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *,
                                                                            Il2CppArraySize_t<Il2CppString *> *)>(
                                        raceInfo->klass, "set_TrainerNameArray", 1)(
                                        raceInfo, trainerNameArray);

                                il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *,
                                                                            Il2CppArraySize_t<Il2CppString *> *)>(
                                        raceInfo->klass, "set_CharacterNameArrayForChampionsText",
                                        1)(raceInfo, nullptr);
                                il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *,
                                                                            Il2CppArraySize_t<Il2CppString *> *)>(
                                        raceInfo->klass, "set_TrainerNameArrayForChampionsText",
                                        1)(raceInfo, nullptr);

                                auto charaInfoList = il2cpp_symbols::get_method_pointer<Il2CppObject *(*)(
                                        Il2CppObject *)>(loadSettings->klass,
                                                         "get_CharacterInfoList", 0)(
                                        loadSettings);

                                FieldInfo *itemsField = il2cpp_class_get_field_from_name(
                                        charaInfoList->klass, "_items");
                                Il2CppArraySize_t<Il2CppObject *> *charaInfoArr;
                                il2cpp_field_get_value(charaInfoList, itemsField, &charaInfoArr);

                                for (int i = 0; i < 9; i++) {
                                    auto info = charaInfoArr->vector[i];
                                    auto charaId = il2cpp_symbols::get_method_pointer<int (*)(
                                            Il2CppObject *)>(info->klass, "get_CharaId",
                                                             0)(info);
                                    auto mobId = il2cpp_symbols::get_method_pointer<int (*)(
                                            Il2CppObject *)>(info->klass, "get_MobId",
                                                             0)(info);

                                    Il2CppString *charaName;
                                    if (charaId == 1) {
                                        charaName = il2cpp_symbols::get_method_pointer<Il2CppString *(*)(
                                                int, int)>("umamusume.dll", "Gallop", "TextUtil",
                                                           "GetMasterText", 2)(59, mobId);
                                    } else {
                                        charaName = il2cpp_symbols::get_method_pointer<Il2CppString *(*)(
                                                int, int)>("umamusume.dll", "Gallop", "TextUtil",
                                                           "GetMasterText", 2)(6, charaId);
                                    }

                                    il2cpp_array_setref(charaNameArray, i, charaName);
                                    il2cpp_array_setref(trainerNameArray, i, il2cpp_string_new(""));
                                }

                                il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *,
                                                                            int)>(
                                        raceInfo->klass, "set_ChampionsMeetingResourceId",
                                        1)(raceInfo,
                                           config::champions_live_resource_id);
                                il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *,
                                                                            int)>(
                                        raceInfo->klass, "set_DateYear", 1)(raceInfo,
                                                                            config::champions_live_year);
                            }
                        }
                    }
                })
        );
        il2cpp_field_static_set_value(activeSceneChangedField, action);
    }
}

static void *il2cpp_handle = nullptr;

static bool dlopen_process(const char *name, void *handle) {
    if (!il2cpp_handle) {
        if (name != nullptr && strstr(name, "libil2cpp.so")) {
            il2cpp_handle = handle;
            LOGI("Got il2cpp handle: %p", handle);

            config::read_config_init();

            il2cpp_init_addr = dlsym(il2cpp_handle, "il2cpp_init");
            il2cpp_symbols::init(il2cpp_handle);
            DobbyHook(il2cpp_init_addr,
                      reinterpret_cast<void *>(il2cpp_init_hook),
                      &il2cpp_init_orig);

            thread init_thread([]() {
                logger::init_logger();
                local::load_textdb(&config::dicts);

                if (!config::text_id_dict.empty()) {
                    local::load_textId_textdb(config::text_id_dict);
                }
            });
            init_thread.detach();
            return true;
        }
    }
    return false;
}

HOOK_DEF(void*, do_dlopen, const char *name, int flags) {
    void *handle = orig_do_dlopen(name, flags);
    if (dlopen_process(name, handle)) {
        DobbyDestroy(addr_do_dlopen);
    }
    return handle;
}

HOOK_DEF(void*, do_dlopen_V24, const char *name, int flags, const void *extinfo [[maybe_unused]],
         void *caller_addr [[maybe_unused]]) {
    void *handle = orig_do_dlopen_V24(name, flags, extinfo, caller_addr);
    if (dlopen_process(name, handle)) {
        DobbyDestroy(addr_do_dlopen_V24);
    }
    return handle;
}

HOOK_DEF(void*, NativeBridgeLoadLibrary_V21, const char *filename, int flag) {
    if (string(filename).find(string("libmain.so")) != string::npos) {
        auto nativeBridge = dlopen("libnativebridge.so", RTLD_NOW);
        auto *NativeBridgeError = reinterpret_cast<bool (*)()>(dlsym(nativeBridge,
                                                                     "_ZN7android17NativeBridgeErrorEv"));

        stringstream path_armV8;
        path_armV8 << "/data/data/" << Game::GetCurrentPackageName().data() << "/arm64-v8a.so";
        stringstream path_armV7;
        path_armV7 << "/data/data/" << Game::GetCurrentPackageName().data() << "/armeabi-v7a.so";

        string path;

        if (access(path_armV8.str().data(), F_OK) != -1) {
            path = path_armV8.str();
        } else if (access(path_armV7.str().data(), F_OK) != -1) {
            path = path_armV7.str();
        }

        if (!path.empty()) {
            thread load_thread([path, NativeBridgeError]() {
                void *lib = orig_NativeBridgeLoadLibrary_V21(path.data(), RTLD_NOW);
                LOGI("%s: %p", path.data(), lib);
                if (NativeBridgeError()) {
                    LOGW("LoadLibrary failed");
                }
                DobbyDestroy(addr_NativeBridgeLoadLibrary_V21);
            });
            load_thread.detach();
        }
    }

    return orig_NativeBridgeLoadLibrary_V21(filename, flag);
}

HOOK_DEF(void*, NativeBridgeLoadLibraryExt_V26, const char *filename, int flag,
         struct native_bridge_namespace_t *ns) {
    if (string(filename).find(string("libmain.so")) != string::npos) {
        auto nativeBridge = dlopen("libnativebridge.so", RTLD_NOW);
        auto *NativeBridgeError = reinterpret_cast<bool (*)()>(dlsym(nativeBridge,
                                                                     "_ZN7android17NativeBridgeErrorEv"));
        auto *NativeBridgeGetError = reinterpret_cast<char *(*)()>(dlsym(nativeBridge,
                                                                         "_ZN7android20NativeBridgeGetErrorEv"));

        stringstream path_armV8;
        path_armV8 << "/data/data/" << Game::GetCurrentPackageName().data() << "/arm64-v8a.so";
        stringstream path_armV7;
        path_armV7 << "/data/data/" << Game::GetCurrentPackageName().data() << "/armeabi-v7a.so";

        string path;

        if (access(path_armV8.str().data(), F_OK) != -1) {
            path = path_armV8.str();
        } else if (access(path_armV7.str().data(), F_OK) != -1) {
            path = path_armV7.str();
        }

        if (!path.empty()) {
            void *lib = orig_NativeBridgeLoadLibraryExt_V26(path.data(), RTLD_NOW, ns);
            LOGI("%s: %p", path.data(), lib);
            if (NativeBridgeError()) {
                char *error_bridge = NativeBridgeGetError();
                if (error_bridge) {
                    LOGW("error_bridge: %s", error_bridge);
                }
            }
            DobbyDestroy(addr_NativeBridgeLoadLibraryExt_V26);
        }
    }

    return orig_NativeBridgeLoadLibraryExt_V26(filename, flag, ns);
}

HOOK_DEF(void*, NativeBridgeLoadLibraryExt_V30, const char *filename, int flag,
         struct native_bridge_namespace_t *ns) {
    LOGD("NativeBridgeLoadLibraryExt_V30: %s", filename);
    if (string(filename).find(string("libmain.so")) != string::npos) {
        auto nativeBridge = dlopen("libnativebridge.so", RTLD_NOW);
        auto *NativeBridgeError = reinterpret_cast<bool (*)()>(dlsym(nativeBridge,
                                                                     "NativeBridgeError"));
        auto *NativeBridgeGetError = reinterpret_cast<char *(*)()>(dlsym(nativeBridge,
                                                                         "NativeBridgeGetError"));

        stringstream path_armV8;
        path_armV8 << "/data/data/" << Game::GetCurrentPackageName().data() << "/arm64-v8a.so";
        stringstream path_armV7;
        path_armV7 << "/data/data/" << Game::GetCurrentPackageName().data() << "/armeabi-v7a.so";

        string path;

        if (access(path_armV8.str().data(), F_OK) != -1) {
            path = path_armV8.str();
        } else if (access(path_armV7.str().data(), F_OK) != -1) {
            path = path_armV7.str();
        }

        if (!path.empty()) {
            void *lib = orig_NativeBridgeLoadLibraryExt_V30(path.data(), RTLD_NOW, ns);
            if (NativeBridgeError()) {
                if (auto error_bridge = NativeBridgeGetError()) {
                    LOGW("error_bridge: %s", error_bridge);
                }
            }
            DobbyDestroy(addr_NativeBridgeLoadLibraryExt_V30);
        }
    }

    return orig_NativeBridgeLoadLibraryExt_V30(filename, flag, ns);
}

//optional<vector<string>> read_config() {
//    ifstream config_stream{
//            string("/sdcard/Android/data/").append(Game::GetCurrentPackageName()).append(
//                    "/config.json")};
//    vector<string> dicts{};
//
//    if (!config_stream.is_open()) {
//        LOGW("config.json not loaded.");
//        return nullopt;
//    }
//
//    LOGI("config.json loaded.");
//
//    rapidjson::IStreamWrapper wrapper{config_stream};
//    rapidjson::Document document;
//
//    document.ParseStream(wrapper);
//
//    if (!document.HasParseError()) {
//        if (document.HasMember("enableLogger")) {
//            g_enable_logger = document["enableLogger"].GetBool();
//        }
//        if (document.HasMember("dumpStaticEntries")) {
//            g_dump_entries = document["dumpStaticEntries"].GetBool();
//        }
//        if (document.HasMember("dumpDbEntries")) {
//            g_dump_db_entries = document["dumpDbEntries"].GetBool();
//        }
//        if (document.HasMember("staticEntriesUseHash")) {
//            g_static_entries_use_hash = document["staticEntriesUseHash"].GetBool();
//        }
//        if (document.HasMember("staticEntriesUseTextIdName")) {
//            g_static_entries_use_text_id_name = document["staticEntriesUseTextIdName"].GetBool();
//        }
//        if (document.HasMember("maxFps")) {
//            g_max_fps = document["maxFps"].GetInt();
//        }
//        if (document.HasMember("uiAnimationScale")) {
//            g_ui_animation_scale = document["uiAnimationScale"].GetFloat();
//        }
//        if (document.HasMember("uiUseSystemResolution")) {
//            g_ui_use_system_resolution = document["uiUseSystemResolution"].GetBool();
//        }
//        if (document.HasMember("resolution3dScale")) {
//            g_resolution_3d_scale = document["resolution3dScale"].GetFloat();
//        }
//        if (document.HasMember("replaceFont")) {
//            g_replace_to_builtin_font = document["replaceFont"].GetBool();
//        }
//        if (!document.HasMember("replaceFont") && document.HasMember("replaceToBuiltinFont")) {
//            g_replace_to_builtin_font = document["replaceToBuiltinFont"].GetBool();
//        }
//        if (document.HasMember("replaceToCustomFont")) {
//            g_replace_to_custom_font = document["replaceToCustomFont"].GetBool();
//        }
//        if (document.HasMember("fontAssetBundlePath")) {
//            g_font_assetbundle_path = string(document["fontAssetBundlePath"].GetString());
//        }
//        if (document.HasMember("fontAssetName")) {
//            g_font_asset_name = string(document["fontAssetName"].GetString());
//        }
//        if (document.HasMember("tmproFontAssetName")) {
//            g_tmpro_font_asset_name = string(document["tmproFontAssetName"].GetString());
//        }
//        if (document.HasMember("graphicsQuality")) {
//            g_graphics_quality = document["graphicsQuality"].GetInt();
//            if (g_graphics_quality < -1) {
//                g_graphics_quality = -1;
//            }
//            if (g_graphics_quality > 4) {
//                g_graphics_quality = 3;
//            }
//        }
//        if (document.HasMember("antiAliasing")) {
//            g_anti_aliasing = document["antiAliasing"].GetInt();
//            vector<int> options = {0, 2, 4, 8, -1};
//            g_anti_aliasing =
//                    options[find(options.begin(), options.end(), g_anti_aliasing) -
//                            options.begin()];
//        }
//        if (document.HasMember("forceLandscape")) {
//            g_force_landscape = document["forceLandscape"].GetBool();
//        }
//        if (document.HasMember("forceLandscapeUiScale")) {
//            g_force_landscape_ui_scale = document["forceLandscapeUiScale"].GetFloat();
//            if (g_force_landscape_ui_scale <= 0) {
//                g_force_landscape_ui_scale = 1;
//            }
//        }
//        if (document.HasMember("uiLoadingShowOrientationGuide")) {
//            g_ui_loading_show_orientation_guide = document["uiLoadingShowOrientationGuide"].GetBool();
//        }
//        /*if (document.HasMember("restoreNotification")) {
//            g_restore_notification = document["restoreNotification"].GetBool();
//        }*/
//        if (document.HasMember("replaceAssetsPath")) {
//            auto replaceAssetsPath = localify::u8_u16(document["replaceAssetsPath"].GetString());
//            if (!replaceAssetsPath.starts_with(u"/")) {
//                replaceAssetsPath.insert(0, u16string(u"/sdcard/Android/data/").append(
//                        localify::u8_u16(Game::GetCurrentPackageName())).append(u"/"));
//            }
//            if (filesystem::exists(replaceAssetsPath) &&
//                filesystem::is_directory(replaceAssetsPath)) {
//                for (auto &file: filesystem::directory_iterator(replaceAssetsPath)) {
//                    if (file.is_regular_file()) {
//                        g_replace_assets.emplace(file.path().filename().string(),
//                                                 ReplaceAsset{file.path().string(), nullptr});
//                    }
//                }
//            }
//        }
//
//        if (document.HasMember("replaceAssetBundleFilePath")) {
//            auto replaceAssetBundleFilePath = localify::u8_u16(
//                    document["replaceAssetBundleFilePath"].GetString());
//            if (!replaceAssetBundleFilePath.starts_with(u"/")) {
//                replaceAssetBundleFilePath.insert(0, u16string(u"/sdcard/Android/data/").append(
//                        localify::u8_u16(Game::GetCurrentPackageName())).append(u"/"));
//            }
//            if (filesystem::exists(replaceAssetBundleFilePath) &&
//                filesystem::is_regular_file(replaceAssetBundleFilePath)) {
//                g_replace_assetbundle_file_path = localify::u16_u8(replaceAssetBundleFilePath);
//            }
//        }
//
//        // Not working correctly...
//        /*if (document.HasMember("replaceTextDBPath")) {
//            auto replaceTextDBPath = localify::u8_u16(
//                    document["replaceTextDBPath"].GetString());
//            if (!replaceTextDBPath.starts_with(u"/")) {
//                replaceTextDBPath.insert(0, u16string(u"/sdcard/Android/data/").append(
//                        localify::u8_u16(Game::GetCurrentPackageName())).append(u"/"));
//            }
//            if (filesystem::exists(replaceTextDBPath) &&
//                filesystem::is_regular_file(replaceTextDBPath)) {
//                g_replace_text_db_path = localify::u16_u8(replaceTextDBPath);
//            }
//        }*/
//
//        if (document.HasMember("characterSystemTextCaption")) {
//            g_character_system_text_caption = document["characterSystemTextCaption"].GetBool();
//        }
//
//        if (document.HasMember("cySpringUpdateMode")) {
//            g_cyspring_update_mode = document["cySpringUpdateMode"].GetInt();
//            vector<int> options = {0, 1, 2, 3, -1};
//            g_cyspring_update_mode =
//                    options[find(options.begin(), options.end(), g_cyspring_update_mode) -
//                            options.begin()];
//        } else if (g_max_fps > 30) {
//            g_cyspring_update_mode = 1;
//        }
//
//        if (document.HasMember("hideNowLoading")) {
//            g_hide_now_loading = document["hideNowLoading"].GetBool();
//        }
//
//        if (document.HasMember("textIdDict")) {
//            text_id_dict = document["textIdDict"].GetString();
//        }
//
//        if (document.HasMember("dicts")) {
//            auto &dicts_arr = document["dicts"];
//            auto len = dicts_arr.Size();
//
//            for (size_t i = 0; i < len; ++i) {
//                auto dict = dicts_arr[i].GetString();
//
//                dicts.emplace_back(dict);
//            }
//        }
//
//        if (document.HasMember("dumpMsgPack")) {
//            g_dump_msgpack = document["dumpMsgPack"].GetBool();
//        }
//
//        if (document.HasMember("dumpMsgPackRequest")) {
//            g_dump_msgpack_request = document["dumpMsgPackRequest"].GetBool();
//        }
//
//        if (document.HasMember("packetNotifier")) {
//            g_packet_notifier = document["packetNotifier"].GetString();
//        }
//
//        if (Game::CurrentGameRegion == Game::Region::KOR) {
//            if (document.HasMember("restoreGallopWebview")) {
//                g_restore_gallop_webview = document["restoreGallopWebview"].GetBool();
//            }
//            if (document.HasMember("useThirdPartyNews")) {
//                g_use_third_party_news = document["useThirdPartyNews"].GetBool();
//            }
//        }
//    }
//
//    config_stream.close();
//    return dicts;
//}

void *GetNativeBridgeLoadLibrary(void *fallbackAddress) {
    void *handle = dlopen(GetNativeBridgeLibrary().data(), RTLD_NOW);
    // clear error
    dlerror();
    if (handle) {
        auto itf = reinterpret_cast<NativeBridgeCallbacks *>(dlsym(handle, "NativeBridgeItf"));
        LOGI("NativeBridgeItf version: %d", itf->version);
        if (GetAndroidApiLevel() >= 26) {
            return reinterpret_cast<void *>(itf->loadLibraryExt);
        }
        return reinterpret_cast<void *>(itf->loadLibrary);
    }
    return fallbackAddress;
}

extern "C" void
onConfigurationChanged_native(JNIEnv *env, jobject /*this*/, jobject activity, jobject newConfig) {
    if (!config::freeform_window) {
        return;
    }

    if (IsABIRequiredNativeBridge()) {
        return;
    }

    if (newConfig == nullptr) {
        return;
    }

    if (!il2cpp_is_vm_thread(il2cpp_thread_current())) {
        return;
    }

    auto windowMetricsCalculatorClass = env->FindClass(
            "androidx/window/layout/WindowMetricsCalculatorCompat");
    auto windowMetricsCalculatorInitId = env->GetMethodID(windowMetricsCalculatorClass, "<init>",
                                                          "()V");

    auto windowMetricsCalculator = env->NewObject(windowMetricsCalculatorClass,
                                                  windowMetricsCalculatorInitId);

    auto computeId = env->GetMethodID(windowMetricsCalculatorClass, "computeCurrentWindowMetrics",
                                      "(Landroid/app/Activity;)Landroidx/window/layout/WindowMetrics;");
    auto metrics = env->CallObjectMethod(windowMetricsCalculator, computeId, activity);

    auto metricsClass = env->GetObjectClass(metrics);

    auto getRectId = env->GetMethodID(metricsClass, "getBounds", "()Landroid/graphics/Rect;");
    auto rect = env->CallObjectMethod(metrics, getRectId);

    auto rectClass = env->GetObjectClass(rect);

    auto widthId = env->GetMethodID(rectClass, "width", "()I");
    const jint width = env->CallIntMethod(rect, widthId);

    auto heightId = env->GetMethodID(rectClass, "height", "()I");
    const jint height = env->CallIntMethod(rect, heightId);

    env->DeleteLocalRef(windowMetricsCalculatorClass);
    env->DeleteLocalRef(windowMetricsCalculator);
    env->DeleteLocalRef(metrics);
    env->DeleteLocalRef(metricsClass);
    env->DeleteLocalRef(rect);
    env->DeleteLocalRef(rectClass);

    auto gameSystem = Gallop::GameSystem::Instance();

    auto ValueTuple2Class = GetGenericClass(
            GetRuntimeType("mscorlib.dll", "System", "ValueTuple`2"),
            GetRuntimeType(il2cpp_defaults.int32_class),
            GetRuntimeType(il2cpp_defaults.int32_class));
    auto tuple = System::ValueTuple<int, int>{width, height};
    auto boxed = il2cpp_value_box(ValueTuple2Class, &tuple);

    auto fn = *[](Il2CppObject *self) {
        auto tuple = *il2cpp_object_unbox_type<System::ValueTuple<int, int> *>(self);
        ResizeWindow(tuple.Item1, tuple.Item2);
    };
    il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject *, Il2CppDelegate *)>("umamusume.dll",
                                                                                   "Gallop",
                                                                                   "MonoBehaviourExtension",
                                                                                   "WaitForEndFrame",
                                                                                   2)(gameSystem,
                                                                                      CreateDelegate(
                                                                                              boxed,
                                                                                              fn));
}

void hack_thread(void *arg [[maybe_unused]]) {
    LOGI("%s hack thread: %d", ABI, gettid());

    const int api_level = GetAndroidApiLevel();
    LOGI("%s api level: %d", ABI, api_level);

    void *addr = nullptr;
    if (IsRunningOnNativeBridge()) {
        addr = reinterpret_cast<void *>(dlopen);
    } else if (!IsABIRequiredNativeBridge()) {
        if (ABI == "x86"s) {
            addr = reinterpret_cast<void *>(dlopen);
        } else {
            addr = dlsym(dlopen("libdl.so", RTLD_NOW),
                         "__dl__Z9do_dlopenPKciPK17android_dlextinfoPKv");
        }
    }

    if (addr) {
        LOGI("%s do_dlopen at: %p", ABI, addr);
        if (IsRunningOnNativeBridge() || ABI == "x86"s) {
            addr_do_dlopen = addr;
            DobbyHook(addr_do_dlopen, reinterpret_cast<void *>(new_do_dlopen),
                      reinterpret_cast<void **>(&orig_do_dlopen));
        } else {
            addr_do_dlopen_V24 = addr;
            DobbyHook(addr_do_dlopen_V24, reinterpret_cast<void *>(new_do_dlopen_V24),
                      reinterpret_cast<void **>(&orig_do_dlopen_V24));
        }
    }

    if (IsABIRequiredNativeBridge()) {
        if (api_level >= 30) {
            addr_NativeBridgeLoadLibraryExt_V30 = dlsym(dlopen("libnativebridge.so", RTLD_NOW),
                                                        "NativeBridgeLoadLibraryExt");
            if (addr_NativeBridgeLoadLibraryExt_V30) {
                LOGI("NativeBridgeLoadLibraryExt at: %p", addr_NativeBridgeLoadLibraryExt_V30);
                DobbyHook(addr_NativeBridgeLoadLibraryExt_V30,
                          reinterpret_cast<void *>(new_NativeBridgeLoadLibraryExt_V30),
                          reinterpret_cast<void **>(&orig_NativeBridgeLoadLibraryExt_V30));
            }
        } else if (api_level >= 26) {
            addr_NativeBridgeLoadLibraryExt_V26 = dlsym(dlopen("libnativebridge.so", RTLD_NOW),
                                                        "_ZN7android26NativeBridgeLoadLibraryExtEPKciPNS_25native_bridge_namespace_tE");
            if (addr_NativeBridgeLoadLibraryExt_V26) {
                LOGI("NativeBridgeLoadLibraryExt at: %p", addr_NativeBridgeLoadLibraryExt_V26);
                DobbyHook(addr_NativeBridgeLoadLibraryExt_V26,
                          reinterpret_cast<void *>(new_NativeBridgeLoadLibraryExt_V26),
                          reinterpret_cast<void **>(&orig_NativeBridgeLoadLibraryExt_V26));
            }
        } else {
            addr_NativeBridgeLoadLibrary_V21 = dlsym(dlopen("libnativebridge.so", RTLD_NOW),
                                                     "_ZN7android23NativeBridgeLoadLibraryEPKci");
            if (addr_NativeBridgeLoadLibrary_V21) {
                LOGI("NativeBridgeLoadLibrary at: %p", addr_NativeBridgeLoadLibrary_V21);
                DobbyHook(addr_NativeBridgeLoadLibrary_V21,
                          reinterpret_cast<void *>(new_NativeBridgeLoadLibrary_V21),
                          reinterpret_cast<void **>(&orig_NativeBridgeLoadLibrary_V21));
            }
        }
    }
}
