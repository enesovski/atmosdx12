#include "EditorApp.h"

#include "core/Log.h"

#include <cstdlib>
#include <exception>
#include <string_view>

namespace
{
    // --scene <file>     .json scene or a single .gltf/.glb (default: the last scene, then scenes/sponza.json)
    // --dump-frame       log the first frame's render graph and culling stats
    // --gpu-validation   D3D12 GPU-based validation (slow)
    // --spawn-lights N   add N random point lights to the scene
    atmos::editor::EditorOptions ParseCommandLine(int argc, char* argv[])
    {
        atmos::editor::EditorOptions options;

        for (int i = 1; i < argc; ++i)
        {
            const std::string_view argument = argv[i];

            if (argument == "--gpu-validation")
                options.device.enableGpuValidation = true;
            else if (argument == "--dump-frame")
                options.dumpFrame = true;
            else if (argument == "--scene" && i + 1 < argc)
                options.scenePath = argv[++i];
            else if (argument == "--spawn-lights" && i + 1 < argc)
                options.spawnLights = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
            else
                atmos::log::Warning("Unknown argument: {}", argument);
        }

        return options;
    }
}

int main(int argc, char* argv[])
{
    // Crisp UI on high-DPI monitors: the editor scales its fonts per monitor instead of being stretched.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    try
    {
        atmos::editor::EditorApp app(GetModuleHandleW(nullptr), ParseCommandLine(argc, argv));
        return app.Run();
    }
    catch (const std::exception& e)
    {
        atmos::log::Error("{}", e.what());
        return 1;
    }
}
