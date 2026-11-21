#include "ConsoleLog.h"

#include <chrono>

namespace atmos::editor
{
    namespace
    {
        double NowSeconds()
        {
            using namespace std::chrono;
            return duration<double>(steady_clock::now().time_since_epoch()).count();
        }
    }

    ConsoleLog::ConsoleLog() : startTime_(NowSeconds())
    {
        sink_ = log::AddSink([this](log::Level level, std::string_view message) {
            const std::lock_guard lock(mutex_);
            incoming_.push_back({ level, std::string(message), NowSeconds() - startTime_ });
        });
    }

    ConsoleLog::~ConsoleLog()
    {
        log::RemoveSink(sink_);
    }

    void ConsoleLog::Drain()
    {
        std::vector<ConsoleEntry> received;

        {
            const std::lock_guard lock(mutex_);
            received.swap(incoming_);
        }

        for (ConsoleEntry& entry : received)
            Append(std::move(entry));
    }

    void ConsoleLog::Clear()
    {
        entries_.clear();
        counts_ = {};
        unseenErrors_ = 0;
    }

    void ConsoleLog::Append(ConsoleEntry entry)
    {
        const auto level = static_cast<size_t>(entry.level);
        ++counts_[level];

        if (entry.level == log::Level::Error)
            ++unseenErrors_;

        entries_.push_back(std::move(entry));

        if (entries_.size() > Capacity)
        {
            --counts_[static_cast<size_t>(entries_.front().level)];
            entries_.pop_front();
        }
    }
}
