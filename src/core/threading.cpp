#include "threading.h"
#include "math.h"
#include <intrin.h>

JobFactory* JobFactory::_instance = new JobFactory{};

bool JobFactory::PerformWork() {
    WorkQueueEntry entry;
    if (_queue.PopFront(entry)) {
        entry.Callback();
        --entry.Context->NumJobs;

        return true;
    }

    return false;
}

void JobFactory::WorkerThreadProc() {
    while (true) {
        if (not PerformWork()) {
            WaitForSingleObjectEx(_semaphoreHandle, INFINITE, FALSE);
        }
    }
}

void JobFactory::InitializeJobSystem() {
    uint32 numThreads = std::thread::hardware_concurrency();
    numThreads = clamp(numThreads, 1u, 8u);
    _semaphoreHandle = CreateSemaphoreExW(0, 0, numThreads, 0, 0, SEMAPHORE_ALL_ACCESS);

    for (uint32 i = 0; i < numThreads; i++) {
        std::thread thread([&] { WorkerThreadProc(); });

        HANDLE handle = (HANDLE)thread.native_handle();

        uint64 affinityMask = 1ull < i;
        SetThreadAffinityMask(handle, affinityMask);

        // SetThreadPriority(handle, THREAD_PRIORITY_HIGHEST);
        SetThreadDescription(handle, L"Worker thread");

        thread.detach();
    }
}

void ThreadJobContext::AddWork(const std::function<void()> &work) {
    WorkQueueEntry entry;
    entry.Callback = work;
    entry.Context = this;
    ++NumJobs;

    while (not JobFactory::Instance()->_queue.PushBack(entry)) {
        JobFactory::Instance()->PerformWork();
    }

    ReleaseSemaphore(JobFactory::Instance()->_semaphoreHandle, 1, 0);
}

void ThreadJobContext::WaitForWorkCompletion() {
    while (NumJobs) {
        if (not JobFactory::Instance()->PerformWork()) {
            while (NumJobs) {}
            break;
        }
    }
}


