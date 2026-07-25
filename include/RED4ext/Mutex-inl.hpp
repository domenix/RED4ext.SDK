#pragma once

#ifdef RED4EXT_STATIC_LIB
#include <RED4ext/Mutex.hpp>
#endif

#if !RED4EXT_PLATFORM_WINDOWS
#include <new>
#include <pthread.h>
#endif

#if RED4EXT_PLATFORM_WINDOWS

RED4EXT_INLINE RED4ext::Mutex::Mutex()
{
    InitializeCriticalSection(&m_cs);
}

RED4EXT_INLINE void RED4ext::Mutex::Lock()
{
    EnterCriticalSection(&m_cs);
}

RED4EXT_INLINE void RED4ext::Mutex::Unlock()
{
    LeaveCriticalSection(&m_cs);
}

#else

// A CRITICAL_SECTION is recursive, so the closest honest equivalent is a recursive
// pthread mutex. Darwin's pthread_mutex_t is 64 bytes and does not fit the 40-byte
// layout this structure must keep, so the mutex is heap-allocated and its address is
// parked in the first field. The remaining fields stay zeroed; only the overall size
// matters for layout compatibility.
//
// IMPORTANT: this makes RED4ext::Mutex usable as an SDK-owned lock. It does NOT let you
// lock a mutex belonging to the game -- off Windows the game's own synchronisation
// primitive is not a CRITICAL_SECTION, so a Mutex reinterpreted from game memory holds a
// pointer to something else entirely. Locking such an instance is undefined.

namespace
{
inline pthread_mutex_t*& RED4extMutexHandle(CRITICAL_SECTION& aSection)
{
    return reinterpret_cast<pthread_mutex_t*&>(aSection.DebugInfo);
}
} // namespace

RED4EXT_INLINE RED4ext::Mutex::Mutex()
    : m_cs{}
{
    auto* handle = new (std::nothrow) pthread_mutex_t;
    if (handle)
    {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(handle, &attr);
        pthread_mutexattr_destroy(&attr);
    }

    RED4extMutexHandle(m_cs) = handle;
}

RED4EXT_INLINE void RED4ext::Mutex::Lock()
{
    if (auto* handle = RED4extMutexHandle(m_cs))
        pthread_mutex_lock(handle);
}

RED4EXT_INLINE void RED4ext::Mutex::Unlock()
{
    if (auto* handle = RED4extMutexHandle(m_cs))
        pthread_mutex_unlock(handle);
}

#endif

RED4EXT_INLINE void RED4ext::Mutex::lock()
{
    Lock();
}

RED4EXT_INLINE void RED4ext::Mutex::unlock()
{
    Unlock();
}
