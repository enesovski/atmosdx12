#include "Application.h"

#include "core/Log.h"
#include "graphics/d3d12/GraphicsDeviceOptions.h"

#include <exception>
#include <string_view>

int main(int argc, char* argv[])
{
    try
    {
        atmos::GraphicsDeviceOptions deviceOptions;

        for (int i = 1; i < argc; ++i)
        {
            if (std::string_view(argv[i]) == "--gpu-validation")
                deviceOptions.enableGpuValidation = true;
        }

        Application app(GetModuleHandleW(nullptr), deviceOptions);
        return app.Run();
    }
    catch (const std::exception& e)
    {
        atmos::log::Error("{}", e.what());
        return 1;
    }
}
