#pragma once

#include <cstddef>
#include <cstdint>

#include <RED4ext/Platform.hpp>

#ifdef RED4EXT_STATIC_LIB
#undef RED4EXT_HEADER_ONLY
#define RED4EXT_INLINE
#else
#define RED4EXT_HEADER_ONLY
#define RED4EXT_INLINE inline
#endif

#ifndef RED4EXT_ASSERT_ESCAPE
#define RED4EXT_ASSERT_ESCAPE(...) __VA_ARGS__
#endif

#ifndef RED4EXT_ASSERT_SIZE
#define RED4EXT_ASSERT_SIZE(cls, size)                                                                                 \
    static_assert(sizeof(cls) == size, #cls " size does not match the expected size (" #size ") ")
#endif

#ifndef RED4EXT_ASSERT_OFFSET
// offsetof is well-formed inside a static_assert on clang with C++20, so the assertion no
// longer needs to be compiled out there. It is worth keeping enabled: member offsets are
// the only thing that catches a base class's tail padding being reused by a derived class,
// which the Itanium ABI does and MSVC does not. The size assertions cannot see that,
// because the explicit unkXX[] padding arrays re-anchor later members.
//
// Both compilers warn about offsetof on non-standard-layout types; the SDK applies it to
// polymorphic types on purpose, so the warning is suppressed locally rather than globally.
#if defined(__clang__)
#define RED4EXT_ASSERT_OFFSET(cls, mbr, offset)                                                                        \
    _Pragma("clang diagnostic push") _Pragma("clang diagnostic ignored \"-Winvalid-offsetof\"") static_assert(         \
        offsetof(cls, mbr) == offset, #cls "::" #mbr " is not on the expected offset (" #offset ")");                  \
    _Pragma("clang diagnostic pop")
#else
#define RED4EXT_ASSERT_OFFSET(cls, mbr, offset)                                                                        \
    static_assert(offsetof(cls, mbr) == offset, #cls "::" #mbr " is not on the expected offset (" #offset ")")
#endif
#endif

/**
 * @brief This macro is used to avoid compiler warnings about unreferenced / used parameter.
 */
#ifndef RED4EXT_UNUSED_PARAMETER
#define RED4EXT_UNUSED_PARAMETER(param) (param)
#endif

#ifndef RED4EXT_DECLARE_TYPE
#define RED4EXT_DECLARE_TYPE(type, name)                                                                               \
    const type* const_##name;                                                                                          \
    type* name;
#endif

// RED4EXT_C_EXPORT and RED4EXT_CALL are defined in <RED4ext/Platform.hpp>.

/*
 * @brief Compute the runtime address of an offset.
 *
 * @example
 *  const auto offset = 0x14022EAD0 - 0x140000000;
 *  const auto addr =  RED4EXT_OFFSET_TO_ADDR(offset);
 */
#ifndef RED4EXT_OFFSET_TO_ADDR
#if RED4EXT_PLATFORM_WINDOWS
#define RED4EXT_OFFSET_TO_ADDR(offset)                                                                                 \
    reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(GetModuleHandle(nullptr)) + offset)
#else
// The equivalent elsewhere is the main image's load address, which is a runtime
// lookup rather than a macro. Left undefined so a use site fails loudly instead of
// silently computing a wrong address.
#endif
#endif
