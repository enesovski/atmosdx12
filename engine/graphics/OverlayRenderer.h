#pragma once

namespace atmos
{
    class CommandContext;

    // Something drawn on top of the final image (e.g. the debug UI). The renderer records it
    // in the last graph pass with the back buffer bound; implementations may change any
    // pipeline state, because nothing renders after them.
    class IOverlayRenderer
    {
    public:
        virtual ~IOverlayRenderer() = default;
        virtual void RecordOverlay(CommandContext& commands) = 0;
    };
}
