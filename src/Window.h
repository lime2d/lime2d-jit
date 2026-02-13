#ifndef WINDOW_H
#define WINDOW_H

#define GLFW_INCLUDE_NONE // Needed? What does this do?

#include "gl.h"

class Screen;

class Window
{
public:
    int width;
    int height;
    GLFWwindow* window;

    int refresh_rate_at_startup;

    Window(const char* title);

    void cleanup();
    void setBackgroundColor(float r, float g, float b);
    void setTitle(const char* title);
    void pollEvents();
    void swapBuffers();
    bool shouldClose();
    void show(Screen* screen = 0);
    bool toggleFullscreen();
    bool getFullscreen() const;
    void setFullscreen(bool fullscreen);

private:
    bool isFullscreen;
    GLFWmonitor* monitor;

    struct Rect
    {
        int x, y, w, h;
    }windowed_layout;

    static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
};

#endif