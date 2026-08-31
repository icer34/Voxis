#include "block_registry.h"

BlockRegistry::BlockRegistry()
    : _nameToID{
        { "stone", 1u },
    }
{
    for (const auto& [name, id] : _nameToID)
        _idToName[id] = name;
}

uint16_t BlockRegistry::idxFromName(const std::string& name) const
{
    return _nameToID.at(name);
}

const std::string& BlockRegistry::nameFromIdx(uint16_t id) const
{
    return _idToName.at(id);
}