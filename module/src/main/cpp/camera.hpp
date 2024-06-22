//
// Created by 지오 on 6/11/23.
//

#ifndef UMAMUSUME_LOCALIFY_ANDROID_CAMERA_HPP
#define UMAMUSUME_LOCALIFY_ANDROID_CAMERA_HPP

#include <stddef.h>
#include <stdint.h>
#include "il2cpp/il2cpp-class.h"

namespace Gallop {
    namespace Camera {
        enum Type {
            CAMERA_RACE
        };

        Vector3_t getCameraPos();
        void setCameraType(int value);
        Quaternion_t slerpTwo(Quaternion_t& rot, Quaternion_t& rot2, float t);
        Quaternion_t updateLookAtByRotation(Quaternion_t rot);
        Quaternion_t updatePosAndLookAtByRotation(Vector3_t pos, Quaternion_t rot);
        Quaternion_t updatePosAndLookAtByRotationStable(Vector3_t pos, Quaternion_t rot);

        float getRaceCamFov();

        void reset_camera();
    }
}


#endif //UMAMUSUME_LOCALIFY_ANDROID_CAMERA_HPP
