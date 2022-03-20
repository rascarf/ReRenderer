#pragma once

#include <memory>

#include "D3D12Renderer.h"
#include "Renderer.h"

    enum class InputMode
    {
        None,
        RotatingView,
        RotatingScene,
    };

    class Application
    {
    public:
        Application();
        ~Application();

        inline static std::unique_ptr<RendererInterface>  mRenderer = std::make_unique<D3D12Renderer>();

        void run();

    private:
        static void MousePositionCallback(GLFWwindow* Window, double Xpos, double Ypos);
        static void MouseButtonCallback(GLFWwindow* Window, int button, int action, int mods);
        static void MouseScrollCallback(GLFWwindow* Window, double XOffset, double YOffset);
        static void KeyCallback(GLFWwindow* Window, int Key,int Scancode, int Action, int Mods);

        GLFWwindow* m_window;
        double m_PrevCursorX;
        double m_PrevCursorY;

        inline static float DeltaTime = 0.0f;
        inline static float LastTime = 0.0f;

        ViewSettings m_ViewSettings;
        SceneSettings m_SceneSettings;

        InputMode m_Mode;
    };

