#pragma once

namespace atmos
{
    class CommandContext;

    // Drawn last, on the back buffer; may change any pipeline state.
    class IOverlayRenderer
    {
    public:
        virtual ~IOverlayRenderer() = default;
        virtual void RecordOverlay(CommandContext& commands) = 0;
    };
}
