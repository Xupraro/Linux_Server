#pragma once

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>

class ThreadPool 
{
private:
    std::vector<std::thread> workers_;          // �����߳�
    std::queue<std::function<void()>>tasks_;    // ������У������������
    std::mutex queue_mutex_;                    // ������еĻ�����
    std::condition_variable condition_;         // �����������������ѹ����߳�
    bool stop_;                                 // �̳߳��Ƿ����ڹ���
public:
    ThreadPool(size_t size) :stop_(false)
    {
        for (size_t i = 0;i < size;++i)
        {
            workers_.emplace_back([this] {
                while (true)
                {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex_);
                        this->condition_.wait(lock, [this] {
                            return this->stop_ || !this->tasks_.empty();
                        });
                        if (this->stop_ && this->tasks_.empty())
                        {
                            return;
                        }
                        task = std::move(tasks_.front());
                        this->tasks_.pop();
                    }
                    task();
                }
            });
        }
    }
    ~ThreadPool()
    {
        {
            std::unique_lock<std::mutex> lock(this->queue_mutex_);
            this->stop_ = true;
        }
        condition_.notify_all();
        for (std::thread& worker : this->workers_)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }
    }
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type>
    {
        using return_type = typename std::result_of<F(Args...)>::type;
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(this->queue_mutex_);
            if (this->stop_)
            {
                throw std::runtime_error("enqueue on stopped ThreadPool.");
            }
            this->tasks_.emplace([task]() {
                (*task)();
            });
        }
        this->condition_.notify_one();
        return res;
    }
};