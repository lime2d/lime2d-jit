#include "keyboard.h"
#include "App.h"
#include "Screen.h"
#include "ScreenInfo.h"
#include "ConsoleCapture.h"

static void toggleConsoleScreen()
{
    if (screen == &console_screen)
    {
        // Directly restore paused screen without calling onSetActive
        screen = console_screen.prev;
        screen->redraw = true;
        console_screen.prev = nullptr;
    }
    else
    {
        console_screen.prev = screen;

        console_screen.setKind(ScreenInfo::Kind::Info);
        console_screen.setTitle("--  C O N S O L E   O U T P U T  --");
        console_screen.setMessage(ConsoleCapture::get());

        // Set console as active without going through the full setActive() flow
        // so the paused screen doesn't know about it
        console_screen.setScroll(1 << 30);
        screen = &console_screen;
        console_screen.redraw = true;
    }
}

void key_callback(GLFWwindow* glfw_window, int key, int scancode, int action, int mods)
{
    if (!screen) return;

    // Handle F11 & F12 globally before delegating to the active screen

    if (key == GLFW_KEY_F11 && action == GLFW_RELEASE)
    {
        window.toggleFullscreen();
        return;
    }

    if (key == GLFW_KEY_F12 && action == GLFW_PRESS)
    {
        toggleConsoleScreen();
        return;
    }

    if (action == GLFW_PRESS || action == GLFW_REPEAT)
        screen->redraw = true;

    screen->key_event(key, scancode, action, mods);
}

void char_callback(GLFWwindow* window, unsigned int c)
{
    if (!screen) return;
    screen->redraw = true;
    screen->char_event(c);
}