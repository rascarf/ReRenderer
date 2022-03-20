#include<cstdio>
#include <string>
#include <memory>
#include <iostream>
#include <glfw/include/GLFW/glfw3.h>

#include "Application.h"
#include "D3D12Renderer.h"
#include "Renderer.h"
int main()
{
    Application app;

    try
    {
        app.run();
    }

    catch (std::runtime_error e)
    {
        std::cout << e.what();
    }
  
    
}
