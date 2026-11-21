#pragma once

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace atmos
{
    // Fixed worker threads running FIFO jobs. Jobs must not throw; queued jobs are dropped on
    // destruction.
    class ThreadPool
    {
    public:
        explicit ThreadPool(uint32_t threadCount);
        ~ThreadPool();

        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;

        void Submit(std::function<void()> job);

        size_t GetPendingCount() const;

    private:
        void WorkerLoop();

        mutable std::mutex mutex_;
        std::condition_variable wakeUp_;
        std::deque<std::function<void()>> jobs_;
        std::vector<std::thread> workers_;
        size_t runningJobs_ = 0;
        bool stopping_ = false;
    };
}
