#include "Jobs.h"

#include <vector>
#include <cassert>
#include "tracy/public/tracy/Tracy.hpp"
#undef TracyFiberEnter
#define TracyFiberEnter(x)
#undef max



//thread_local int osthread_id = 0;


void JobSystem::worker_thread_loop(int id)
{
    //osthread_id = id;
    for (;;)
    {
        JobDecl job;
        {
            std::unique_lock<std::mutex> lock(inst->job_mutex);
            inst->cv.wait(lock, []() { return !inst->job_queue.empty(); });
            job = inst->job_queue[0];
            inst->job_queue.pop_front();
        }
   
        job.func(job.funcarg);
    
        JobCounter* c = job.counter;
        if (c) {
            int o = c->c.fetch_sub(1);  // decrement
            if (o == 1/*  0 after decrement */) {
                std::unique_lock<std::mutex> lock(inst->mt.mutex);
                inst->mt.cv.notify_all();
            }
        }
    }
}


void JobSystem::wait_and_free_counter(JobCounter*& c)
{
    if (!c)
        return;
    if (c->c.load() == 0) {
        delete c;
        c = nullptr;
        return; // dont need to wait, continue
    }

    std::unique_lock<std::mutex> lock(mt.mutex);
    mt.cv.wait(lock, [&]() { return c->c.load()<=0; });

    delete c;
    c = nullptr;
}

JobSystem* JobSystem::inst = nullptr;

ConfigVar threading_num_worker_threads("threading.num_worker_threads", "0", CVAR_INTEGER | CVAR_DEV, "number of worker threads, 0 for default", 0, 8);

JobSystem::JobSystem() {
    inst = this;

    int NUM_WORKERS = threading_num_worker_threads.get_integer();
    if (NUM_WORKERS == 0)
        NUM_WORKERS = std::thread::hardware_concurrency() - 2;
    if (NUM_WORKERS <= 0)
        NUM_WORKERS = 1;

    for (int i = 0; i < NUM_WORKERS; i++) {
        threads.push_back(std::thread(worker_thread_loop, i + 1));
        threads.back().detach();
    }
}
void JobSystem::add_job_internal(JobDecl j)
{
    {
        std::lock_guard<std::mutex> lock(job_mutex);
        job_queue.push_back(j);
    }
    cv.notify_one();
}
void JobSystem::add_job_no_counter(void(*func)(uintptr_t), uintptr_t user)
{
    JobDecl decl;
    decl.funcarg = user;
    decl.func = func;
    add_job_internal(decl);
}
void JobSystem::add_job(JobDecl j, JobCounter*& c) {
    if (!with_threading.get_bool()) {
        j.func(j.funcarg);
        return;
    }
    
    if (!c)
        c = new JobCounter;

    j.counter = c;
    j.counter->c.fetch_add(1, std::memory_order::memory_order_relaxed);
    
    add_job_internal(j);
}
void JobSystem::add_job(void(*func)(uintptr_t), uintptr_t user, JobCounter*& c) {
    JobDecl decl;
    decl.funcarg = user;
    decl.func = func;
    add_job(decl, c);
}

void JobSystem::add_jobs(JobDecl* j, int count, JobCounter*& c) {
    if (!with_threading.get_bool()) {
        for(int i=0;i<count;i++)
            j[i].func(j[i].funcarg);
        return;
    }
    if (!c)
        c = new JobCounter;
    c->c.fetch_add(count);
    for (int i = 0; i < count; i++) {
        j[i].counter = c;
    }
    {
        std::lock_guard<std::mutex> lock(job_mutex);
        for (int i = 0; i < count; i++)
            job_queue.push_back(j[i]);
    }
    cv.notify_all();
}
