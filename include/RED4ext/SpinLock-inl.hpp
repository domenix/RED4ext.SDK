#pragma once

#ifdef RED4EXT_STATIC_LIB
#include <RED4ext/SpinLock.hpp>
#endif

#include <cstdint>

#include <RED4ext/Platform.hpp>

RED4EXT_INLINE RED4ext::SpinLock::SpinLock()
    : state(0)
{
}

RED4EXT_INLINE bool RED4ext::SpinLock::TryLock()
{
    return RED4ext::Detail::Platform::AtomicExchange(&state, static_cast<char>(1)) == 0;
}

RED4EXT_INLINE void RED4ext::SpinLock::Lock()
{
    uint32_t loopCount = 0;
    while (true)
    {
        if (TryLock())
            break;

        if (loopCount >= 16)
            RED4ext::Detail::Platform::YieldThread();
        ++loopCount;
    }
}

RED4EXT_INLINE void RED4ext::SpinLock::Unlock()
{
    RED4ext::Detail::Platform::AtomicExchange(&state, static_cast<char>(0));
}

// ----------------------------
// -- support for lock_guard --
// ----------------------------

RED4EXT_INLINE bool RED4ext::SpinLock::try_lock()
{
    return TryLock();
}

RED4EXT_INLINE void RED4ext::SpinLock::lock()
{
    Lock();
}

RED4EXT_INLINE void RED4ext::SpinLock::unlock()
{
    Unlock();
}
