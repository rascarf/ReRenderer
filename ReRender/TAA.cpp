#include "TAA.h"

#include <cmath>


void TAA::SetProjectionJitter(int Width, int Height, int& SampleIndex)
{
    static const auto CalculateHaltonNumber = [](int Index, int Base)
    {
        float f = 1.0, result = 0.0f;

        for (int i = Index; i > 0;)
        {
            f /= static_cast<float>(Base);
            result = result + f * static_cast<float>(i % Base);
            i = static_cast<int>(floor(static_cast<float>(i) / static_cast<float>(Base)));
        }

        return result;
    };

    //16x TAA
    SampleIndex = (SampleIndex + 1) % 16;

    float JitterX = 2.0f * CalculateHaltonNumber(SampleIndex + 1, 2) - 1.0f;
    float JitterY = 2.0f * CalculateHaltonNumber(SampleIndex + 1, 3) - 1.0f;

    JitterX /= static_cast<float>(Width);
    JitterY /= static_cast<float>(Height);
}

void TAA::SetProjectionJitter(float JitterX, float JitterY, Mat4& Projection)
{
    Projection[2][0] = JitterX;
    Projection[2][1] = JitterY;
}
