#include "ShaderCompiler.h"

#include <stdexcept>
#include <string>

Microsoft::WRL::ComPtr<ID3DBlob> ShaderCompiler::Compile(
    const std::filesystem::path& path, const char* entryPoint, const char* target)
{
    Microsoft::WRL::ComPtr<ID3DBlob> shader;
    Microsoft::WRL::ComPtr<ID3DBlob> errors;

    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;

#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    const HRESULT result = D3DCompileFromFile(
        path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entryPoint, target, flags, 0, &shader, &errors);

    if (FAILED(result))
    {
        std::string message = "Failed to compile shader: " + path.string();

        if (errors)
        {
            message += "\n";
            message.append(static_cast<const char*>(errors->GetBufferPointer()),
                           errors->GetBufferSize());
        }

        throw std::runtime_error(message);
    }

    return shader;
}
