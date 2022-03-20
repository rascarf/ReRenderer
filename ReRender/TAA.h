#pragma once
#include <glm/include/glm/detail/type_mat.hpp>
#include <glm/include/glm/glm.hpp>

using Mat4 = glm::mat4;

struct TaaCB
{
    Mat4 PreViewPorjectionMatrix;
    Mat4 PreSkyProjectionMatrix;
    Mat4 PreSceneRotationMatix;
};

class TAA
{
public:
    static  void SetProjectionJitter(int Width, int Height, int& SampleIndex);
    static void  SetProjectionJitter(float JitterX, float JitterY, Mat4& Projection);
};

