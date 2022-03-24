#pragma once

#include "Camera.h"

#include "glm/include/glm/mat4x4.hpp"
#include <type_traits>

struct Light
{
    glm::vec3 Position;
    glm::vec3 Direction;
    glm::vec3 Radiance;
    bool Enabel = false;
};

struct GLFWwindow;

    struct ViewSettings
    {
        float Width;
        float Height;
        float distance;
        float fov;
    };

    struct SceneSettings
    {
        float pitch = 0.0f;
        float yaw = 0.0f;

        static const int NumLights = 3;
        Light Lights[NumLights];
    };

    class RendererInterface
    {
    public:

        Camera mCamera;
        virtual ~RendererInterface() = default;

        virtual GLFWwindow* initialize(int Width, int Height, int MaxSamples) = 0;
        virtual void ShutDown() = 0;
        virtual void Setup(const ViewSettings& view, const SceneSettings& Scene) = 0;
        virtual void Update(const float DeltaTime) = 0;
        virtual void Render(GLFWwindow* Window,const float DeltaTime) = 0;

        virtual void SetLight() = 0;
    };


