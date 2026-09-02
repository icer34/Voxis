#pragma once

#include <functional>

/**
 * @brief Handles the deletion of multiple objects that have the SAME LIFETIME
 * (see renderer.cpp for usage example)
 *
 */
class DeletionQueue
{
public:
    void push(std::function<void()> deleter) { _deleters.push_back(std::move(deleter)); }

    void flush()
    {
        for (auto it = _deleters.rbegin(); it != _deleters.rend(); it++)
            (*it)();

        _deleters.clear();
    }

private:
    std::vector<std::function<void()>> _deleters;
};