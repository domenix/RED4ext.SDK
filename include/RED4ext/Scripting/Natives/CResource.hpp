#pragma once

#include <RED4ext/Common.hpp>
#include <RED4ext/ISerializable.hpp>
#include <RED4ext/ResourcePath.hpp>
#include <RED4ext/Scripting/Natives/Generated/ECookingPlatform.hpp>

namespace RED4ext
{
struct CResource : ISerializable
{
    static constexpr const char* NAME = "CResource";
    static constexpr const char* ALIAS = NAME;

    ResourcePath path;                // 30
    ECookingPlatform cookingPlatform; // 38

    // Explicit trailing padding. ECookingPlatform is an int8_t enum, so the members end at
    // 0x39 and the remaining 7 bytes are implicit padding -- which the Itanium ABI reuses
    // for a derived class's first member while MSVC does not, shifting every CResource
    // subclass by 7 bytes.
    //
    // Generated/CResource.hpp declares the same type with this padding already present, so
    // without it the layout of RED4ext::CResource depends on which of the two headers a
    // translation unit happens to include first. sizeof is 0x40 either way.
    uint8_t unk39[0x40 - 0x39]; // 39
};
RED4EXT_ASSERT_SIZE(CResource, 0x40);
} // namespace RED4ext
