#pragma once

#include <functional>
#include <mutex>
#include <queue>
#include <semaphore>
#include <thread>

#include "task_stack.h"
#include "../../parameters.h"

namespace ppqsort::impl::cpp {
    template <typename taskType = std::function<void()>>
    class ThreadPool {
        public:
            ThreadPool(ThreadPool&) = delete;

            explicit ThreadPool(const std::size_t threads_count = std::jthread::hardware_concurrency()) :
            threads_count_(threads_count), threads_queues_(threads_count) {
                threads_.reserve(threads_count);
                for (unsigned int i = 0; i < threads_count; ++i) {
                    threads_.emplace_back([this, i](const std::stop_token& st) { this->worker(st, i); });
                }
            }

            ~ThreadPool() {
                if (!stopped)
                    wait_and_stop();
            }

            /**
             * \brief Wait for all tasks to finish and stop the threads.
             *        After this call returns, the thread pool is not usable anymore.
             */
            void wait_and_stop() {
                // the queue can be empty, but any running task can generate new tasks
                // check if there are any tasks in queue or being executed, if not we can peacefully stop
                // otherwise, wait for signal
                // after the last thread is finished, it will signal that all tasks are finished
                if (total_tasks_.load(std::memory_order_acquire) > 0)
                        threads_done_semaphore_.acquire();

                // request stops
                for (auto& thread : threads_)
                    thread.request_stop();

                // wake all potentially sleeping threads
                pending_tasks_.store(1, std::memory_order_release);
                pending_tasks_.notify_all();

                for (auto& thread : threads_)
                    thread.join();
                stopped = true;
            }

            void push_task(taskType&& task) {
                // firstly signal, that we will have some tasks
                total_tasks_.fetch_add(1, std::memory_order_release);

                // relaxed is ok, because we do not care about the exact value
                const std::size_t i = index_.fetch_add(1, std::memory_order_relaxed);

                // try to push without blocking to any queue
                // if no queue is available, try again for K times, before waiting for lock
                constexpr unsigned int K = 2;
                for (std::size_t n = 0; n < threads_count_ * K; ++n) {
                    if (threads_queues_[(i + n) % threads_count_].try_push(std::forward<taskType>(task))) {
                        pending_tasks_.fetch_add(1, std::memory_order_release);
                        pending_tasks_.notify_all();
                        return;
                    }
                }

                // all queues busy, wait for the first one
                threads_queues_[i % threads_count_].push(std::forward<taskType>(task));
                pending_tasks_.fetch_add(1, std::memory_order_release);
                pending_tasks_.notify_all();
            }

        private:
            #ifdef GTEST_FLAG
            FRIEND_TEST(testThreadPool, PushBusyQueues);
            FRIEND_TEST(testThreadPool, PopBusyQueues);
            #endif

            bool get_next_task(const unsigned int id) {
                bool found = false;

                // spin around all queues to find a task, start with my queue
                for (std::size_t n = 0; n < threads_count_; ++n) {
                    if (auto task = threads_queues_[(id + n) % threads_count_].try_pop()) {
                        run_task(std::forward<taskType>(task.value()));
                        found = true;
                        break;
                    }
                }

                // if all queues busy or empty, check our queue if empty
                if (!found) {
                    if (auto task = threads_queues_[id].pop()) {
                        run_task(std::forward<taskType>(task.value()));
                        found = true;
                    }
                }

                return found;
            }

            void run_task(taskType&& task) {
                pending_tasks_.fetch_sub(1, std::memory_order_release);
                task();
                total_tasks_.fetch_sub(1, std::memory_order_release);
            }

            void worker(const std::stop_token& stop_token, const unsigned int id) {
                while (true) {
                    // sleep until there are any tasks in queues
                    pending_tasks_.wait(0, std::memory_order_acquire);
                    if (stop_token.stop_requested())
                        break;

                    // while there are tasks, execute them (mine or stolen)
                    while (get_next_task(id));

                    // no tasks are in queues or handled --> signal that our work is done
                    if (total_tasks_.load(std::memory_order_acquire) == 0)
                        threads_done_semaphore_.release();
                }
            }

            const unsigned int threads_count_;
            std::vector<std::jthread> threads_;
            std::vector<TaskStack<>> threads_queues_;
            alignas(parameters::cacheline_size) std::atomic<std::size_t> index_{0};
            alignas(parameters::cacheline_size) std::atomic<std::size_t> pending_tasks_{0};
            alignas(parameters::cacheline_size) std::atomic<std::size_t> total_tasks_{0};
            std::binary_semaphore threads_done_semaphore_{0};   // used to wait for all tasks to finish
            std::mutex mtx_priority_;
            bool stopped = false;
    };
}