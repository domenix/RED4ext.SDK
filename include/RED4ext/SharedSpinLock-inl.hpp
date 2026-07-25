#pragma once

#ifdef RED4EXT_STATIC_LIB
#include <RED4ext/SharedSpinLock.hpp>
#endif

#include <cstdint>

#include <RED4ext/Platform.hpp>

RED4EXT_INLINE RED4ext::SharedSpinLock::SharedSpinLock()
    : state(0)
{
}

RED4EXT_INLINE bool RED4ext::SharedSpinLock::TryLock()
{
    return RED4ext::Detail::Platform::AtomicCompareExchange(&state, static_cast<char>(-1), static_cast<char>(0)) == 0;
}

RED4EXT_INLINE void RED4ext::SharedSpinLock::Lock()
{
    int32_t loopCount = 0;
    while (true)
    {
        if (TryLock())
            break;

        ++loopCount;
        if (loopCount == 0x4000)
            loopCount = 0;
        else if (!(loopCount & 511))
            RED4ext::Detail::Platform::YieldThread();
    }
}

RED4EXT_INLINE void RED4ext::SharedSpinLock::Unlock()
{
    RED4ext::Detail::Platform::AtomicExchange(&state, static_cast<char>(0));
}

RED4EXT_INLINE bool RED4ext::SharedSpinLock::TryLockShared()
{
    char currentState = state;
    if (currentState != -1)
    {
        return RED4ext::Detail::Platform::AtomicCompareExchange(&state, static_cast<char>(currentState + 1), currentState) ==
               currentState;
    }
    return false;
}

RED4EXT_INLINE void RED4ext::SharedSpinLock::LockShared()
{
    int32_t loopCount = 0;
    while (true)
    {
        if (TryLockShared())
            break;

        ++loopCount;
        if (loopCount == 0x4000)
            loopCount = 0;
        else if (!(loopCount & 511))
            RED4ext::Detail::Platform::YieldThread();
    }
}

RED4EXT_INLINE void RED4ext::SharedSpinLock::UnlockShared()
{
    RED4ext::Detail::Platform::AtomicFetchAdd(&state, static_cast<char>(-1));
}

// --------------------------------------------
// -- support for lock_guard and shared_lock --
// --------------------------------------------

RED4EXT_INLINE bool RED4ext::SharedSpinLock::try_lock()
{
    return TryLock();
}

RED4EXT_INLINE void RED4ext::SharedSpinLock::lock()
{
    Lock();
}

RED4EXT_INLINE void RED4ext::SharedSpinLock::unlock()
{
    Unlock();
}

RED4EXT_INLINE bool RED4ext::SharedSpinLock::try_lock_shared()
{
    return TryLockShared();
}

RED4EXT_INLINE void RED4ext::SharedSpinLock::lock_shared()
{
    LockShared();
}

RED4EXT_INLINE void RED4ext::SharedSpinLock::unlock_shared()
{
    UnlockShared();
}
