#pragma once

#include "graphics/d3d12/D3D12Utils.h"

#include <dxcapi.h>

#include <filesystem>
#include <string_view>

namespace atmos
{
    enum class ShaderStage
    {
        Vertex,
        Pixel,
        Compute,
    };

    // DXC front end targeting Shader Model 6.6. Matrices are packed row-major (-Zpr),
    // so HLSL uses mul(vector, matrix) exactly like DirectXMath.
    class ShaderCompiler
    {
    public:
        explicit ShaderCompiler(std::filesystem::path shaderDirectory);

        // `fileName` is relative to the shader directory, which is also the include root.
        ComPtr<IDxcBlob> Compile(std::wstring_view fileName, std::wstring_view entryPoint,
                                 ShaderStage stage) const;

    private:
        std::filesystem::path shaderDirectory_;
        ComPtr<IDxcUtils> utils_;
        ComPtr<IDxcCompiler3> compiler_;
        ComPtr<IDxcIncludeHandler> includeHandler_;
    };
}
