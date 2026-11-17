#include "Application.h"

#include "core/Log.h"

#include <exception>
#include <string_view>

namespace
{
    // --gpu-validation   D3D12 GPU-based validation (slow)
    // --scene <file>     load a .gltf/.glb instead of the built-in test scene
    // --dump-frame       log the first frame's render graph and culling stats
    AppOptions ParseCommandLine(int argc, char* argv[])
    {
        AppOptions options;

        for (int i = 1; i < argc; ++i)
        {
            const std::string_view argument = argv[i];

            if (argument == "--gpu-validation")
                options.device.enableGpuValidation = true;
            else if (argument == "--dump-frame")
                options.dumpFrame = true;
            else if (argument == "--scene" && i + 1 < argc)
                options.scenePath = argv[++i];
            else
                atmos::log::Warning("Unknown argument: {}", argument);
        }

        return options;
    }
}

int main(int argc, char* argv[])
{
    try
    {
        Application app(GetModuleHandleW(nullptr), ParseCommandLine(argc, argv));
        return app.Run();
    }
    catch (const std::exception& e)
    {
        atmos::log::Error("{}", e.what());
        return 1;
    }
}
