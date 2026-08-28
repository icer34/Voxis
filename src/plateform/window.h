#pragma once

#include <string>
#include <cstdint>
#include <functional>
#include <vector>

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

struct SDL_Window;

enum class Key
{
    W,
    A,
    S,
    D,
    Space,
    LShift,
    LCtrl,
    Esc,
    F3
};

enum class MouseButton
{
    Left,
    Right,
    Middle
};

class Window
{
public:
    Window(uint32_t width, uint32_t height, const std::string& title);
    ~Window();

    // core
    void update();
    bool shouldClose();
    double getTime();

    // graphics related
    VkSurfaceKHR getVulkanSurface(VkInstance instance);
    const char** getVulkanInstanceExtensions(uint32_t* count);
    uint32_t width() const { return _width; }
    uint32_t height() const { return _height; }

    // input related
    bool isKeyPressed(Key key) const;
    bool consumeKeyPress(Key key);

    bool isButtonPressed(MouseButton button) const;
    bool consumeButtonPress(MouseButton button);

    double consumeDx();
    double consumeDy();
    double consumeScroll();
    glm::vec2 getCursorPos();

    void resetMouse();

    void setCursorEnabled(bool enabled);
    bool isCursorEnabled();

    void enableInput();
    void disableInput();
    bool isInputEnabled();

private:
    uint32_t _width = 0;
    uint32_t _height = 0;
    std::string _title;

    SDL_Window* _handle;
    bool _shouldClose = false;
    std::vector<const char*> _vulkanExtensions;

    static constexpr int MAX_KEYS = 350;
    static constexpr int MAX_BUTTONS = 8;

    std::array<bool, MAX_KEYS> _keys{};
    std::array<bool, MAX_KEYS> _keysPressed{};

    std::array<bool, MAX_BUTTONS> _buttons{};
    std::array<bool, MAX_BUTTONS> _buttonsPressed{};

    double _mouseX = 0.0, _mouseY = 0.0;
    double _dx = 0.0, _dy = 0.0;
    double _scrollY = 0.0;
    bool _cursorToggle = false;

    bool _inputEnabled = true;
};