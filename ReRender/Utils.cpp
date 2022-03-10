#include "Utils.h"
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

