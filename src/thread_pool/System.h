#pragma once

// forward declaration for Promise.h
namespace me::eax::thread_pool {
class System; 
}

#include <thread>
#include <chrono>
#include <queue>
#include <vector>
#include <atomic>
#include <functional>
#include <random>
#include <memory>
#include "Future.h"
#include "Promise.h"

namespace me::eax::thread_pool {

class System {

template <class T> friend class Future;

public:
    /*
     * System should be global, so we forbid to copy and move it.
     */
    System(System const &) = delete;
    void operator=(System const &) = delete;

    System() {
        int nthreads = std::thread::hardware_concurrency();
        if(nthreads == 0) nthreads = 2;
        _init(nthreads);
    }

    System(int nthreads) {
        if(nthreads <= 0)
            throw std::invalid_argument("me::eax::thread_pool::System() - nthreads should be grater then zero");
        _init(nthreads);
    }

    ~System() {
        _exit_flag = true;

        for(ThreadInfoEntry* thread_info_ptr: _thread_info) {
            thread_info_ptr->thread_ptr->join();
            delete thread_info_ptr->thread_ptr;
            delete thread_info_ptr;
        }
    }

    template<
        class L,
        class R = typename std::result_of<L(void)>::type
    >
    auto spawn(L lambda) -> std::shared_ptr< Future<R> > {
        auto p = std::make_shared< Promise<R> >(*this);
        _schedule([p, lambda]() {
            try {
                R result = lambda();
                try {
                    p->success(result);
                } catch(const std::exception& err) {
                    // Prevents calling p->failure() after p->success(), hiding the real exception
                    std::terminate();
                }
            } catch(const std::exception& err) {
                p->failure(err);
            }
        });

        return p->future();
    }

    /* Wait until all work is finished */
    auto wait() -> System& {
        bool all_done = false;

        while(!all_done) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            all_done = true;
            for(auto& thread_info: _thread_info) {
                std::lock_guard<std::mutex> lock(thread_info->queue_mutex);
                if(thread_info->executing) {
                    all_done = false;
                    break;
                }
            }
        }

        return *this;
    }

private:
    struct ThreadInfoEntry {
        std::thread* thread_ptr;
        std::mutex queue_mutex;
        std::queue<std::function<void()>> queue;
        bool executing;
    };

    /* Init the object. Common code for all constructors */
    auto _init(int nthreads) -> System& {
        _exit_flag = false;

        for(int i = 0; i < nthreads; i++) {
            ThreadInfoEntry* thread_info_ptr = new ThreadInfoEntry();
            _thread_info.push_back(thread_info_ptr);
            /* only a corresponding thread should change `executing` to false */
            thread_info_ptr->executing = true;
            thread_info_ptr->thread_ptr = new std::thread(_threadProc, thread_info_ptr, &_exit_flag);
        }

        return *this;
    }

    auto _schedule(std::function<void()> task) -> System& {
        int thread_num = random() % _thread_info.size();
        {
            std::lock_guard<std::mutex> lock(_thread_info[thread_num]->queue_mutex);
            _thread_info[thread_num]->queue.push(task);
        }
        return *this;
    }

    static auto _threadProc(ThreadInfoEntry* thread_info_ptr, std::atomic_bool* exit_flag_ptr) -> void {
        while(!*exit_flag_ptr) {
            bool task_found = false;
            std::function<void()> task;

            {
                std::lock_guard<std::mutex> lock(thread_info_ptr->queue_mutex);
                if(thread_info_ptr->queue.empty()) {
                    /* Mark that thread is doing nothing */
                    thread_info_ptr->executing = false;
                } else {
                    task = thread_info_ptr->queue.front();
                    thread_info_ptr->queue.pop();
                    thread_info_ptr->executing = true;
                    task_found = true;
                }
            }

            if(!task_found) {
                // TODO: implement job stealing
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            task();
        }
    }

    std::atomic_bool _exit_flag;
    std::vector< ThreadInfoEntry* > _thread_info;

}; // System

} // namespace
