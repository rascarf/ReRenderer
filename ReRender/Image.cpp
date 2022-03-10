#include "Image.h"

#include <stdexcept>
#include <stb/include/stb_image.h>

Image::Image()
    :m_Width(0)
    ,m_Height(0)
    ,m_Channels(0)
    ,m_Hdr(false)
{}

std::shared_ptr<Image> Image::FromFile(const std::string& FileName, int Channels)
{
    std::printf("Loading Image:%s\n", FileName.c_str());

    std::shared_ptr<Image>image{ new Image };

    if(stbi_is_hdr(FileName.c_str()))
    {
        float* Pixels = stbi_loadf(FileName.c_str(), &image->m_Width, &image->m_Height, &image->m_Channels, Channels);

        if(Pixels)
        {
            image->m_Pixels.reset(reinterpret_cast<unsigned char*>(Pixels));
            image->m_Hdr = true;
        }
    }
    else
    {
        unsigned char* Pixels = stbi_load(FileName.c_str(), &image->m_Width, &image->m_Height, &image->m_Channels, Channels);

        if(Pixels)
        {
            image->m_Pixels.reset(Pixels);
            image->m_Hdr = false;
        }
    }

    if(Channels > 0)
    {
        image->m_Channels = Channels;
    }

    if(!image->m_Pixels)
    {
        throw std::runtime_error("Failed to load image file :" + FileName);
    }

    return image;
}

