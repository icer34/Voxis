#include "chunk_mesher.h"

#include "directions.h"

#include <array>
#include <vector>
#include <bit>

namespace
{
/**
 * @brief get the binary columns relkative to a given pair of axis
 *
 * @param chunk
 * @param axisA
 * @param axisB
 * @return an array containing all the columns
 */
std::array<uint16_t, Chunk::SIZE * Chunk::SIZE> getBinColumns(Chunk& chunk, int axisA, int axisB, int axisMain)
{
    std::array<uint16_t, Chunk::SIZE * Chunk::SIZE> columns;

    for (size_t a = 0; a < Chunk::SIZE; a++)
    {
        for (size_t b = 0; b < Chunk::SIZE; b++)
        {
            uint16_t col = 0;
            for (size_t c = 0; c < Chunk::SIZE; c++)
            {
                glm::ivec3 pos{};
                pos[axisMain] = static_cast<int>(c);
                pos[axisA] = static_cast<int>(a);
                pos[axisB] = static_cast<int>(b);

                if (chunk.getBlock(pos) == 0)
                    continue;

                col |= (1u << c);
            }

            columns[a + b * Chunk::SIZE] = col;
        }
    }

    return columns;
}

/**
 * @brief extract a single layer out of a packed column array (bit `layer` of every column) as a 2D
 * grid, one row per "a" coordinate, one bit per "b" coordinate - used both for the visibility grid
 * and the raw-solidity grid (AO neighbor queries)
 */
std::array<uint16_t, Chunk::SIZE> extractLayer(const std::array<uint16_t, Chunk::SIZE * Chunk::SIZE>& source,
                                               uint32_t layer)
{
    std::array<uint16_t, Chunk::SIZE> result{};
    for (uint32_t a = 0; a < Chunk::SIZE; a++)
    {
        for (uint32_t b = 0; b < Chunk::SIZE; b++)
        {
            if (source[a + b * Chunk::SIZE] & (1u << layer))
                result[a] |= (1u << b);
        }
    }
    return result;
}

/**
 * @brief the classic voxel AO formula: if both edge neighbors are solid, the corner is fully
 * occluded regardless of the diagonal neighbor; otherwise the occlusion level is just how many of
 * the 3 neighbors are solid
 */
uint8_t cornerAO(bool side1, bool side2, bool corner)
{
    if (side1 && side2)
        return 0u;
    return static_cast<uint8_t>(3 - (static_cast<int>(side1) + static_cast<int>(side2) + static_cast<int>(corner)));
}

/**
 * @brief For every cell of a layer, compute the AO level (0-3) of its 4 corners from the neighbors
 * solid at that same layer, packed into a single byte (2 bits per corner: bits 0-1 = (a-,b-),
 * 2-3 = (a-,b+), 4-5 = (a+,b-), 6-7 = (a+,b+))
 *
 * @param solid raw block solidity of the layer the face opens into (not the solid voxel's own layer,
 * and not visibility - just "is there a block here")
 */
std::array<std::array<uint8_t, Chunk::SIZE>, Chunk::SIZE> computeAOGrid(const std::array<uint16_t, Chunk::SIZE>& solid)
{
    std::array<std::array<uint8_t, Chunk::SIZE>, Chunk::SIZE> ao{};

    for (uint32_t a = 0; a < Chunk::SIZE; a++)
    {
        uint16_t rowMinus = (a > 0) ? solid[a - 1] : 0u;
        uint16_t rowPlus = (a + 1 < Chunk::SIZE) ? solid[a + 1] : 0u;

        for (uint32_t b = 0; b < Chunk::SIZE; b++)
        {
            bool bMinus = (b > 0) && (solid[a] & (1u << (b - 1)));
            bool bPlus = (b + 1 < Chunk::SIZE) && (solid[a] & (1u << (b + 1)));
            bool aMinus = (rowMinus & (1u << b)) != 0u;
            bool aPlus = (rowPlus & (1u << b)) != 0u;
            bool cornerMM = (b > 0) && (rowMinus & (1u << (b - 1)));
            bool cornerMP = (b + 1 < Chunk::SIZE) && (rowMinus & (1u << (b + 1)));
            bool cornerPM = (b > 0) && (rowPlus & (1u << (b - 1)));
            bool cornerPP = (b + 1 < Chunk::SIZE) && (rowPlus & (1u << (b + 1)));

            uint8_t ao00 = cornerAO(aMinus, bMinus, cornerMM);
            uint8_t ao01 = cornerAO(aMinus, bPlus, cornerMP);
            uint8_t ao10 = cornerAO(aPlus, bMinus, cornerPM);
            uint8_t ao11 = cornerAO(aPlus, bPlus, cornerPP);

            ao[a][b] = static_cast<uint8_t>(ao00 | (ao01 << 2) | (ao10 << 4) | (ao11 << 6));
        }
    }

    return ao;
}

struct Rect
{
    uint32_t x, y, width, height;
    uint8_t ao;
};

/**
 * @brief 2D greedy merge - two cells only merge if they're both visible AND share the exact same
 * AO signature (otherwise the AO gradient across the merged quad would be wrong, since a quad only
 * has 4 corners to interpolate between)
 */
std::vector<Rect> greedyMerge2D(std::array<uint16_t, Chunk::SIZE> grid,
                                const std::array<std::array<uint8_t, Chunk::SIZE>, Chunk::SIZE>& ao)
{
    std::vector<Rect> rects;

    for (uint32_t a = 0; a < Chunk::SIZE; a++)
    {
        for (uint32_t b = 0; b < Chunk::SIZE; b++)
        {
            if (!(grid[a] & (1u << b)))
                continue;

            uint8_t signature = ao[a][b];

            // expand on width: bound by the visibility run (fast, bitwise), then narrow down to
            // where the AO signature stops matching
            uint32_t visWidth = static_cast<uint32_t>(std::countr_one(static_cast<uint16_t>(grid[a] >> b)));
            uint32_t width = 1;
            while (width < visWidth && ao[a][b + width] == signature)
                width++;

            uint16_t mask = static_cast<uint16_t>(((1u << width) - 1u) << b);

            // expand on height: every cell of the next row must also match both visibility and AO
            uint32_t height = 0;
            while (a + height < Chunk::SIZE && (grid[a + height] & mask) == mask)
            {
                bool rowMatches = true;
                for (uint32_t i = 0; i < width; i++)
                {
                    if (ao[a + height][b + i] != signature)
                    {
                        rowMatches = false;
                        break;
                    }
                }

                if (!rowMatches)
                    break;

                height++;
            }

            // set to 0 used bits
            for (uint32_t i = 0; i < height; i++)
                grid[a + i] &= ~mask;

            // push the obtained rect
            rects.push_back(Rect{ a, b, width, height, signature });
        }
    }

    return rects;
}

void addQuad(MeshData& mesh,
             glm::ivec3 basePos,
             Direction dir,
             uint32_t width,
             uint32_t height,
             std::array<uint8_t, 4> cornerAOLevels)
{
    uint32_t base = static_cast<uint32_t>(mesh.vertices.size());

    for (uint32_t cornerID = 0; cornerID < 4; cornerID++)
    {
        uint32_t data1 = 0;
        data1 |= (static_cast<uint32_t>(basePos.x) & 0xFu);
        data1 |= ((static_cast<uint32_t>(basePos.y) & 0xFu) << 4);
        data1 |= ((static_cast<uint32_t>(basePos.z) & 0xFu) << 8);
        data1 |= ((static_cast<uint32_t>(dir) & 0x7u) << 12);
        data1 |= ((cornerID & 0x3u) << 15);
        data1 |= ((static_cast<uint32_t>(cornerAOLevels[cornerID]) & 0x3u) << 17);

        uint32_t data2 = 0;
        data2 |= (width - 1) & 0xFu;
        data2 |= ((height - 1) & 0xFu) << 4;

        mesh.vertices.push_back({ data1, data2 });
    }

    mesh.indices.push_back(base + 0);
    mesh.indices.push_back(base + 1);
    mesh.indices.push_back(base + 2);
    mesh.indices.push_back(base + 0);
    mesh.indices.push_back(base + 2);
    mesh.indices.push_back(base + 3);
}
} // namespace

namespace ChunkMesher
{
MeshData getMeshData(Chunk& chunk, const BlockRegistry& registry)
{
    MeshData mesh;

    // build the columns binary masks for each axis
    std::array<std::array<uint16_t, Chunk::SIZE * Chunk::SIZE>, 3> binColumns;
    for (int axis = 0; axis < 3; axis++)
    {
        int axisA = (axis + 1) % 3;
        int axisB = (axis + 2) % 3;
        binColumns[static_cast<size_t>(axis)] = getBinColumns(chunk, axisA, axisB, axis);
    }

    // apply binary greedy meshing algorithm
    for (Direction dir : ALL_DIRECTIONS)
    {
        auto [axis, sign] = axisFromDirection(dir);

        // get the visibility masks (is the block solid && the block next is air)
        std::array<uint16_t, Chunk::SIZE * Chunk::SIZE> visibility;
        for (uint32_t a = 0; a < Chunk::SIZE; a++)
        {
            for (uint32_t b = 0; b < Chunk::SIZE; b++)
            {
                uint16_t col = binColumns[static_cast<size_t>(axis)][a + b * Chunk::SIZE];
                visibility[a + b * Chunk::SIZE] = (sign == 1) ? (col & ~(col >> 1)) : (col & ~(col << 1));
            }
        }

        // get the other axies
        int axisA = (static_cast<int>(axis) + 1) % 3;
        int axisB = (static_cast<int>(axis) + 2) % 3;

        // the shader's per-direction offset table doesn't map corner2D.x/y to axisA/axisB the same
        // way for every direction (it was hand-tuned for correct winding) - for these 3, width/
        // height and the corresponding AO corners need to be swapped before emission
        bool axisSwapped = (dir == Direction::SOUTH || dir == Direction::EAST || dir == Direction::UP);

        // extract the 2D grid per layer
        for (uint32_t c = 0; c < Chunk::SIZE; c++)
        {
            std::array<uint16_t, Chunk::SIZE> grid = extractLayer(visibility, c);

            // raw solidity of the layer the face actually opens into (the "outside" layer, same one
            // used to derive visibility above) - not the solid voxel's own layer, which would find
            // every same-height neighbor solid on any flat surface and read as fully occluded
            int neighborLayer = (sign == 1) ? static_cast<int>(c) + 1 : static_cast<int>(c) - 1;
            std::array<uint16_t, Chunk::SIZE> solidLayer{};
            if (neighborLayer >= 0 && neighborLayer < static_cast<int>(Chunk::SIZE))
                solidLayer = extractLayer(binColumns[static_cast<size_t>(axis)], static_cast<uint32_t>(neighborLayer));

            auto aoGrid = computeAOGrid(solidLayer);

            auto rects = greedyMerge2D(grid, aoGrid);

            // build a quad for every rect
            for (auto rect : rects)
            {
                glm::ivec3 pos{};
                pos[static_cast<int>(axis)] = static_cast<int>(c);
                pos[axisA] = static_cast<int>(rect.x);
                pos[axisB] = static_cast<int>(rect.y);

                uint8_t ao00 = rect.ao & 0x3u;
                uint8_t ao01 = (rect.ao >> 2) & 0x3u;
                uint8_t ao10 = (rect.ao >> 4) & 0x3u;
                uint8_t ao11 = (rect.ao >> 6) & 0x3u;

                // cornerID 0/2 are always the (start,start)/(end,end) grid corners; 1/3 depend on
                // which grid axis "width" maps to for this direction (see axisSwapped above)
                std::array<uint8_t, 4> cornerAOLevels{};
                cornerAOLevels[0] = ao00;
                cornerAOLevels[2] = ao11;
                cornerAOLevels[1] = axisSwapped ? ao10 : ao01;
                cornerAOLevels[3] = axisSwapped ? ao01 : ao10;

                if (axisSwapped)
                    addQuad(mesh, pos, dir, rect.height, rect.width, cornerAOLevels);
                else
                    addQuad(mesh, pos, dir, rect.width, rect.height, cornerAOLevels);
            }
        }
    }

    return mesh;
}
} // namespace ChunkMesher
