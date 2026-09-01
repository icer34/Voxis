#include "block_atlas.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <vector>

#include "log.h"

namespace fs = std::filesystem;

namespace
{
std::vector<unsigned char> downsample(const std::vector<unsigned char>& src, uint32_t w, uint32_t h)
{
    uint32_t newW = w / 2;
    uint32_t newH = h / 2;
    std::vector<unsigned char> dst(static_cast<size_t>(newW) * newH * 4);

    for (uint32_t y = 0; y < newH; y++)
    {
        for (uint32_t x = 0; x < newW; x++)
        {
            for (uint32_t c = 0; c < 4; c++)
            {
                uint32_t p1 = src[((2 * x) + (2 * y) * w) * 4 + c];
                uint32_t p2 = src[((2 * x + 1) + (2 * y) * w) * 4 + c];
                uint32_t p3 = src[((2 * x) + (2 * y + 1) * w) * 4 + c];
                uint32_t p4 = src[((2 * x + 1) + (2 * y + 1) * w) * 4 + c];

                dst[(x + y * newW) * 4 + c] = static_cast<unsigned char>((p1 + p2 + p3 + p4) / 4);
            }
        }
    }

    return dst;
}
} // namespace

BlockAtlas::BlockAtlas(Renderer& renderer)
    : _renderer(renderer)
{
    std::vector<fs::path> paths;
    for (const auto& entry : fs::directory_iterator(TEXTURES_DIR))
    {
        if (!entry.is_regular_file() || entry.path().extension() != ".png")
            continue;
        paths.push_back(entry.path());
    }
    std::sort(paths.begin(), paths.end());

    if (paths.empty())
        VoxisLog::critical("No block textures found in {}", TEXTURES_DIR);

    uint32_t numTiles = static_cast<uint32_t>(paths.size());
    uint32_t gridSize = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<double>(numTiles))));
    uint32_t atlasSize = gridSize * CELL_STRIDE;

    std::array<std::vector<unsigned char>, MIP_LEVELS> levels;
    for (uint32_t lvl = 0; lvl < MIP_LEVELS; lvl++)
    {
        uint32_t levelSize = atlasSize >> lvl;
        levels[lvl].assign(static_cast<size_t>(levelSize) * levelSize * 4, 0);
    }

    uint32_t col = 0;
    uint32_t row = 0;

    for (const auto& path : paths)
    {
        int width = 0, height = 0, channels = 0;
        unsigned char* data = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
        if (!data)
        {
            VoxisLog::error("Failed to load block texture: {}", path.string());
            continue;
        }

        std::vector<unsigned char> lvlData(data, data + (static_cast<size_t>(width) * static_cast<size_t>(height) * 4));
        stbi_image_free(data);

        uint32_t lvlW = static_cast<uint32_t>(width);
        uint32_t lvlH = static_cast<uint32_t>(height);

        for (uint32_t lvl = 0; lvl < MIP_LEVELS; lvl++)
        {
            uint32_t cellStride = CELL_STRIDE >> lvl;
            uint32_t padding = PADDING >> lvl;

            std::vector<unsigned char> padded(static_cast<size_t>(cellStride) * cellStride * 4);
            for (uint32_t py = 0; py < cellStride; py++)
            {
                uint32_t srcY = std::clamp(py, padding, padding + lvlH - 1) - padding;
                for (uint32_t px = 0; px < cellStride; px++)
                {
                    uint32_t srcX = std::clamp(px, padding, padding + lvlW - 1) - padding;
                    for (uint32_t c = 0; c < 4; c++)
                        padded[(px + py * cellStride) * 4 + c] = lvlData[(srcX + srcY * lvlW) * 4 + c];
                }
            }

            uint32_t levelAtlasSize = atlasSize >> lvl;
            uint32_t dstX = col * cellStride;
            uint32_t dstY = row * cellStride;
            for (uint32_t y = 0; y < cellStride; y++)
            {
                for (uint32_t x = 0; x < cellStride; x++)
                {
                    for (uint32_t c = 0; c < 4; c++)
                    {
                        levels[lvl][((dstX + x) + (dstY + y) * levelAtlasSize) * 4 +
                                    c] = padded[(x + y * cellStride) * 4 + c];
                    }
                }
            }

            if (lvlW <= 1 && lvlH <= 1)
                continue;

            lvlData = downsample(lvlData, lvlW, lvlH);
            lvlW = std::max(1u, lvlW / 2);
            lvlH = std::max(1u, lvlH / 2);
        }

        _nameToIndex[path.stem().string()] = row * gridSize + col;

        col++;
        if (col >= gridSize)
        {
            col = 0;
            row++;
        }
    }

    std::array<const unsigned char*, MIP_LEVELS> levelPtrs{};
    for (uint32_t lvl = 0; lvl < MIP_LEVELS; lvl++)
        levelPtrs[lvl] = levels[lvl].data();

    _handle = _renderer.createTexture2D(atlasSize, atlasSize, MIP_LEVELS, VK_FORMAT_R8G8B8A8_SRGB, 4, levelPtrs.data());
}

BlockAtlas::~BlockAtlas()
{
    _renderer.destroyTexture2D(_handle);
}

Texture2DHandle BlockAtlas::handle() const
{
    return _handle;
}

uint32_t BlockAtlas::tileIndex(const std::string& name) const
{
    return _nameToIndex.at(name);
}
