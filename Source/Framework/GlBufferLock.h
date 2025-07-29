#pragma once

#include "glad/glad.h"
#include <vector>

// From azdo presentation
//https://github.dev/nvMcJohn/apitest/tree/master/src/solutions

// --------------------------------------------------------------------------------------------------------------------
struct BufferRange
{
    size_t mStartOffset=0;
    size_t mLength=0;

    bool Overlaps(const BufferRange& _rhs) const {
        return mStartOffset < (_rhs.mStartOffset + _rhs.mLength)
            && _rhs.mStartOffset < (mStartOffset + mLength);
    }
};

// --------------------------------------------------------------------------------------------------------------------
struct BufferLock
{
    BufferRange mRange;
    GLsync mSyncObj=nullptr;

};

// --------------------------------------------------------------------------------------------------------------------
class BufferLockManager
{
public:
    BufferLockManager(bool _cpuUpdates);
    ~BufferLockManager();

    void WaitForLockedRange(size_t _lockBeginBytes, size_t _lockLength);
    void LockRange(size_t _lockBeginBytes, size_t _lockLength);

private:
    void wait(GLsync* _syncObj);
    void cleanup(BufferLock* _bufferLock);

    std::vector<BufferLock> mBufferLocks;

    // Whether it's the CPU (true) that updates, or the GPU (false)
    bool mCPUUpdates=false;
};
