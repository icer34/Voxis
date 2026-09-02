#pragma once

#include <cassert>
#include <functional>
#include <future>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>

/**
 * @brief generic class that allows to submit tasks to a thread pool of a given size
 *
 */
class ThreadPool
{
public:
    ThreadPool(uint32_t n)
    {
        _workers.reserve(n);

        for (size_t i = 0; i < n; i++)
            _workers.emplace_back(std::thread{ &ThreadPool::workerLoop, this });
    }

    ~ThreadPool()
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _stop = true;
        }

        _condition.notify_all();

        for (auto& thread : _workers)
            thread.join();
    }

    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>>
    {
        using ReturnType = std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            [func = std::forward<F>(f), ...capturedArgs = std::forward<Args>(args)]() mutable
            { return std::invoke(func, capturedArgs...); });

        std::future<ReturnType> result = task->get_future();

        {
            std::unique_lock<std::mutex> lock(_mutex);
            assert(!_stop && "ThreadPool::submit() called after the pool has started shutting down");
            _tasks.emplace([task] { (*task)(); });
        }

        _condition.notify_one();
        return result;
    }

private:
    std::queue<std::function<void()>> _tasks;
    std::mutex _mutex; // protects _tasks and _stop
    std::condition_variable _condition;
    std::vector<std::thread> _workers;
    bool _stop = false;

    void workerLoop()
    {
        while (true)
        {
            std::unique_lock<std::mutex> lock(_mutex);

            _condition.wait(lock, [this] { return _stop || !_tasks.empty(); });

            if (_stop && _tasks.empty())
                break;

            auto task = std::move(_tasks.front());
            _tasks.pop();

            lock.unlock();

            task();
        }
    }
};