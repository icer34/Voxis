#pragma once

#include <cstdint>
#include <functional>

// Handle types definition
template <typename Tag> struct Handle
{
    uint32_t value = 0;
    bool operator==(const Handle&) const = default;
};
namespace std
{
template <typename Tag> struct hash<Handle<Tag>>
{
    size_t operator()(const Handle<Tag>& h) const noexcept { return std::hash<uint32_t>{}(h.value); };
};
} // namespace std

using MeshHandle = Handle<struct MeshTag>;
using Texture2DHandle = Handle<struct Texture2DTag>;
using Texture3DHandle = Handle<struct Texture3DTag>;
