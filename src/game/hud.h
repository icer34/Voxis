#pragma once

#include <cstdint>

/**
 * @brief Debug overlay - currently just an FPS counter drawn via ImGui, meant to be replaced by a
 * custom-rendered HUD later (see Renderer for the ImGui backend plumbing, this class only builds
 * widget content)
 *
 */
class Hud
{
public:
    void draw(float dt);

private:
    constexpr static float FPS_UPDATE_INTERVAL = 1.0f;

    float _fpsAccumTime = 0.0f;
    uint32_t _fpsAccumFrames = 0;
    float _displayedFps = 0.0f;
};
