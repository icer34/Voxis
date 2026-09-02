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
    void draw(double dt);

    /**
     * @brief Records one Game::tick() execution - call once per tick with how long the tick's own
     * logic took to run (not TICK_DT, the actual measured wall time)
     */
    void recordTick(double tickDurationSeconds);

private:
    constexpr static float FPS_UPDATE_INTERVAL = 1.0f;

    float _fpsAccumTime = 0.0f;
    uint32_t _fpsAccumFrames = 0;
    float _displayedFps = 0.0f;

    // refreshed on the same real-time window as FPS above (driven by draw()'s dt) - NOT on
    // accumulated tick execution time, otherwise the refresh cadence itself would depend on how
    // fast/slow ticks are (fast ticks -> takes forever in real time to accumulate 1s of tick time)
    double _tickTimeSum = 0.0;
    uint32_t _tickCount = 0;
    double _displayedTPS = 0.0;
    double _displayedMsPerTick = 0.0;
};
