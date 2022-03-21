#pragma once
#include <d3dcommon.h>
#include <d3dcompiler.h>
#include <stdexcept>
#include <string>
#include <wrl/client.h>

#include "Utils.h"

class Shader
{
public:
    static Microsoft::WRL::ComPtr<ID3DBlob> compileShader(const std::string& filename, const std::string& entryPoint, const std::string& profile)
    {
        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if  _DEBUG
        flags |= D3DCOMPILE_DEBUG;
        flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        Microsoft::WRL::ComPtr<ID3DBlob> Shader;
        Microsoft::WRL::ComPtr<ID3DBlob> ErrorBlob;

        std::printf("Compiling HLSL shader: %s [%s]\n", filename.c_str(), entryPoint.c_str());

        if (FAILED(D3DCompileFromFile(
            Utils::convertToUTF16(filename).c_str(),
            nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
            entryPoint.c_str(),
            profile.c_str(),
            flags,
            0,
            &Shader,
            &ErrorBlob)))
        {
            std::string ErrorMsg = "Shader compilation failed: " + filename;
            if (ErrorBlob)
            {
                ErrorMsg += std::string("\n") + static_cast<const char*>(ErrorBlob->GetBufferPointer());
            }

            throw std::runtime_error(ErrorMsg);
        }
        return Shader;
    }
};

