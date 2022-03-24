#include "Camera.h"

#include <glm/include/glm/gtc/matrix_transform.hpp>
#include <glm/include/glm/gtx/euler_angles.hpp>

Camera::Camera()
{
}

Camera::~Camera()
{
}

Vec3 Camera::GetPosition() const
{
    return mPosition;
}

void Camera::SetPosition(float x, float y, float z)
{
    mPosition[0] = x;
    mPosition[1] = y;
    mPosition[2] = z;

    UpdateViewMatrix();
}

Vec3 Camera::GetRight() const
{
    return mRight;
}

Vec3 Camera::GetUp() const
{
    return mUp;
}

Vec3 Camera::GetLook() const
{
    return mLook;
}

float Camera::GetNearZ() const
{
    return mNearZ;
}

float Camera::GetFarZ() const
{
    return mFarZ;
}

float Camera::GetAspect() const
{
    return mAspect;
}

float Camera::GetFovY() const
{
    return mFovY;
}

float Camera::GetFovX() const
{
    float halfWidth = 0.5f * GetNearWindowWidth();
    return 2.0f * atan(halfWidth / mNearZ);
}

float Camera::GetNearWindowWidth() const
{
    return mAspect * mNearWindowHeight;
}

float Camera::GetNearWindowHeight() const
{
    return mNearWindowHeight;
}

float Camera::GetFarWindowWidth() const
{
    return mAspect * mFarWindowHeight;
}

float Camera::GetFarWindowHeight() const
{
    return mFarWindowHeight;
}

void Camera::SetLens(float fovY, float Width ,float Height, float zn, float zf)
{
    // cache properties
    mFovY = fovY;
    mAspect = Width / Height;
    mNearZ = zn;
    mFarZ = zf;

    mNearWindowHeight = 2.0f * mNearZ * tanf(0.5f * mFovY);
    mFarWindowHeight = 2.0f * mFarZ * tanf(0.5f * mFovY);

    Mat4 P;
    P = glm::perspectiveFovRH_ZO(mFovY, Width, Height, mNearZ, mFarZ);
    // P = glm::orthoRH_ZO(-512.0f, 512.0f, -512.0f, 512.0f, 0.1f, 1000.0f);

    mProj = P;
}

Mat4 Camera::GetView()
{
    return glm::lookAt(mPosition, mPosition + mLook, mUp);
}

Mat4 Camera::GetProj()
{
    return mProj;
}


void Camera::Strafe(float d)
{
    mPosition[0] = d * mRight[0] + mPosition[0];
    mPosition[1] = d * mRight[1] + mPosition[1];
    mPosition[2] = d * mRight[2] + mPosition[2];

    UpdateViewMatrix();
}

void Camera::Walk(float d)
{
    // mPosition += d * mLook
    mPosition[0] = d * mLook[0] + mPosition[0];
    mPosition[1] = d * mLook[1] + mPosition[1];
    mPosition[2] = d * mLook[2] + mPosition[2];

    UpdateViewMatrix();
}

void Camera::Ascend(float d)
{
    // mPosition += d * mUp
    mPosition[0] = d * mUp[0] + mPosition[0];
    mPosition[1] = d * mUp[1] + mPosition[1];
    mPosition[2] = d * mUp[2] + mPosition[2];

    UpdateViewMatrix();
}

void Camera::Rotate(float DPitch, float DYaw)
{
    Pitch += DPitch;
    Yaw += DYaw;

    if (Pitch > 89.0f)
        Pitch = 89.0f;
    if (Pitch < -89.0f)
        Pitch = -89.0f;

    UpdateViewMatrix();
}

Mat4 Camera::GetRotation()
{
    Mat4 ViewRotationMatrix = glm::eulerAngleXY(-glm::radians(Pitch),glm::radians(Yaw));
    return ViewRotationMatrix;
}

void Camera::UpdateViewMatrix()
{
    glm::vec3 front;

    front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    front.y = sin(glm::radians(Pitch));
    front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));


    mLook = glm::normalize(front);
    mRight = glm::normalize(glm::cross(mLook, WorldUp)); 
    mUp = glm::normalize(glm::cross(mRight, mLook));
}








