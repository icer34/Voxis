#pragma once

#include <cstdint>
#include <array>

#include <glm/glm.hpp>

enum class Direction : uint32_t
{
    NORTH = 0,
    SOUTH,
    EAST,
    WEST,
    UP,
    DOWN,
};

constexpr std::array<Direction, 6> ALL_DIRECTIONS{
    Direction::NORTH, Direction::SOUTH, Direction::EAST, Direction::WEST, Direction::UP, Direction::DOWN,
};

constexpr std::array<glm::ivec3, 6> DIRECTION_NORMALS{
    glm::ivec3(0, 0, -1), // North
    glm::ivec3(0, 0, 1),  // South
    glm::ivec3(1, 0, 0),  // East
    glm::ivec3(-1, 0, 0), // West
    glm::ivec3(0, 1, 0),  // Up
    glm::ivec3(0, -1, 0), // Down
};

enum class Axis : uint32_t
{
    X = 0,
    Y,
    Z,
};

inline std::pair<Axis, int> axisFromDirection(Direction dir)
{
    switch (dir)
    {
    case Direction::NORTH:
        return { Axis::Z, -1 };
    case Direction::SOUTH:
        return { Axis::Z, 1 };
    case Direction::EAST:
        return { Axis::X, 1 };
    case Direction::WEST:
        return { Axis::X, -1 };
    case Direction::UP:
        return { Axis::Y, 1 };
    case Direction::DOWN:
        return { Axis::Y, -1 };
    }
}