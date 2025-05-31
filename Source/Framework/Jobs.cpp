#include "Jobs.h"
#include <Windows.h>
#include <vector>
#include <cassert>
#include "tracy/public/tracy/Tracy.hpp"
#undef TracyFiberEnter
#define TracyFiberEnter(x)
#undef max
template<typename T>
class RingBuffer
{
public:
    RingBuffer(int count = 0) :buf(count) {}
    bool empty() const {
        return size() == 0;
    }
    int size() const {
        return count;
    }
    // push to end
    void push_back(const T& item) {
        if (count >= buf.size()) {
            resize_internal_space(std::max(buf.size() * 2, 4ull));
        }
        count++;
        (*this)[size() - 1] = item;
    }
    // pop from start
    void pop_front() {
        assert(count > 0);
        start = get_buf_index(1);
        count--;
    }

    const T& operator[](int index) const {
        return buf.at(get_buf_index(index));
    }
    T& operator[](int index) {
        return buf.at(get_buf_index(index));
    }

    void resize_internal_space(int newcount) {
        std::vector<T> newbuf(newcount);
        for (int i = 0; i < count; i++) {
            newbuf[i] = std::move((*this)[i]);
        }
        start = 0;
        buf = std::move(newbuf);
    }
private:
    int get_buf_index(int i) const {
        //assert(i < count);
        return (start + i) % buf.size();
    }

    std::vector<T> buf;
    int start = 0;
    int count = 0;
};


namespace jobs
{

thread_local int osthread_id = 0;

struct Counter
{
    std::atomic<int> c; // counts down to 0
};


std::vector<std::thread> threads;
RingBuffer<JobDecl> job_queue;
std::mutex job_mutex;
std::condition_variable cv;

struct mainthread_data {
    std::mutex mutex;
    std::condition_variable cv;
}mt;


void worker_thread_loop()
{
    for (;;)
    {
        JobDecl job;
        {
            std::unique_lock<std::mutex> lock(job_mutex);
            cv.wait(lock, []() { return !job_queue.empty(); });
            job = job_queue[0];
            job_queue.pop_front();
        }
   
        job.func(job.funcarg);
    
        Counter* c = job.counter;
        if (c) {
            int o = c->c.fetch_sub(1);  // decrement
            if (o == 1/*  0 after decrement */) {
                std::unique_lock<std::mutex> lock(mt.mutex);
                mt.cv.notify_all();
            }
        }
    }
}


void worker_thread_main(int id)
{
    osthread_id = id;
    worker_thread_loop();
}

void wait_and_free_counter(Counter*& c)
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

ConfigVar threading_num_worker_threads("threading.num_worker_threads", "0", CVAR_INTEGER | CVAR_DEV, "number of worker threads, 0 for default", 0, 8);

void init() {
 
    int NUM_WORKERS = threading_num_worker_threads.get_integer();
    if (NUM_WORKERS == 0)
        NUM_WORKERS = std::thread::hardware_concurrency() - 2;
    if (NUM_WORKERS <= 0)
        NUM_WORKERS = 1;

    for (int i = 0; i < NUM_WORKERS; i++) {
        threads.push_back(std::thread(worker_thread_main, i + 1));
        threads.back().detach();
    }
}
static void add_job_internal(JobDecl j)
{
    {
        std::lock_guard<std::mutex> lock(job_mutex);
        job_queue.push_back(j);
    }
    cv.notify_one();
}
void add_job_no_counter(void(*func)(uintptr_t), uintptr_t user)
{
    JobDecl decl;
    decl.funcarg = user;
    decl.func = func;
    add_job_internal(decl);
}
void add_job(JobDecl j, Counter*& c) {
    if (!with_threading.get_bool()) {
        j.func(j.funcarg);
        return;
    }
    
    if (!c)
        c = new Counter;

    j.counter = c;
    j.counter->c.fetch_add(1, std::memory_order::memory_order_relaxed);
    
    add_job_internal(j);
}
void add_job(void(*func)(uintptr_t), uintptr_t user, Counter*& c) {
    JobDecl decl;
    decl.funcarg = user;
    decl.func = func;
    add_job(decl, c);
}

void add_jobs(JobDecl* j, int count, Counter*& c) {
    if (!with_threading.get_bool()) {
        for(int i=0;i<count;i++)
            j[i].func(j[i].funcarg);
        return;
    }
    if (!c)
        c = new Counter;
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
}