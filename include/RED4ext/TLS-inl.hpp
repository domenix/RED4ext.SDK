#pragma once

#include <RED4ext/Platform.hpp>

#ifdef RED4EXT_STATIC_LIB
#include <RED4ext/TLS.hpp>
#endif


RED4EXT_INLINE RED4ext::TLS* RED4ext::TLS::Get()
{
#if RED4EXT_PLATFORM_WINDOWS
    return *reinterpret_cast<TLS**>(__readgsqword(0x58));
#else
    // __readgsqword(0x58) reads slot 11 of the Windows Thread Environment Block through
    // the GS segment. There is no equivalent off Windows: the TEB is a Windows construct
    // and the offset is meaningless against any other thread-local layout.
    //
    // Returning the AArch64 thread pointer would compile and yield a plausible non-null
    // pointer that is simply wrong. Returning null instead fails immediately and
    // unmistakably at the first use, rather than silently reading garbage.
    //
    // A static_assert was tried here first and rejected: TLS::Get() is a plain inline
    // function, so the assertion fires merely from including the header, which would make
    // the SDK uncompilable for every consumer on this platform rather than only for those
    // who actually call it.
    //
    // Locating the game's own thread-local storage is reverse-engineering work, not a
    // portability shim.
    return nullptr;
#endif
}
