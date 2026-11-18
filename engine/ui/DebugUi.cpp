#include "ui/DebugUi.h"
#include "graphics/d3d12/CommandContext.h"
#include "graphics/d3d12/GraphicsConfig.h"
#include "graphics/d3d12/GraphicsDevice.h"

#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>

#include <algorithm>
#include <stdexcept>

// Declared by the backend only under #if 0, so the application declares it.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

namespace atmos
{
    namespace
    {
        DebugUi* GetOwner(ImGui_ImplDX12_InitInfo* info)
        {
            return static_cast<DebugUi*>(info->UserData);
        }
    }

    DebugUi::DebugUi(HWND window, GraphicsDevice& device)
        : shaderResourceHeap_(device.GetShaderResourceHeap())
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;  // No imgui.ini next to the working directory.
        ImGui::StyleColorsDark();

        if (!ImGui_ImplWin32_Init(window))
            throw std::runtime_error("Failed to initialize the ImGui Win32 backend");

        // The backend allocates its SRVs from our bindless heap through these callbacks.
        ImGui_ImplDX12_InitInfo info;
        info.Device = device.GetNativeDevice();
        info.CommandQueue = device.GetGraphicsQueue();
        info.NumFramesInFlight = static_cast<int>(GraphicsConfig::FramesInFlight);
        info.RTVFormat = GraphicsConfig::BackBufferFormat;
        info.DSVFormat = DXGI_FORMAT_UNKNOWN;
        info.UserData = this;
        info.SrvDescriptorHeap = shaderResourceHeap_.GetHeap();
        info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* initInfo, D3D12_CPU_DESCRIPTOR_HANDLE* cpu,
                                       D3D12_GPU_DESCRIPTOR_HANDLE* gpu) {
            DebugUi* self = GetOwner(initInfo);
            const Descriptor descriptor = self->shaderResourceHeap_.Allocate();
            self->descriptors_.push_back(descriptor);
            *cpu = descriptor.cpu;
            *gpu = descriptor.gpu;
        };
        info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* initInfo, D3D12_CPU_DESCRIPTOR_HANDLE cpu,
                                      D3D12_GPU_DESCRIPTOR_HANDLE) {
            DebugUi* self = GetOwner(initInfo);
            auto found = std::ranges::find(self->descriptors_, cpu.ptr,
                                           [](const Descriptor& descriptor) { return descriptor.cpu.ptr; });

            if (found != self->descriptors_.end())
            {
                self->shaderResourceHeap_.Free(*found);
                self->descriptors_.erase(found);
            }
        };

        if (!ImGui_ImplDX12_Init(&info))
            throw std::runtime_error("Failed to initialize the ImGui D3D12 backend");
    }

    DebugUi::~DebugUi()
    {
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    bool DebugUi::HandleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        return ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam) != 0;
    }

    void DebugUi::BeginFrame()
    {
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    void DebugUi::EndFrame()
    {
        ImGui::Render();
    }

    bool DebugUi::WantsMouse() const
    {
        return ImGui::GetIO().WantCaptureMouse;
    }

    bool DebugUi::WantsKeyboard() const
    {
        return ImGui::GetIO().WantCaptureKeyboard;
    }

    void DebugUi::RecordOverlay(CommandContext& commands)
    {
        // The backend binds its own root signature; it reads SRVs from the heap already set
        // by CommandContext::Begin.
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commands.GetNativeCommandList());
    }
}
