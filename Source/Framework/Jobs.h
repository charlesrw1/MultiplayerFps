#pragma once
#include "Framework/Config.h"
#include <future>
#include "RingBuffer.h"
extern ConfigVar with_threading;

class JobSystem;
struct JobCounter {
public:
private:
    JobCounter() = default;
    std::atomic<int> c; // counts down to 0
    friend class JobSystem;
};
struct JobDecl {
    void(*func)(uintptr_t arg) = nullptr;
    uintptr_t funcarg = 0;
    JobCounter* counter = nullptr;
};
class JobSystem
{
public:
    static JobSystem* inst;
    JobSystem();
    ~JobSystem() {}
    void add_job(JobDecl j, JobCounter*& c);
    void add_job(void(*func)(uintptr_t), uintptr_t user, JobCounter*& c);
    void add_job_no_counter(void(*func)(uintptr_t), uintptr_t user);
    void add_jobs(JobDecl* j, int count, JobCounter*& c);
    void wait_and_free_counter(JobCounter*& c);
private:
    void add_job_internal(JobDecl j);
    static void worker_thread_loop(int id);
    std::vector<std::thread> threads;
    RingBuffer<JobDecl> job_queue;
    std::mutex job_mutex;
    std::condition_variable cv;
    struct mainthread_data {
        std::mutex mutex;
        std::condition_variable cv;
    }mt;
};
