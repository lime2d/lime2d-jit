#include "App.h"
#include "keyboard.h"
#include "LuaHost.h"
#include "misc.h"
#include "Screen.h"
#include "Window.h"

static void window_close_callback(GLFWwindow* glfw_window)
{
    try
    {
        if (lua.invokeQuitCallback())
            glfwSetWindowShouldClose(glfw_window, GLFW_FALSE);
    }
    catch (const std::exception& e)
    {
        logError(std::string("Error in lime.quit callback: ") + e.what());
    }
}

Window::Window(const char* title)
    : width(640), height(360), isFullscreen(false), monitor(0)
{
    cout("Starting application...");

    if (!glfwInit()) app.fatal("Failed to initialize GLFW");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    glfwWindowHint(GLFW_VISIBLE, GL_FALSE); // Delay showing the window until it's ready

    window = glfwCreateWindow(width, height, title, NULL, NULL); // Initial window creation
    if (!window) app.fatal("Failed to create GLFW window");

    glfwMakeContextCurrent(window); // Now we have a context and can initialize GLAD

    cout(" Window [created]");

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) app.fatal("Failed to initialize GLAD");

    glfwSetWindowUserPointer(window, this);
    //glfwSetWindowSizeCallback(window, window_size_callback); // Available if needed
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    glfwGetWindowSize(window, &width, &height);

    int left, top, right, bottom;
    glfwGetWindowFrameSize(window, &left, &top, &right, &bottom);

    int frame_horizontal = left + right;
    int frame_vertical = top + bottom;

    const GLFWvidmode* mode = glfwGetVideoMode(monitor = glfwGetPrimaryMonitor());
    int desktopWidth = mode->width;
    int desktopHeight = mode->height;
    refresh_rate_at_startup = mode->refreshRate;
    glfwSwapInterval(1); // Enable vsync for windowed mode

    if (desktopWidth * 9 >= desktopHeight * 16)
    {
        // 16:9 Aspect ratio or wider
        Screen::_init(width, height);
    }
    else
    {
        // Extend height to take advantage of extra vertical space
        height += 40;

        glfwSetWindowSize(window, width, height);
        glfwGetWindowSize(window, &width, &height);

        Screen::_init(width, height);
    }

    // We want to magnify the canvas as much as possible without having any part of the window
    // (including frame) go offscreen or overlap with global task bars

    int wax, way, waw, wah;
    glfwGetMonitorWorkarea(monitor, &wax, &way, &waw, &wah);

    int max_windowed_canvas_scaling = min((waw - frame_horizontal) / width, (wah - frame_vertical) / height);

    if (max_windowed_canvas_scaling > 1)
    {
        if (width * max_windowed_canvas_scaling >= desktopWidth || height * max_windowed_canvas_scaling >= desktopHeight)
            --max_windowed_canvas_scaling; // Only needed if window is undecorated and the desktop has no global taskbar

        width *= max_windowed_canvas_scaling;
        height *= max_windowed_canvas_scaling;

        glfwSetWindowSize(window, width, height);
        glfwGetWindowSize(window, &width, &height);
    }

    // Center window on desktop without overlapping global taskbars

    int x = (desktopWidth - width) / 2;
    int y = (desktopHeight - height) / 2;

    if (x < wax) x = wax;
    if (y < way) y = way;

    glfwSetWindowPos(window, x, y);
    glfwGetWindowPos(window, &x, &y);
    windowed_layout.x = x;
    windowed_layout.y = y;
    windowed_layout.w = width;
    windowed_layout.h = height;

    glfwSetKeyCallback(window, key_callback); // Detailed tracking of key events
    glfwSetCharCallback(window, char_callback); // Mainly for text input scenarios

    glfwSetWindowCloseCallback(window, window_close_callback);

    setBackgroundColor(0.0f, 0.0f, 0.0f);
}

void Window::cleanup()
{
    glfwDestroyWindow(window);
    cout(" Window [ok]");
}

void Window::setBackgroundColor(float r, float g, float b)
{
    glClearColor(r, g, b, 1.0f);
}

void Window::setTitle(const char* title)
{
    if (title)
        glfwSetWindowTitle(window, title);
}

void Window::pollEvents()
{
    glfwPollEvents();
}

void Window::swapBuffers()
{
    glfwSwapBuffers(window);
    app.metrics.buffer_swaps++;
}

bool Window::shouldClose()
{
    return glfwWindowShouldClose(window);
}

void Window::show(Screen* screen)
{
    if (screen) screen->setActive();
    glfwShowWindow(window);
}

bool Window::toggleFullscreen()
{
    isFullscreen = !isFullscreen;

    if (isFullscreen)
    {
        monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        glfwGetWindowPos(window, &windowed_layout.x, &windowed_layout.y);
        glfwGetWindowSize(window, &windowed_layout.w, &windowed_layout.h);
        glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    }
    else
    {
        glfwSetWindowMonitor(window, NULL, windowed_layout.x, windowed_layout.y, windowed_layout.w, windowed_layout.h, 0);
    }

    glfwSwapInterval(1); // Ensure vsync
    return isFullscreen;
}

bool Window::getFullscreen() const
{
    return isFullscreen;
}

void Window::setFullscreen(bool fullscreen)
{
    if (fullscreen == isFullscreen) return;
    toggleFullscreen();
}

void Window::framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (win)
    {
        win->width = width;
        win->height = height;
        glViewport(0, 0, width, height);
        Screen::render_frames = 3;
    }
}