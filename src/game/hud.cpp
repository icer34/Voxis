#include "hud.h"

#include <imgui.h>

void Hud::draw(float dt)
{
    _fpsAccumTime += dt;
    _fpsAccumFrames++;

    if (_fpsAccumTime >= FPS_UPDATE_INTERVAL)
    {
        _displayedFps = static_cast<float>(_fpsAccumFrames) / _fpsAccumTime;
        _fpsAccumTime = 0.0f;
        _fpsAccumFrames = 0;
    }

    ImGui::Begin("Stats");
    ImGui::Text("FPS: %.1f", static_cast<double>(_displayedFps));
    ImGui::End();
}
