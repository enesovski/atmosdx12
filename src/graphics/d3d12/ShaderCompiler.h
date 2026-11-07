#pragma once

#include <d3dcompiler.h>
#include <wrl/client.h>

#include <filesystem>

namespace ShaderCompiler
{
    Microsoft::WRL::ComPtr<ID3DBlob> Compile(const std::filesystem::path& path,
                                          const char* entryPoint, const char* target);
}
