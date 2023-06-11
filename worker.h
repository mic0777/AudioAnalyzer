#include <queue>
#include <thread>
#include <mutex>
#include <future>
#include <condition_variable>
#include <fstream>

class Worker
{
    std::vector<char> compressedAudio;
    std::string songName;
    std::ofstream &ofStream;
    int64_t duration;
    unsigned frequency;
public:
    Worker(std::vector<char> &&v, std::ofstream &ofStream, std::string name) :
         compressedAudio(v), ofStream(ofStream), songName(name) {}
    Worker() = delete;
    Worker(const Worker &) = delete;
    Worker(const Worker && w) : compressedAudio(std::move(w.compressedAudio)), ofStream(w.ofStream), songName(w.songName){}
    Worker & operator=(const Worker &) = delete;
    ~Worker() = default;
    void operator()();
private:
    void writeToCSV(std::string &key, std::string &tempo);
    std::string detectTempo(std::vector<float> &wav);
    std::string detectKey(std::vector<float> &wav);
};



#ifndef THREAD_POOL_NAMESPACE_NAME
#define THREAD_POOL_NAMESPACE_NAME thread_pool
#endif

class ThreadPool {
private:
    class TaskWrapper {
        struct ImplBase {
            virtual void call() = 0;
            virtual ~ImplBase() = default;
        };
        std::unique_ptr<ImplBase> impl;

        template <typename F>
        struct ImplType : ImplBase {
            F f;
            ImplType(F&& f_) : f{std::forward<F>(f_)} {}
            void call() final { f(); }
        };

    public:
        template <typename F>
        TaskWrapper(F f) : impl{std::make_unique<ImplType<F>>(std::move(f))} {}

        auto operator()() { impl->call(); }
    };

    class JoinThreads {
    public:
        explicit JoinThreads(std::vector<std::thread>& threads)
            : _threads{threads} {}

        ~JoinThreads() {
            for (auto& t : _threads) {
                t.join();
            }
        }

    private:
        std::vector<std::thread>& _threads;
    };

public:
    explicit ThreadPool(
        size_t threadCount = std::thread::hardware_concurrency())
        : _done{false}, _joiner{_threads} {
        if (0u == threadCount) {
            threadCount = 1u;
        }
        _threads.reserve(threadCount);
        try {
            for (size_t i = 0; i < threadCount; ++i) {
                _threads.emplace_back(&ThreadPool::workerThread, this);
            }
        } catch (...) {
            _done = true;
            _queue.cv.notify_all();
            exception = std::current_exception();
            throw;
        }
    }

    ~ThreadPool() {
        _done = true;
        _queue.cv.notify_all();
    }

    size_t capacity() const { return _threads.size(); }
    size_t queueSize() const {
        std::unique_lock<std::mutex> l{_queue.m};
        return _queue.q.size();
    }

    
    void submit(Worker w) {
        {
            std::lock_guard<std::mutex> l{_queue.m};
            _queue.q.push(std::move(w));
        }
        _queue.cv.notify_all();
        ++totalSubmitted;
    }

    int getPercentDone() {
        if (totalSubmitted == 0) return 0;
        return 100 * totalDone / totalSubmitted;
    }

    int getTotalDone() {
        return totalDone;
    }

    bool done() {return _done;}

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    std::exception_ptr exception;

private:
    void workerThread() {
        while (!_done) {
            std::unique_lock<std::mutex> l{_queue.m};
            _queue.cv.wait(l, [&] { return !_queue.q.empty() || _done; });
            if (_done) {
                break;
            }
            auto task = std::move(_queue.q.front());
            _queue.q.pop();
            l.unlock();
            try {
                task();
            } catch (...) {
            _done = true;
            _queue.cv.notify_all();
            exception = std::current_exception();
            throw;
            }

            ++totalDone;
        }
    }

    struct TaskQueue {
        std::queue<TaskWrapper> q;
        std::mutex m;
        std::condition_variable cv;
    };

private:
    std::atomic_bool _done;
    mutable TaskQueue _queue;
    std::vector<std::thread> _threads;
    JoinThreads _joiner;
    std::atomic_int totalSubmitted{0};
    std::atomic_int totalDone{0};
};
