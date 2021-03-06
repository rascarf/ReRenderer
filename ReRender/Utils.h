#pragma once
#include <string>
#include <vector>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

#define SetDebugName(Object,Name) Re::SetName(Object, std::string(#Name).c_str());

class File
{
public:
    static std::string ReadText(const std::string& FileName);
    static std::vector<char> ReadBinary(const std::string& FileName);
};


class Utils
{
public:

    template<typename T>
    static constexpr T numMipmapLevels(T Width,T Height)
    {
        T levels = 1;
        while((Width|Height) >> levels)
        {
            ++levels;
        }
        return levels;
    }

    template<typename T>
    static constexpr bool IsPowerOfTwo(T Value)
    {
        return Value != 0 && (Value & (Value - 1)) == 0;
    }

    template<typename T>
    static constexpr T roundToPowerOfTwo(T Value,int POT)
    {
        return (Value + POT - 1) & -POT;
    }

	static std::string convertToUTF8(const std::wstring& wstr);
	static std::wstring convertToUTF16(const std::string& str);

};

namespace  Re
{
    void SetName(ID3D12Object* pObj, const char* name);
    void SetName(ID3D12Object* pObj, const std::string& name);
}


