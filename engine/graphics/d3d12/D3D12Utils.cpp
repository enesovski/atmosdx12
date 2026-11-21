#include "graphics/d3d12/D3D12Utils.h"

#include <cstdint>
#include <filesystem>
#include <format>

namespace atmos
{
    bool DxException::IsDeviceLost() const
    {
        return result_ == DXGI_ERROR_DEVICE_REMOVED ||
               result_ == DXGI_ERROR_DEVICE_RESET ||
               result_ == DXGI_ERROR_DEVICE_HUNG ||
               result_ == DXGI_ERROR_DRIVER_INTERNAL_ERROR;
    }

    void ThrowDxError(HRESULT result, std::string_view what, const std::source_location& location)
    {
        const std::string file = std::filesystem::path(location.file_name()).filename().string();

        throw DxException(result, std::format("{} (HRESULT {:#010x}) [{}:{}]", what,
                                              static_cast<uint32_t>(result), file, location.line()));
    }

    void SetDebugName(ID3D12Object* object, std::wstring_view name)
    {
        if (object && !name.empty())
            object->SetName(std::wstring(name).c_str());
    }
}

namespace atmos
{
    const char* GetFormatName(DXGI_FORMAT format)
    {
        switch (format)
        {
            case DXGI_FORMAT_R8G8B8A8_UNORM:      return "RGBA8";
            case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return "RGBA8 sRGB";
            case DXGI_FORMAT_R10G10B10A2_UNORM:   return "RGB10A2";
            case DXGI_FORMAT_R16G16B16A16_FLOAT:  return "RGBA16F";
            case DXGI_FORMAT_R16G16_FLOAT:        return "RG16F";
            case DXGI_FORMAT_R32_FLOAT:           return "R32F";
            case DXGI_FORMAT_R8_UNORM:            return "R8";
            case DXGI_FORMAT_D32_FLOAT:           return "D32F";
            case DXGI_FORMAT_BC1_UNORM:           return "BC1";
            case DXGI_FORMAT_BC1_UNORM_SRGB:      return "BC1 sRGB";
            case DXGI_FORMAT_BC3_UNORM:           return "BC3";
            case DXGI_FORMAT_BC3_UNORM_SRGB:      return "BC3 sRGB";
            case DXGI_FORMAT_BC5_UNORM:           return "BC5";
            default:                              return "other";
        }
    }
}
