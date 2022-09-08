#include <iostream>
#include <thread>
#include <mutex>
#include <future>
#include <vector>
#include <condition_variable>
#include <queue>
#include <unistd.h>

class ThreadPool {
private:
    std::vector<std::thread> workers;
    std::mutex mux;
    std::condition_variable cv;
    std::queue<std::function<void()>> tasks;
    bool stop;

public:
    explicit ThreadPool(size_t threadSize): stop(false) {
        for (int i = 0; i < threadSize; i ++) {
            workers.emplace_back([this]() {
                for (;;) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> ul(mux);
                        cv.wait(ul, [this] () {return this->stop || !this->tasks.empty();});
                        if (stop && tasks.empty()) break;
                        task = std::move(tasks.front());
                        tasks.pop();
                    }
                    task();
                }
            });
        }
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> ul(mux);
            stop = true;
        }
        cv.notify_all();
        for (auto &worker: workers) {
            worker.join();
        }
    }

    template<typename F, typename ...Args>
    auto submit(F &&func, Args&&... args)->std::future<decltype(func(args...))> {
        auto sharePtr = std::make_shared<std::packaged_task<decltype(func(args...))()>> (
            std::bind(std::forward<F>(func), std::forward<Args>(args)...)
        );
        {
            std::unique_lock<std::mutex> ul(mux);
            if (stop) throw std::runtime_error("thread pool had already been stopped");
            tasks.emplace([sharePtr]() {(*sharePtr)();});
        }
        cv.notify_all();
        return sharePtr->get_future();
    }
};

int main() {
    ThreadPool pool(4);
    std::vector<std::future<int>> results;
    for(int i = 0; i < 8; ++i) {
        results.emplace_back(pool.submit([]() {
            // computing task and return result
            std::cout << "PID is : " << getpid() << std::endl;
            return getpid();
        }));
    }
    return 0;
}