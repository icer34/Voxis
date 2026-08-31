#pragma once

#include <string>
#include <fstream>
#include <sstream>

namespace Voxis
{
std::string readFile(const std::string& filePath)
{
    std::ifstream infile(filePath);
    if (infile.is_open())
    {
        std::stringstream buffer;
        buffer << infile.rdbuf();
        const std::string output = buffer.str();
        infile.close();
        return output;
    }
    return std::string();
}
} // namespace Voxis
