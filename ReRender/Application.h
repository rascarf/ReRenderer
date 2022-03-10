#pragma once

#include <memory>

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

        void run(const std::unique_ptr<RendererInterface>& Renderer);

    private:
        static void MousePositionCallback(GLFWwindow* Window, double Xpos, double Ypos);
        static void MouseButtonCallback(GLFWwindow* Window, int button, int action, int mods);
        static void MouseScrollCallback(GLFWwindow* Window, double XOffset, double YOffset);
        static void KeyCallback(GLFWwindow* Window, int Key,int Scancode, int Action, int Mods);

        GLFWwindow* m_window;
        double m_PrevCursorX;
        double m_PrevCursorY;

        ViewSettings m_ViewSettings;
        SceneSettings m_SceneSettings;

        InputMode m_Mode;
    };

