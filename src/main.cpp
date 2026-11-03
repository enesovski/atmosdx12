#include "Application.h"

#include <exception>
#include <iostream>

int main()
{
    try
    {
        constexpr int width = 1280;
        constexpr int height = 720;

        Application app(GetModuleHandleW(nullptr), width, height);
        return app.Run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}
