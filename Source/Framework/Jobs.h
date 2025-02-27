#pragma once
#include "Framework/Config.h"
#include <future>
extern ConfigVar with_threading;

namespace jobs
{

struct Counter;
struct JobDecl
{
    void(*func)(uintptr_t arg) = nullptr;
    uintptr_t funcarg = 0;
    Counter* counter = nullptr;
};

void init();
void add_job(JobDecl j, Counter*& c);
void add_job(void(*func)(uintptr_t), uintptr_t user, Counter*& c);
void add_job_no_counter(void(*func)(uintptr_t), uintptr_t user);

void add_jobs(JobDecl* j, int count, Counter*& c);

void wait_and_free_counter(Counter*& c);

}
