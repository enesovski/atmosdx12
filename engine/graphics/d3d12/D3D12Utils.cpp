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
