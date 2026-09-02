#include "hud.h"

#include <imgui.h>

void Hud::draw(double dt)
{
    _fpsAccumTime += static_cast<float>(dt);
    _fpsAccumFrames++;

    if (_fpsAccumTime >= FPS_UPDATE_INTERVAL)
    {
        _displayedFps = static_cast<float>(_fpsAccumFrames) / _fpsAccumTime;

        // ticks executed per real second elapsed (not per accumulated tick-execution time) - drops
        // below TICK_RATE when tick logic can't keep up with its budget, settles back once it can
        _displayedTPS = static_cast<double>(_tickCount) / static_cast<double>(_fpsAccumTime);
        _displayedMsPerTick = _tickCount > 0 ? (_tickTimeSum / static_cast<double>(_tickCount)) * 1000.0 : 0.0;

        _fpsAccumTime = 0.0f;
        _fpsAccumFrames = 0;
        _tickTimeSum = 0.0;
        _tickCount = 0;
    }

    ImGui::Begin("Stats");
    ImGui::Text("FPS: %.1f", static_cast<double>(_displayedFps));
    ImGui::Text("TPS: %.1f", _displayedTPS);
    ImGui::Text("Tick: %.2f ms", _displayedMsPerTick);
    ImGui::End();
}

void Hud::recordTick(double tickDurationSeconds)
{
    _tickTimeSum += tickDurationSeconds;
    _tickCount++;
}
