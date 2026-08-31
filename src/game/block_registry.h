#pragma once

#include <unordered_map>
#include <string>
#include <cstdint>

/**
 * @brief Registry that holds all blocks and their properties,
 * created from a config file where the registry reads all the block properties.
 *
 */
class BlockRegistry
{
public:
    BlockRegistry();

    uint16_t idxFromName(const std::string& name) const;
    const std::string& nameFromIdx(uint16_t id) const;

private:
    std::unordered_map<std::string, uint16_t> _nameToID;
    std::unordered_map<uint16_t, std::string> _idToName;
};