#pragma once
#include <cassert>
#include <memory>
#include <string>


class Image
{
public:
    static std::shared_ptr<Image> FromFile(const std::string& FileName, int Channels = 4);

    int Width() const { return m_Width; }
    int Height() const { return m_Height; }
    int Channels() const { return m_Channels; }
    bool IsHdr() const { return m_Hdr; }
    int Pitch() const { return m_Width * BytesPerPixel(); }

    template<typename T>
    const T* Pixels() const
    {
        return reinterpret_cast<const T*>(m_Pixels.get());
    }

    int BytesPerPixel() const
    {
        return m_Channels * (m_Hdr ? sizeof (float) : sizeof(unsigned char));
    }

private:
    Image();

    int m_Width;
    int m_Height;
    int m_Channels;
    bool m_Hdr;
    std::unique_ptr<unsigned char>m_Pixels;


};

