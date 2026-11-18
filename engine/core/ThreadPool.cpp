#include "core/ThreadPool.h"

#include <algorithm>

namespace atmos
{
    ThreadPool::ThreadPool(uint32_t threadCount)
    {
        threadCount = std::max(threadCount, 1u);

        for (uint32_t i = 0; i < threadCount; ++i)
            workers_.emplace_back([this] { WorkerLoop(); });
    }

    ThreadPool::~ThreadPool()
    {
        {
            const std::lock_guard lock(mutex_);
            stopping_ = true;
            jobs_.clear();
        }

        wakeUp_.notify_all();

        for (std::thread& worker : workers_)
            worker.join();
    }

    void ThreadPool::Submit(std::function<void()> job)
    {
        {
            const std::lock_guard lock(mutex_);
            jobs_.push_back(std::move(job));
        }

        wakeUp_.notify_one();
    }

    size_t ThreadPool::GetPendingCount() const
    {
        const std::lock_guard lock(mutex_);
        return jobs_.size() + runningJobs_;
    }

    void ThreadPool::WorkerLoop()
    {
        while (true)
        {
            std::function<void()> job;

            {
                std::unique_lock lock(mutex_);
                wakeUp_.wait(lock, [this] { return stopping_ || !jobs_.empty(); });

                if (stopping_)
                    return;

                job = std::move(jobs_.front());
                jobs_.pop_front();
                ++runningJobs_;
            }

            job();

            const std::lock_guard lock(mutex_);
            --runningJobs_;
        }
    }
}
