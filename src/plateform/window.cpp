#include "window.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <chrono>

#include "log.h"

Window::Window(uint32_t width, uint32_t height, const std::string& title)
{
    _width = width;
    _height = height;
    _title = title;

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        VoxisLog::critical("Failed to init SDL: {}", SDL_GetError());

    VoxisLog::info("SDL initialized");

    _handle = SDL_CreateWindow(_title.c_str(),
                               SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED,
                               static_cast<int>(_width),
                               static_cast<int>(_height),
                               SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (!_handle)
        VoxisLog::critical("Failed to create the SDL window: {}", SDL_GetError());

    SDL_SetWindowGrab(_handle, SDL_TRUE);
    SDL_SetRelativeMouseMode(SDL_TRUE);

    VoxisLog::info("Window created");
}

Window::~Window()
{
    VoxisLog::info("Destroying window");

    SDL_DestroyWindow(_handle);
    SDL_Quit();
}

VkSurfaceKHR Window::getVulkanSurface(VkInstance instance)
{
    VkSurfaceKHR surface;
    if (SDL_Vulkan_CreateSurface(_handle, instance, &surface) != SDL_TRUE)
        VoxisLog::critical("Failed to create vulkan surface: {}", SDL_GetError());
    return surface;
}

const char** Window::getVulkanInstanceExtensions(uint32_t* count)
{
    unsigned int extensionCount = 0;
    SDL_Vulkan_GetInstanceExtensions(_handle, &extensionCount, nullptr);

    _vulkanExtensions.resize(extensionCount);
    SDL_Vulkan_GetInstanceExtensions(_handle, &extensionCount, _vulkanExtensions.data());

    *count = extensionCount;
    return _vulkanExtensions.data();
}

void Window::update()
{
    using clock = std::chrono::steady_clock;
    auto tStart = clock::now();

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
        case SDL_QUIT:
            _shouldClose = true;
            break;

        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
            {
                int w = event.window.data1;
                int h = event.window.data2;
                if (w == 0 || h == 0)
                    break;

                _width = static_cast<uint32_t>(w);
                _height = static_cast<uint32_t>(h);
            }
            break;

        case SDL_KEYDOWN:
            if (!event.key.repeat && event.key.keysym.scancode < MAX_KEYS)
            {
                _keys[static_cast<size_t>(event.key.keysym.scancode)] = true;
                _keysPressed[static_cast<size_t>(event.key.keysym.scancode)] = true;
            }
            break;

        case SDL_KEYUP:
            if (event.key.keysym.scancode < MAX_KEYS)
                _keys[static_cast<size_t>(event.key.keysym.scancode)] = false;
            break;

        case SDL_MOUSEMOTION:
            _dx += event.motion.xrel;
            _dy += -event.motion.yrel;
            _mouseX = event.motion.x;
            _mouseY = event.motion.y;
            break;

        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button < MAX_BUTTONS)
            {
                _buttons[event.button.button] = true;
                _buttonsPressed[event.button.button] = true;
            }
            break;

        case SDL_MOUSEBUTTONUP:
            if (event.button.button < MAX_BUTTONS)
                _buttons[event.button.button] = false;
            break;

        case SDL_MOUSEWHEEL:
            _scrollY += event.wheel.y;
            break;

        default:
            break;
        }
    }

    auto tEnd = clock::now();
    double ms = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
    if (ms > 50.0)
        VoxisLog::info("SDL_PollEvent took {:.3f}ms", ms);
}

bool Window::shouldClose()
{
    return _shouldClose;
}

double Window::getTime()
{
    return static_cast<double>(SDL_GetPerformanceCounter()) / static_cast<double>(SDL_GetPerformanceFrequency());
}

//* ======= INPUT HANDLING =========

namespace
{
SDL_Scancode toSdlScancode(Key key)
{
    switch (key)
    {
    case Key::W:
        return SDL_SCANCODE_W;
    case Key::A:
        return SDL_SCANCODE_A;
    case Key::S:
        return SDL_SCANCODE_S;
    case Key::D:
        return SDL_SCANCODE_D;
    case Key::Esc:
        return SDL_SCANCODE_ESCAPE;
    case Key::Space:
        return SDL_SCANCODE_SPACE;
    case Key::LShift:
        return SDL_SCANCODE_LSHIFT;
    case Key::LCtrl:
        return SDL_SCANCODE_LCTRL;
    case Key::F3:
        return SDL_SCANCODE_F3;
    }

    return SDL_SCANCODE_UNKNOWN;
}

int toSdlButton(MouseButton button)
{
    switch (button)
    {
    case MouseButton::Left:
        return SDL_BUTTON_LEFT;
    case MouseButton::Right:
        return SDL_BUTTON_RIGHT;
    case MouseButton::Middle:
        return SDL_BUTTON_MIDDLE;
    }

    return -1;
}
} // namespace

bool Window::isKeyPressed(Key key) const
{
    return _keys[static_cast<size_t>(toSdlScancode(key))];
}

bool Window::consumeKeyPress(Key key)
{
    size_t scancode = static_cast<size_t>(toSdlScancode(key));
    bool value = _keysPressed[scancode];
    _keysPressed[scancode] = false;
    return value;
}

bool Window::isButtonPressed(MouseButton button) const
{
    return _buttons[static_cast<size_t>(toSdlButton(button))];
}

bool Window::consumeButtonPress(MouseButton button)
{
    size_t code = static_cast<size_t>(toSdlButton(button));
    bool value = _buttonsPressed[code];
    _buttonsPressed[code] = false;
    return value;
}

double Window::consumeDx()
{
    double tmp = _dx;
    _dx = 0.0;
    return tmp;
}

double Window::consumeDy()
{
    double tmp = _dy;
    _dy = 0.0;
    return tmp;
}

double Window::consumeScroll()
{
    double tmp = _scrollY;
    _scrollY = 0.0;
    return tmp;
}

glm::vec2 Window::getCursorPos()
{
    return glm::vec2(_mouseX, _mouseY);
}

void Window::resetMouse()
{
    _dx = 0.0;
    _dy = 0.0;
}

void Window::setCursorEnabled(bool enabled)
{
    _cursorToggle = enabled;
    SDL_SetWindowGrab(_handle, enabled ? SDL_FALSE : SDL_TRUE);
    SDL_SetRelativeMouseMode(enabled ? SDL_FALSE : SDL_TRUE);
    if (!enabled)
        resetMouse();
}

bool Window::isCursorEnabled()
{
    return _cursorToggle;
}

void Window::enableInput()
{
    _inputEnabled = true;
}

void Window::disableInput()
{
    _inputEnabled = false;
    _keysPressed.fill(false);
    _buttonsPressed.fill(false);
}

bool Window::isInputEnabled()
{
    return _inputEnabled;
}
