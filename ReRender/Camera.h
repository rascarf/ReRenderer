#pragma once
#include <vector>

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/include/glm/glm.hpp"

using Mat4 = glm::mat4x4;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;

class Camera
{
public:
    Camera();
    ~Camera();

    //World Camera Position
    Vec3 GetPosition() const;
    void SetPosition(float x, float y, float z);

    //Camaera Basis Vectors
    Vec3 GetRight() const;
    Vec3 GetUp() const;
    Vec3 GetLook() const;

    //Get Frustum Properties
    float GetNearZ() const;
    float GetFarZ() const;
    float GetAspect() const;
    float GetFovY() const;
    float GetFovX() const;

    // Get near and far plane dimensions in view space coordinates.
    float GetNearWindowWidth()const;
    float GetNearWindowHeight()const;
    float GetFarWindowWidth()const;
    float GetFarWindowHeight()const;

    // Set frustum.
    void SetLens(float fovY, float Width, float Height, float zn, float zf);

    Mat4 GetView();
    Mat4 GetProj();
    Mat4 GetReversedProj();

    // Strafe/Walk/Ascend the camera a distance d.
    void Strafe(float d);
    void Walk(float d);
    void Ascend(float d);

    // Rotate the camera.
    void Rotate(float Pitch,float Yaw);
    Mat4 GetRotation();

    // After modifying camera position/orientation, call to rebuild the view matrix.
    void UpdateViewMatrix();

private:
    Vec3 mPosition = {0,0,-10};
    Vec3 mRight = { 1.0f, 0.0f, 0.0f };
    Vec3 mUp = { 0.0f, 1.0f, 0.0f };
    Vec3 mLook = { 0, 0 , 1 };
    Vec3 WorldUp = { 0.0f,1.0f,0.0f };

    Mat4 mView;
    Mat4 mProj;

    Mat4 mViewRotation;

    //frustum properties
    float mNearZ = 0.0f;
    float mFarZ = 0.0f;
    float mAspect = 0.0f;
    float mFovY = 0.0f;

    float mNearWindowHeight = 0.0f;
    float mFarWindowHeight = 0.0f;

    float Pitch;
    float Yaw;
};

