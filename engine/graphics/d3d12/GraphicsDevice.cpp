#include "graphics/d3d12/GraphicsDevice.h"
#include "graphics/d3d12/BindlessRootSignature.h"
#include "graphics/d3d12/GraphicsConfig.h"
#include "core/Log.h"
#include "core/StringUtils.h"

#include <cstdint>
#include <vector>

namespace atmos
{
    namespace
    {
        constexpr D3D_FEATURE_LEVEL MinimumFeatureLevel = D3D_FEATURE_LEVEL_12_0;

        // Debug layer, GPU validation and DRED must be configured before the device exists.
        void EnableDebugFeatures(const GraphicsDeviceOptions& options, UINT& factoryFlags)
        {
            if (options.enableDebugLayer || options.enableGpuValidation)
            {
                ComPtr<ID3D12Debug1> debug;

                if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
                {
                    debug->EnableDebugLayer();
                    debug->SetEnableGPUBasedValidation(options.enableGpuValidation);
                    factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
                }
                else
                {
                    log::Warning("D3D12 debug layer unavailable; install the Graphics Tools feature");
                }
            }

            if (options.enableDred)
            {
                ComPtr<ID3D12DeviceRemovedExtendedDataSettings> dred;

                if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dred))))
                {
                    dred->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
                    dred->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
                }
            }
        }

        ComPtr<IDXGIFactory6> CreateFactory(const GraphicsDeviceOptions& options)
        {
            UINT flags = 0;
            EnableDebugFeatures(options, flags);

            ComPtr<IDXGIFactory6> factory;
            ThrowIfFailed(CreateDXGIFactory2(flags, IID_PPV_ARGS(&factory)), "Failed to create DXGI factory");
            return factory;
        }

        ComPtr<IDXGIAdapter1> SelectAdapter(IDXGIFactory6* factory)
        {
            for (UINT i = 0;; ++i)
            {
                ComPtr<IDXGIAdapter1> adapter;
                const HRESULT result = factory->EnumAdapterByGpuPreference(
                    i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter));

                if (result == DXGI_ERROR_NOT_FOUND)
                    break;

                ThrowIfFailed(result, "Failed to enumerate GPU adapters");

                DXGI_ADAPTER_DESC1 desc{};
                ThrowIfFailed(adapter->GetDesc1(&desc), "Failed to get adapter description");

                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                    continue;

                if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), MinimumFeatureLevel,
                                                __uuidof(ID3D12Device), nullptr)))
                {
                    log::Info("GPU: {} ({} MB VRAM)", ToUtf8(desc.Description),
                              desc.DedicatedVideoMemory / (1024 * 1024));
                    return adapter;
                }
            }

            throw std::runtime_error("No hardware Direct3D 12 adapter found");
        }

        // Bindless rendering needs SM 6.6 dynamic resources and resource binding tier 3.
        void CheckRequiredFeatures(ID3D12Device* device)
        {
            D3D12_FEATURE_DATA_SHADER_MODEL shaderModel{ D3D_SHADER_MODEL_6_6 };

            if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel,
                                                   sizeof(shaderModel))) ||
                shaderModel.HighestShaderModel < D3D_SHADER_MODEL_6_6)
            {
                throw std::runtime_error("GPU or driver does not support Shader Model 6.6");
            }

            D3D12_FEATURE_DATA_D3D12_OPTIONS options{};
            ThrowIfFailed(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options)),
                          "Failed to query D3D12 options");

            if (options.ResourceBindingTier < D3D12_RESOURCE_BINDING_TIER_3)
                throw std::runtime_error("GPU does not support resource binding tier 3");
        }

        ComPtr<ID3D12Device> CreateDevice(IDXGIAdapter1* adapter)
        {
            ComPtr<ID3D12Device> device;
            ThrowIfFailed(D3D12CreateDevice(adapter, MinimumFeatureLevel, IID_PPV_ARGS(&device)),
                          "Failed to create D3D12 device");
            SetDebugName(device.Get(), L"Atmos Device");

            CheckRequiredFeatures(device.Get());
            return device;
        }

        ComPtr<ID3D12CommandQueue> CreateGraphicsQueue(ID3D12Device* device)
        {
            D3D12_COMMAND_QUEUE_DESC desc{};
            desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

            ComPtr<ID3D12CommandQueue> queue;
            ThrowIfFailed(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&queue)),
                          "Failed to create graphics queue");
            SetDebugName(queue.Get(), L"Graphics Queue");
            return queue;
        }
    }

    GraphicsDevice::GraphicsDevice(const GraphicsDeviceOptions& options)
        : factory_(CreateFactory(options)),
          adapter_(SelectAdapter(factory_.Get())),
          device_(CreateDevice(adapter_.Get())),
          graphicsQueue_(CreateGraphicsQueue(device_.Get())),
          shaderResourceHeap_(device_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                              GraphicsConfig::ShaderResourceDescriptorCount, true, L"Shader Resource Heap"),
          renderTargetHeap_(device_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
                            GraphicsConfig::RenderTargetDescriptorCount, false, L"Render Target Heap"),
          depthStencilHeap_(device_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
                            GraphicsConfig::DepthStencilDescriptorCount, false, L"Depth Stencil Heap"),
          bindlessRootSignature_(CreateBindlessRootSignature(device_.Get()))
    {
        if (options.enableDebugLayer || options.enableGpuValidation)
            ConfigureInfoQueue();
    }

    void GraphicsDevice::ConfigureInfoQueue()
    {
        if (FAILED(device_.As(&infoQueue_)))
            return;

        // Only store warnings and errors; info messages are noise.
        D3D12_MESSAGE_SEVERITY deniedSeverities[] = {
            D3D12_MESSAGE_SEVERITY_INFO,
            D3D12_MESSAGE_SEVERITY_MESSAGE,
        };

        D3D12_INFO_QUEUE_FILTER filter{};
        filter.DenyList.NumSeverities = static_cast<UINT>(std::size(deniedSeverities));
        filter.DenyList.pSeverityList = deniedSeverities;
        infoQueue_->PushStorageFilter(&filter);

        if (IsDebuggerPresent())
        {
            infoQueue_->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
            infoQueue_->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
        }
    }

    void GraphicsDevice::LogDebugMessages()
    {
        if (!infoQueue_)
            return;

        const UINT64 count = infoQueue_->GetNumStoredMessages();

        for (UINT64 i = 0; i < count; ++i)
        {
            SIZE_T size = 0;
            infoQueue_->GetMessage(i, nullptr, &size);

            std::vector<std::byte> storage(size);
            auto* message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());

            if (FAILED(infoQueue_->GetMessage(i, message, &size)))
                continue;

            switch (message->Severity)
            {
                case D3D12_MESSAGE_SEVERITY_CORRUPTION:
                case D3D12_MESSAGE_SEVERITY_ERROR:
                    log::Error("D3D12: {}", message->pDescription);
                    break;

                case D3D12_MESSAGE_SEVERITY_WARNING:
                    log::Warning("D3D12: {}", message->pDescription);
                    break;

                default:
                    log::Info("D3D12: {}", message->pDescription);
                    break;
            }
        }

        infoQueue_->ClearStoredMessages();
    }

    void GraphicsDevice::LogDeviceRemovedInfo()
    {
        log::Error("GPU device removed (reason {:#010x})",
                   static_cast<uint32_t>(device_->GetDeviceRemovedReason()));

        ComPtr<ID3D12DeviceRemovedExtendedData> dred;

        if (FAILED(device_.As(&dred)))
            return;

        D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT breadcrumbs{};

        if (SUCCEEDED(dred->GetAutoBreadcrumbsOutput(&breadcrumbs)))
        {
            for (const D3D12_AUTO_BREADCRUMB_NODE* node = breadcrumbs.pHeadAutoBreadcrumbNode; node;
                 node = node->pNext)
            {
                const UINT completed = node->pLastBreadcrumbValue ? *node->pLastBreadcrumbValue : 0;

                if (completed < node->BreadcrumbCount)
                {
                    const wchar_t* name = node->pCommandListDebugNameW ? node->pCommandListDebugNameW
                                                                       : L"<unnamed>";
                    log::Error("  Unfinished command list '{}': {}/{} operations completed",
                               ToUtf8(name), completed, node->BreadcrumbCount);
                }
            }
        }

        D3D12_DRED_PAGE_FAULT_OUTPUT pageFault{};

        if (SUCCEEDED(dred->GetPageFaultAllocationOutput(&pageFault)) && pageFault.PageFaultVA != 0)
            log::Error("  Page fault at GPU address {:#x}", pageFault.PageFaultVA);
    }
}
