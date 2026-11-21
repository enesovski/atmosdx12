#pragma once

#include <d3d12.h>
#include <wrl/client.h>

#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>

namespace atmos
{
    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    class DxException : public std::runtime_error
    {
    public:
        DxException(HRESULT result, const std::string& message)
            : std::runtime_error(message), result_(result) {}

        HRESULT GetResult() const { return result_; }
        bool IsDeviceLost() const;

    private:
        HRESULT result_;
    };

    [[noreturn]] void ThrowDxError(HRESULT result, std::string_view what,
                                   const std::source_location& location);

    inline void ThrowIfFailed(HRESULT result, std::string_view what,
                              const std::source_location& location = std::source_location::current())
    {
        if (FAILED(result))
            ThrowDxError(result, what, location);
    }

    // Names show up in PIX, debug-layer messages and DRED reports.
    void SetDebugName(ID3D12Object* object, std::wstring_view name);
}

namespace atmos
{
    // Short name of the formats the engine uses ("RGBA16F"); "other" for the rest.
    const char* GetFormatName(DXGI_FORMAT format);
}
