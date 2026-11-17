#include "graphics/d3d12/ShaderCompiler.h"
#include "core/Build.h"
#include "core/Log.h"
#include "core/StringUtils.h"

#include <string>
#include <vector>

namespace atmos
{
    namespace
    {
        const wchar_t* GetTargetProfile(ShaderStage stage)
        {
            switch (stage)
            {
                case ShaderStage::Vertex:  return L"vs_6_6";
                case ShaderStage::Pixel:   return L"ps_6_6";
                case ShaderStage::Compute: return L"cs_6_6";
            }

            return L"";
        }
    }

    ShaderCompiler::ShaderCompiler(std::filesystem::path shaderDirectory)
        : shaderDirectory_(std::move(shaderDirectory))
    {
        ThrowIfFailed(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils_)), "Failed to create DXC utils");
        ThrowIfFailed(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler_)),
                      "Failed to create DXC compiler");
        ThrowIfFailed(utils_->CreateDefaultIncludeHandler(&includeHandler_),
                      "Failed to create DXC include handler");
    }

    ComPtr<IDxcBlob> ShaderCompiler::Compile(std::wstring_view fileName, std::wstring_view entryPoint,
                                             ShaderStage stage) const
    {
        const std::filesystem::path path = shaderDirectory_ / fileName;
        const std::string displayName = ToUtf8(path.filename().wstring());

        ComPtr<IDxcBlobEncoding> source;
        ThrowIfFailed(utils_->LoadFile(path.c_str(), nullptr, &source),
                      "Failed to load shader " + displayName);

        const std::wstring entry(entryPoint);
        const std::wstring includeDirectory = shaderDirectory_.wstring();

        std::vector<const wchar_t*> arguments = {
            path.c_str(),
            L"-E", entry.c_str(),
            L"-T", GetTargetProfile(stage),
            L"-I", includeDirectory.c_str(),
            L"-HV", L"2021",
            DXC_ARG_PACK_MATRIX_ROW_MAJOR,
            DXC_ARG_WARNINGS_ARE_ERRORS,
        };

        if constexpr (IsDebugBuild)
        {
            arguments.insert(arguments.end(), { DXC_ARG_DEBUG, L"-Qembed_debug", DXC_ARG_SKIP_OPTIMIZATIONS });
        }
        else
        {
            arguments.push_back(DXC_ARG_OPTIMIZATION_LEVEL3);
        }

        const DxcBuffer buffer{ source->GetBufferPointer(), source->GetBufferSize(), DXC_CP_ACP };

        ComPtr<IDxcResult> result;
        ThrowIfFailed(compiler_->Compile(&buffer, arguments.data(), static_cast<UINT32>(arguments.size()),
                                         includeHandler_.Get(), IID_PPV_ARGS(&result)),
                      "Failed to invoke DXC for " + displayName);

        ComPtr<IDxcBlobUtf8> diagnostics;
        result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&diagnostics), nullptr);
        const bool hasDiagnostics = diagnostics && diagnostics->GetStringLength() > 0;

        HRESULT status = S_OK;
        result->GetStatus(&status);

        if (FAILED(status))
        {
            throw std::runtime_error("Shader compilation failed: " + displayName + "\n" +
                                     (hasDiagnostics ? diagnostics->GetStringPointer() : ""));
        }

        if (hasDiagnostics)
            log::Warning("Shader {}:\n{}", displayName, diagnostics->GetStringPointer());

        ComPtr<IDxcBlob> bytecode;
        ThrowIfFailed(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&bytecode), nullptr),
                      "Failed to get shader bytecode for " + displayName);

        return bytecode;
    }
}
