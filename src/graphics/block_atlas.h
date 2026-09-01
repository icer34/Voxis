#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "renderer.h"

/**
 * @brief Loads all the block textures and makes available a single uint index for every texture file
 *
 */
class BlockAtlas
{
public:
    /**
     * @brief Loads all the textures in the folder "assets/textures/block", builds the mips and
     * uploads the result as a single bindless texture via the renderer
     *
     * @param renderer used to create (and, on destruction, release) the GPU texture
     */
    explicit BlockAtlas(Renderer& renderer);
    ~BlockAtlas();

    BlockAtlas(const BlockAtlas&) = delete;
    BlockAtlas& operator=(const BlockAtlas&) = delete;

    /**
     * @brief The bindless texture handle to use (ex: in a PushConstants) when drawing
     */
    Texture2DHandle handle() const;

    /**
     * @brief The tile index for a given texture file (its name, without extension), used by the
     * mesher to compute UVs
     * TODO: might need to become per-face once blocks have different textures per face
     *
     * @param name the texture file's stem, ex: "stone" for "assets/textures/block/stone.png"
     */
    uint32_t tileIndex(const std::string& name) const;

    // shared with the shaders (injected as macro definitions at shader-compile time by Renderer,
    // see createShaderModule) - single source of truth, don't duplicate these values in GLSL
    constexpr static uint32_t TEXTURE_SIZE = 16;
    constexpr static uint32_t MIP_LEVELS = 4;
    constexpr static uint32_t PADDING = 1 << (MIP_LEVELS - 1);
    constexpr static uint32_t CELL_STRIDE = TEXTURE_SIZE + 2 * PADDING;

private:
    constexpr static const char* TEXTURES_DIR = "assets/textures/block";

    Renderer& _renderer;
    Texture2DHandle _handle{};
    std::unordered_map<std::string, uint32_t> _nameToIndex;
};
