#include "Utils.h"

#include <cassert>
#include <fstream>
#include <sstream>
#include <memory>

#if _WIN32
#include <Windows.h>
#endif


std::string File::ReadText(const std::string& FileName)
{
    std::ifstream file{ FileName };
    if(!file.is_open())
    {
        throw std::runtime_error("Couldn't open file" + FileName);
    }

    std::stringstream Buffer;
    Buffer << file.rdbuf();
    return Buffer.str();
}

std::vector<char> File::ReadBinary(const std::string& FileName)
{
    std::ifstream file{ FileName,std::ios::binary | std::ios::ate };
    if (!file.is_open())
    {
        throw std::runtime_error("Couldn't open file" + FileName);
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char>buffer(size);
    file.read(buffer.data(), size);

    return buffer;
}

std::string Utils::convertToUTF8(const std::wstring& wstr)
{
    const int bufferSize = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    const std::unique_ptr<char[]> buffer(new char[bufferSize]);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, buffer.get(), bufferSize, nullptr, nullptr);
    return std::string(buffer.get());
}

std::wstring Utils::convertToUTF16(const std::string& str)
{
    const int bufferSize = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    const std::unique_ptr<wchar_t[]> buffer(new wchar_t[bufferSize]);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, buffer.get(), bufferSize);
    return std::wstring(buffer.get());
}

namespace Re
{
    void SetName(ID3D12Object* pObj, const char* name)
    {
        if (name != NULL)
        {
            SetName(pObj, std::string(name));
        }
    }

    void SetName(ID3D12Object* pObj, const std::string& name)
    {
        assert(pObj != NULL);

        wchar_t NameBuffer[128];

        // Truncate the string if it's too big (keep the tail as it likely has the most useful information - some name have full paths)
        if (name.size() >= 128)
            swprintf(NameBuffer, 128, L"%S", name.substr(name.size() - 127, name.size()).c_str());
        else
            swprintf(NameBuffer, name.size() + 1, L"%S", name.c_str());

        pObj->SetName(NameBuffer);
    }
}

