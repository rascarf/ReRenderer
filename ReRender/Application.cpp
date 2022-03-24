#include <stdexcept>
#include <glfw/include/GLFW/glfw3.h>


#include "Application.h"

    const int DisplaySizeX = 1024;
    const int DisplaySizeY = 1024;
    const int DisplaySamples = 16;

    const float ViewDistance = 150.0f;
    const float ViewFOV = 45.0f;
    const float OrbitSpeed = 1.0f;
    const float ZoomSpeed = 4.0f;

	const float Velocity = 100.0f;

    Application::Application()
        :m_window(nullptr)
        ,m_PrevCursorX(0.0)
        ,m_PrevCursorY(0.0)
        ,m_Mode(InputMode::None)
    {

        if(!glfwInit())
        {
            throw std::runtime_error("Failed to initialize GLFW library");
        }

        m_ViewSettings.distance = ViewDistance;
        m_ViewSettings.fov = ViewFOV;

		m_ViewSettings.Width = DisplaySizeX;
		m_ViewSettings.Height = DisplaySizeY;


        m_SceneSettings.Lights[0].Direction = glm::normalize(glm::vec3{ -1.0f,  0.0f, 0.0f });
        m_SceneSettings.Lights[1].Direction = glm::normalize(glm::vec3{ 1.0f,  0.0f, 0.0f });
        m_SceneSettings.Lights[2].Direction = glm::normalize(glm::vec3{ 0.0f, -1.0f, 0.0f });
    }

    Application::~Application()
    {
        if(m_window)
        {
            glfwDestroyWindow(m_window);
        }

        glfwTerminate();
    }

    void Application::run()
    {
        glfwWindowHint(GLFW_RESIZABLE, 0);

        m_window = mRenderer->initialize(DisplaySizeX, DisplaySizeY, DisplaySamples);

        glfwSetWindowUserPointer(m_window, this);
        glfwSetCursorPosCallback(m_window, Application::MousePositionCallback);
        glfwSetMouseButtonCallback(m_window, Application::MouseButtonCallback);
        glfwSetScrollCallback(m_window, Application::MouseScrollCallback);
        glfwSetKeyCallback(m_window, Application::KeyCallback);

		mRenderer->Setup(m_ViewSettings, m_SceneSettings);

        while(!glfwWindowShouldClose(m_window))
        {
			float CurrentTime = static_cast<float>(glfwGetTime());
			DeltaTime = CurrentTime - LastTime;
			LastTime = CurrentTime;

			mRenderer->Update(DeltaTime);
			mRenderer->Render(m_window,DeltaTime);
            glfwPollEvents();
        }

		mRenderer->ShutDown();
    }

	void Application::MousePositionCallback(GLFWwindow* window, double xpos, double ypos)
	{
		Application* self = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
		if (self->m_Mode != InputMode::None) 
		{
			const double dx = xpos - self->m_PrevCursorX;
			const double dy = self->m_PrevCursorY - ypos;

			switch (self->m_Mode)
		    {
			    case InputMode::RotatingScene:
				    self->m_SceneSettings.yaw += OrbitSpeed * float( - dx);
				    self->m_SceneSettings.pitch += OrbitSpeed * float(dy);
				    break;
			    case InputMode::RotatingView:
				    self->mRenderer->mCamera.Rotate(OrbitSpeed * float(dy), OrbitSpeed * float(dx));
				    break;
			}

			self->m_PrevCursorX = xpos;
			self->m_PrevCursorY = ypos;
		}
	}

	void Application::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
	{
		Application* self = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));

		const InputMode oldMode = self->m_Mode;
		if (action == GLFW_PRESS && self->m_Mode == InputMode::None) 
		{
			switch (button)
		    {
			case GLFW_MOUSE_BUTTON_1:
				self->m_Mode = InputMode::RotatingView;
				break;
			case GLFW_MOUSE_BUTTON_2:
				self->m_Mode = InputMode::RotatingScene;
				break;
			}
		}

		if (action == GLFW_RELEASE && (button == GLFW_MOUSE_BUTTON_1 || button == GLFW_MOUSE_BUTTON_2)) 
		{
			self->m_Mode = InputMode::None;
		}

		if (oldMode != self->m_Mode) 
		{
			if (self->m_Mode == InputMode::None) 
			{
				glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			}
			else {
				glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
				glfwGetCursorPos(window, &self->m_PrevCursorX, &self->m_PrevCursorY);
			}
		}
	}

	void Application::MouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
	{
		Application* self = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
		self->m_ViewSettings.fov += ZoomSpeed * float(-yoffset);
	}

	void Application::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
	{
		auto Offset = Velocity * DeltaTime;
		Application* self = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));

		if (action == GLFW_PRESS || action == GLFW_REPEAT) 
		{
			Light* light = nullptr;

			switch (key)
		    {
			case GLFW_KEY_F1:
				light = &self->m_SceneSettings.Lights[0];
				break;
			case GLFW_KEY_F2:
				light = &self->m_SceneSettings.Lights[1];
				break;
			case GLFW_KEY_F3:
				light = &self->m_SceneSettings.Lights[2];
				break;

			case GLFW_KEY_W:
				mRenderer->mCamera.Walk(Offset);
				break;
			case GLFW_KEY_S:
				mRenderer->mCamera.Walk(-Offset);
				break;
			case GLFW_KEY_A:
				mRenderer->mCamera.Strafe(-Offset);
				break;
			case GLFW_KEY_D:
				mRenderer->mCamera.Strafe(Offset);
				break;
			case GLFW_KEY_J:
				mRenderer->SetLight();
			}

			if (light) 
			{
				light->Enabel = !light->Enabel;
			}
		}
	}


