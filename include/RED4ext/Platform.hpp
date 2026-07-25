#pragma once

/**
 * @file
 * @brief The single place in the SDK where platform differences are spelled out.
 *
 * On Windows this is a thin pass-through to <Windows.h> plus the MSVC spellings the
 * SDK already used, so behaviour there is unchanged.
 *
 * Elsewhere it supplies layout-compatible stand-ins for the handful of Windows types
 * that appear inside size-asserted structs, and small primitives for the operations
 * the SDK performs through Win32 (atomics, thread yielding, module lookup).
 *
 * Nothing here attempts to emulate Windows semantics. Where an operation has no honest
 * counterpart on the host platform, the corresponding declaration is deliberately
 * absent rather than faked.
 */

#if defined(_WIN32)
#define RED4EXT_PLATFORM_WINDOWS 1
#else
#define RED4EXT_PLATFORM_WINDOWS 0
#endif

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>

#if RED4EXT_PLATFORM_WINDOWS

#include <Windows.h>
#include <intrin.h> // _Interlocked* intrinsics used by the atomics below

#else

#include <cerrno>
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <sched.h>

// Handle types. These appear as members of structs whose size is asserted, so they
// must stay pointer-sized. They are opaque on Windows too, so aliasing them to void*
// loses nothing.
using HANDLE = void*;
using HMODULE = void*;
using HINSTANCE = void*;
using HWND = void*;

/**
 * @brief Layout-compatible stand-in for the Win32 CRITICAL_SECTION.
 *
 * RED4ext::Mutex embeds one of these and asserts its own size is 40 bytes, matching
 * the game's layout. The field names mirror the Windows definition so the size and
 * alignment are derived rather than hard-coded.
 */
struct CRITICAL_SECTION
{
    void* DebugInfo;             // 00
    std::int32_t LockCount;      // 08
    std::int32_t RecursionCount; // 0C
    HANDLE OwningThread;         // 10
    HANDLE LockSemaphore;        // 18
    std::uintptr_t SpinCount;    // 20
}; // 28

#endif

/**
 * @brief The calling convention the game's native functions use.
 *
 * __fastcall is the x64 Windows default. On AArch64 there is a single calling
 * convention (AAPCS64), so the annotation is simply empty.
 */
#ifndef RED4EXT_CALL
#if RED4EXT_PLATFORM_WINDOWS
#define RED4EXT_CALL __fastcall
#else
#define RED4EXT_CALL
#endif
#endif

/**
 * @brief Marks a symbol exported from a plugin.
 */
#ifndef RED4EXT_C_EXPORT
#if RED4EXT_PLATFORM_WINDOWS
#define RED4EXT_C_EXPORT extern "C" __declspec(dllexport)
#else
#define RED4EXT_C_EXPORT extern "C" __attribute__((visibility("default")))
#endif
#endif

namespace RED4ext
{
namespace Detail
{
/**
 * @brief Primitives the SDK needs from the platform.
 *
 * On Windows these forward to the Interlocked* intrinsics the SDK used directly
 * before; elsewhere they use the compiler's atomic builtins. Both are sequentially
 * consistent, matching the Interlocked* guarantee.
 */
namespace Platform
{
template<typename T>
inline T AtomicExchange(volatile T* aTarget, T aValue)
{
#if RED4EXT_PLATFORM_WINDOWS
    if constexpr (sizeof(T) == 1)
        return static_cast<T>(
            _InterlockedExchange8(reinterpret_cast<volatile char*>(aTarget), static_cast<char>(aValue)));
    else if constexpr (sizeof(T) == 2)
        return static_cast<T>(
            _InterlockedExchange16(reinterpret_cast<volatile short*>(aTarget), static_cast<short>(aValue)));
    else if constexpr (sizeof(T) == 4)
        return static_cast<T>(
            _InterlockedExchange(reinterpret_cast<volatile long*>(aTarget), static_cast<long>(aValue)));
    else
        return static_cast<T>(
            _InterlockedExchange64(reinterpret_cast<volatile __int64*>(aTarget), static_cast<__int64>(aValue)));
#else
    return __atomic_exchange_n(aTarget, aValue, __ATOMIC_SEQ_CST);
#endif
}

/**
 * @brief Adds @p aValue and returns the value held *before* the addition.
 */
template<typename T>
inline T AtomicFetchAdd(volatile T* aTarget, T aValue)
{
#if RED4EXT_PLATFORM_WINDOWS
    if constexpr (sizeof(T) == 1)
        return static_cast<T>(
            _InterlockedExchangeAdd8(reinterpret_cast<volatile char*>(aTarget), static_cast<char>(aValue)));
    else if constexpr (sizeof(T) == 2)
        return static_cast<T>(
            _InterlockedExchangeAdd16(reinterpret_cast<volatile short*>(aTarget), static_cast<short>(aValue)));
    else if constexpr (sizeof(T) == 4)
        return static_cast<T>(
            _InterlockedExchangeAdd(reinterpret_cast<volatile long*>(aTarget), static_cast<long>(aValue)));
    else
        return static_cast<T>(
            _InterlockedExchangeAdd64(reinterpret_cast<volatile __int64*>(aTarget), static_cast<__int64>(aValue)));
#else
    return __atomic_fetch_add(aTarget, aValue, __ATOMIC_SEQ_CST);
#endif
}

/**
 * @brief Adds @p aValue and returns the resulting value.
 */
template<typename T>
inline T AtomicAddFetch(volatile T* aTarget, T aValue)
{
#if RED4EXT_PLATFORM_WINDOWS
    return static_cast<T>(AtomicFetchAdd(aTarget, aValue) + aValue);
#else
    return __atomic_add_fetch(aTarget, aValue, __ATOMIC_SEQ_CST);
#endif
}

/**
 * @brief Compare-and-swap. Returns the value the target held beforehand, matching
 *        InterlockedCompareExchange rather than the std::atomic convention.
 */
template<typename T>
inline T AtomicCompareExchange(volatile T* aTarget, T aExchange, T aComparand)
{
#if RED4EXT_PLATFORM_WINDOWS
    if constexpr (sizeof(T) == 1)
        return static_cast<T>(_InterlockedCompareExchange8(
            reinterpret_cast<volatile char*>(aTarget), static_cast<char>(aExchange), static_cast<char>(aComparand)));
    else if constexpr (sizeof(T) == 2)
        return static_cast<T>(_InterlockedCompareExchange16(
            reinterpret_cast<volatile short*>(aTarget), static_cast<short>(aExchange), static_cast<short>(aComparand)));
    else if constexpr (sizeof(T) == 4)
        return static_cast<T>(_InterlockedCompareExchange(reinterpret_cast<volatile long*>(aTarget),
                                                          static_cast<long>(aExchange), static_cast<long>(aComparand)));
    else
        return static_cast<T>(_InterlockedCompareExchange64(reinterpret_cast<volatile __int64*>(aTarget),
                                                            static_cast<__int64>(aExchange),
                                                            static_cast<__int64>(aComparand)));
#else
    T expected = aComparand;
    __atomic_compare_exchange_n(aTarget, &expected, aExchange, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return expected;
#endif
}

/**
 * @brief Offers the remainder of the current time slice to another ready thread.
 */
inline void YieldThread()
{
#if RED4EXT_PLATFORM_WINDOWS
    SwitchToThread();
#else
    sched_yield();
#endif
}

/**
 * @brief Base address the main executable is loaded at.
 *
 * On Windows this is the module handle of the process image. Elsewhere the loaded
 * images must be scanned for the one of type MH_EXECUTE: index 0 is *not* reliable,
 * because DYLD_INSERT_LIBRARIES entries can precede the executable in that list.
 */
inline std::uintptr_t GetMainImageBase()
{
#if RED4EXT_PLATFORM_WINDOWS
    return reinterpret_cast<std::uintptr_t>(GetModuleHandle(nullptr));
#else
    static const std::uintptr_t base = []() -> std::uintptr_t
    {
        const std::uint32_t count = _dyld_image_count();
        for (std::uint32_t i = 0; i < count; ++i)
        {
            const auto* header = reinterpret_cast<const mach_header_64*>(_dyld_get_image_header(i));
            if (header && header->magic == MH_MAGIC_64 && header->filetype == MH_EXECUTE)
            {
                return reinterpret_cast<std::uintptr_t>(header);
            }
        }
        return 0;
    }();
    return base;
#endif
}

/**
 * @brief Name of the RED4ext runtime module, in this platform's shared-library form.
 */
#if RED4EXT_PLATFORM_WINDOWS
inline constexpr const wchar_t* RuntimeModuleName = L"RED4ext.dll";
#else
inline constexpr const char* RuntimeModuleName = "RED4ext.dylib";
#endif

/**
 * @brief Handle of an already-loaded module, or nullptr when it is not loaded.
 *
 * Neither implementation loads the module: on Windows GetModuleHandleW only looks up
 * loaded modules, and RTLD_NOLOAD gives dlopen the same behaviour.
 */
inline HMODULE GetLoadedModule()
{
#if RED4EXT_PLATFORM_WINDOWS
    return GetModuleHandleW(RuntimeModuleName);
#else
    return dlopen(RuntimeModuleName, RTLD_LAZY | RTLD_NOLOAD);
#endif
}

/**
 * @brief Address of an exported symbol within a module.
 */
inline void* GetSymbol(HMODULE aModule, const char* aName)
{
#if RED4EXT_PLATFORM_WINDOWS
    return reinterpret_cast<void*>(GetProcAddress(aModule, aName));
#else
    return dlsym(aModule, aName);
#endif
}

/**
 * @brief Handle of the module containing @p aAddress, or nullptr on failure.
 */
inline HMODULE GetModuleContaining(const void* aAddress)
{
#if RED4EXT_PLATFORM_WINDOWS
    HMODULE result = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(aAddress), &result))
    {
        return nullptr;
    }
    return result;
#else
    Dl_info info{};
    if (dladdr(aAddress, &info) == 0)
    {
        return nullptr;
    }
    // dlopen on the resolved path returns a usable handle for the same image; RTLD_NOLOAD
    // keeps it from loading anything new.
    return dlopen(info.dli_fname, RTLD_LAZY | RTLD_NOLOAD);
#endif
}

/**
 * @brief Filesystem path of the module containing @p aAddress.
 */
inline std::filesystem::path GetModulePathContaining(const void* aAddress)
{
#if RED4EXT_PLATFORM_WINDOWS
    HMODULE handle = GetModuleContaining(aAddress);
    if (!handle)
    {
        return {};
    }

    std::wstring fileName;
    DWORD length = 0;
    do
    {
        fileName.resize(fileName.size() + MAX_PATH, L'\0');
        length = GetModuleFileNameW(handle, fileName.data(), static_cast<std::uint32_t>(fileName.size()));
    } while (GetLastError() == ERROR_INSUFFICIENT_BUFFER);

    if (length > 0)
    {
        fileName.resize(length);
    }
    return fileName;
#else
    Dl_info info{};
    if (dladdr(aAddress, &info) == 0 || !info.dli_fname)
    {
        return {};
    }
    return std::filesystem::path(info.dli_fname);
#endif
}

/**
 * @brief The platform's last-error code, for inclusion in diagnostics.
 */
inline std::uint32_t GetLastErrorCode()
{
#if RED4EXT_PLATFORM_WINDOWS
    return static_cast<std::uint32_t>(GetLastError());
#else
    return static_cast<std::uint32_t>(errno);
#endif
}

/**
 * @brief Reports an unrecoverable error to the user and terminates the process.
 *
 * Windows shows a message box, matching the SDK's long-standing behaviour. Elsewhere the
 * text goes to stderr, which is where a mod's output is actually observable.
 */
[[noreturn]] inline void FatalError(std::wstring_view aTitle, std::wstring_view aMessage)
{
#if RED4EXT_PLATFORM_WINDOWS
    const std::wstring title(aTitle);
    const std::wstring message(aMessage);
    MessageBoxW(nullptr, message.c_str(), title.c_str(), MB_ICONERROR | MB_OK);
    TerminateProcess(GetCurrentProcess(), 1);
#else
    std::fwprintf(stderr, L"\n[RED4ext.SDK] %.*ls\n%.*ls\n", static_cast<int>(aTitle.size()), aTitle.data(),
                  static_cast<int>(aMessage.size()), aMessage.data());
    std::fflush(stderr);
#endif
    std::abort();
}

/**
 * @brief Aligned allocation. @p aAlignment must be a power of two.
 */
inline void* AlignedAlloc(std::size_t aSize, std::size_t aAlignment)
{
#if RED4EXT_PLATFORM_WINDOWS
    return _aligned_malloc(aSize, aAlignment);
#else
    void* result = nullptr;
    // posix_memalign additionally requires the alignment to be a multiple of sizeof(void*).
    if (aAlignment < sizeof(void*))
    {
        aAlignment = sizeof(void*);
    }
    if (posix_memalign(&result, aAlignment, aSize) != 0)
    {
        return nullptr;
    }
    return result;
#endif
}

inline void AlignedFree(void* aMemory)
{
#if RED4EXT_PLATFORM_WINDOWS
    _aligned_free(aMemory);
#else
    std::free(aMemory);
#endif
}
} // namespace Platform
} // namespace Detail
} // namespace RED4ext
