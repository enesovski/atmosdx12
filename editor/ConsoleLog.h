#pragma once

#include "core/Log.h"

#include <array>
#include <cstddef>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

namespace atmos::editor
{
    struct ConsoleEntry
    {
        log::Level level;
        std::string message;
        double time;  // seconds since the console was created
    };

    // Captures the engine log into a bounded ring buffer. Messages arrive from any thread; the UI
    // copies new ones out once per frame with Drain().
    class ConsoleLog
    {
    public:
        static constexpr size_t Capacity = 2000;

        ConsoleLog();
        ~ConsoleLog();

        ConsoleLog(const ConsoleLog&) = delete;
        ConsoleLog& operator=(const ConsoleLog&) = delete;

        // Moves messages received since the last call into the UI-side history.
        void Drain();
        void Clear();

        const std::deque<ConsoleEntry>& GetEntries() const { return entries_; }
        size_t GetCount(log::Level level) const { return counts_[static_cast<size_t>(level)]; }

        // Errors since the console was last viewed (the status bar flashes until then).
        size_t GetUnseenErrors() const { return unseenErrors_; }
        void MarkSeen() { unseenErrors_ = 0; }

    private:
        void Append(ConsoleEntry entry);

        log::SinkId sink_ = 0;
        std::mutex mutex_;
        std::vector<ConsoleEntry> incoming_;
        std::deque<ConsoleEntry> entries_;
        std::array<size_t, 3> counts_{};
        size_t unseenErrors_ = 0;
        double startTime_ = 0.0;
    };
}
