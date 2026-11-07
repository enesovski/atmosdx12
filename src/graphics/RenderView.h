#pragma once

#include "../scene/Camera.h"

struct RenderView
{
    CameraBasis camera{};
    int viewportWidth = 0;
    int viewportHeight = 0;
};
