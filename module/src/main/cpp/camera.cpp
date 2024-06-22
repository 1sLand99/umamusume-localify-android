#include "camera.hpp"
#include <unistd.h>
#include <string>
#include <vector>

using namespace std;

namespace Gallop {
    namespace {
        int cameraType = -1;
        float look_radius = 0;  // 조향 반경?

        float raceDefaultFOV = 12;

        Vector3_t cameraPos{-51.72, 50, 108.57};

        Vector3_t raceFirstPersonLookAtOffset{0, 0, 0};

        float raceFirstShakeStrength = 0;

        typedef struct Point {
            double x, y;

            Point() {
                x = 0;
                y = 0;
            }
        } SDPoint;
    }
    namespace Camera {
        struct Vector3 {
        public:
            float x, y, z;

            Vector3(float _x, float _y, float _z) {
                x = _x;
                y = _y;
                z = _z;
            }

            Vector3(Vector3_t &vec) {
                x = vec.x;
                y = vec.y;
                z = vec.z;
            }

            Vector3 operator+(const Vector3 &v) const {
                return {x + v.x, y + v.y, z + v.z};
            }

            Vector3 operator-(const Vector3 &other) const {
                return {x - other.x, y - other.y, z - other.z};
            }

            Vector3 operator*(float s) const {
                return {x * s, y * s, z * s};
            }

            Vector3 normalized() const {
                float n = norm();
                return {x / n, y / n, z / n};
            }

            float norm() const {
                return sqrt(x * x + y * y + z * z);
            }

            static Vector3 cross(const Vector3 &a, const Vector3 &b) {
                return {a.y * b.z - a.z * b.y,
                               a.z * b.x - a.x * b.z,
                               a.x * b.y - a.y * b.x};
            }
        };


        struct Quaternion {
        public :
            float w, x, y, z;

            Quaternion(float _w, float _x, float _y, float _z) {
                w = _w;
                x = _x;
                y = _y;
                z = _z;
            }

            Quaternion(Quaternion_t &quat) {
                w = quat.w;
                x = quat.x;
                y = quat.y;
                z = quat.z;
            }

            operator Quaternion_t() const {
                return Quaternion_t{w, x, y, z};
            }

            Quaternion operator*(const Quaternion &q) const {
                const float nw = w * q.w - x * q.x - y * q.y - z * q.z;
                const float nx = w * q.x + x * q.w + y * q.z - z * q.y;
                const float ny = w * q.y - x * q.z + y * q.w + z * q.x;
                const float nz = w * q.z + x * q.y - y * q.x + z * q.w;
                return {nw, nx, ny, nz};
            }

            Quaternion operator*(const float &rhs) const {
                return {w * rhs, x * rhs, y * rhs, z * rhs};
            }

            Quaternion operator+(const Quaternion &rhs) const {
                return {w + rhs.w, x + rhs.x, y + rhs.y, z + rhs.z};
            }

            Quaternion operator-(const Quaternion &rhs) const {
                return {w - rhs.w, x - rhs.x, y - rhs.y, z - rhs.z};
            }

            Quaternion operator-() const {
                return {-w, -x, -y, -z};
            }

            float norm() const {
                return sqrt(w * w + x * x + y * y + z * z);
            }

            Quaternion normalized() const {
                float n = norm();
                return {w / n, x / n, y / n, z / n};
            }

            Quaternion Conjugate() const {
                return {w, -x, -y, -z};
            }

            static float Dot(const Quaternion &q1, const Quaternion &q2) {
                return q1.w * q2.w + q1.x * q2.x + q1.y * q2.y + q1.z * q2.z;
            }

            // 计算夹角
            static float Acos(const float x) {
                if (x < -1.0f) {
                    return M_PI;
                } else if (x > 1.0f) {
                    return 0.0f;
                } else {
                    return acos(x);
                }
            }

            // Slerp方法
            static Quaternion Slerp(const Quaternion &q1, const Quaternion &q2, const float t) {
                Quaternion q3(0, 0, 0, 0);
                float dot = Quaternion::Dot(q1, q2);
                if (dot < 0.0f) {
                    dot = -dot;
                    q3.w = -q2.w;
                    q3.x = -q2.x;
                    q3.y = -q2.y;
                    q3.z = -q2.z;
                } else {
                    q3 = q2;
                }
                if (dot < 0.95f) {
                    const float angle = 2.0f * Quaternion::Acos(dot);
                    const float sinAngle = sinf(angle);
                    const float sin1 = sinf((1.0f - t) * angle) / sinAngle;
                    const float sin2 = sinf(t * angle) / sinAngle;
                    q3.w = (q1.w * sin1 + q3.w * sin2);
                    q3.x = (q1.x * sin1 + q3.x * sin2);
                    q3.y = (q1.y * sin1 + q3.y * sin2);
                    q3.z = (q1.z * sin1 + q3.z * sin2);
                } else {
                    q3.w = q1.w + t * (q3.w - q1.w);
                    q3.x = q1.x + t * (q3.x - q1.x);
                    q3.y = q1.y + t * (q3.y - q1.y);
                    q3.z = q1.z + t * (q3.z - q1.z);
                }
                return q3;
            }
        };

        void SmoothQuaternion(Quaternion &q0, Quaternion &q1, const float threshold) {
            float angle = 2.0f * Quaternion::Acos(Quaternion::Dot(q0, q1));
            int forCount = 0;
            while (angle > threshold) {
                const float t = threshold / angle;
                const Quaternion q2 = Quaternion::Slerp(q0, q1, t);
                q0 = q2;
                angle = 2.0f * Quaternion::Acos(Quaternion::Dot(q0, q1));
                forCount++;
                if (forCount > 50) break;
            }
        }

        Quaternion RotateQuaternion(const Quaternion &q, float angle_degrees, const Vector3 &axis) {
            const float angle_radians = angle_degrees * M_PI / 180.0f;
            const float half_angle = angle_radians * 0.5f;
            const float s = sin(half_angle);
            const Vector3 normalized_axis = axis.normalized();
            const Quaternion q1(cos(half_angle), normalized_axis.x * s, normalized_axis.y * s,
                                normalized_axis.z * s);
            Quaternion q2 = q * q1;
            return q2;
        }

        Vector3 RotateVector(const Quaternion &q, const Vector3 &v) {
            const Quaternion p{0, v.x, v.y, v.z};
            const Quaternion q_inv = q.Conjugate();
            const Quaternion rotated = q * p * q_inv;
            return {rotated.x, rotated.y, rotated.z};
        }

        Vector3 GetFrontPos(const Vector3 &pos, const Quaternion &rot, float distance) {
            const Vector3 v{0, 0, distance};
            const Vector3 v_rotated = RotateVector(rot, v);
            Vector3 pos_front = pos + v_rotated;
            return pos_front;
        }

        Vector3_t GetFrontPos(const Vector3_t &pos, const Quaternion_t &rot, float distance) {
            const Vector3 vPos{pos.x, pos.y, pos.z};
            const Quaternion vQos{rot.w, rot.x, rot.y, rot.z};
            const auto ret = GetFrontPos(vPos, vQos, distance);
            return Vector3_t{ret.x, ret.y, ret.z};
        }

        float getRaceCamFov() {
            return raceDefaultFOV;
        }

        void reset_camera() {
            cameraPos = Vector3_t{-51.72, 50, 108.57};

            raceFirstPersonLookAtOffset = Vector3_t{0, 0, 0};
            raceFirstShakeStrength = 0;
        }

        void setCameraType(int value) {
            if (cameraType == value) return;

            cameraType = value;
            reset_camera();
        }

        Vector3_t getCameraPos() {
            return cameraPos;
        }

        Quaternion_t slerpTwo(Quaternion_t &rot, Quaternion_t &rot2, float t) {
            return Camera::Quaternion::Slerp(rot, rot2, t);
        }

        Quaternion_t updateLookAtByRotation(Quaternion_t rot) {
            auto newRot = Camera::RotateQuaternion(rot, raceFirstPersonLookAtOffset.y,
                                                   Camera::Vector3(1, 0, 0));
            rot = Camera::RotateQuaternion(newRot, raceFirstPersonLookAtOffset.x,
                                           Camera::Vector3(0, 1, 0));

            return rot;
        }

        Quaternion_t updatePosAndLookAtByRotation(Vector3_t pos, Quaternion_t rot) {
            cameraPos.x = pos.x;
            cameraPos.y = pos.y;
            cameraPos.z = pos.z;
            return updateLookAtByRotation(rot);
        }

        Camera::Vector3 lastPos(0, 0, 0);

        Camera::Quaternion lastRot = Camera::Quaternion(1, 1, 1, 1);

        Quaternion_t
        updatePosAndLookAtByRotationStable(Vector3_t pos, Quaternion_t rot) {
            auto rotateQuat = Camera::Quaternion(rot);
            Camera::SmoothQuaternion(rotateQuat, lastRot, 0.01f);

            auto newRt = Camera::RotateQuaternion(rotateQuat,
                                                  pos.y - lastPos.y > 0 ? -raceFirstShakeStrength
                                                                        : raceFirstShakeStrength,
                                                  Camera::Vector3(1, 0, 0));
            rotateQuat.w = newRt.w;
            rotateQuat.x = newRt.x;
            rotateQuat.y = newRt.y;
            rotateQuat.z = newRt.z;

            lastRot.w = rotateQuat.w;
            lastRot.x = rotateQuat.x;
            lastRot.y = rotateQuat.y;
            lastRot.z = rotateQuat.z;

            lastPos = pos;
            return updatePosAndLookAtByRotation(pos, static_cast<Quaternion_t>(rotateQuat));
        }
    }
}
