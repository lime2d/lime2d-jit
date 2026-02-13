#include "App.h"
#include "gl.h"
#include "LuaHost.h"
#include "ScreenInfo.h"
#include "ScreenLua.h"

ScreenLua::ScreenLua(const char* label) : Screen(label) {}

void ScreenLua::onSetActive(bool initial)
{
    lua.callOnSetActive(initial);
}

void ScreenLua::update(float dt)
{
    lua.callUpdate(dt);
}

void ScreenLua::draw()
{
    lua.callDraw();
}

void ScreenLua::showSystemInfoScreen()
{
    info_screen.prev = this;

    std::ostringstream oss;
    oss << "Version: " << LIME2D_VERSION << "\n\n";
    oss << "Code Page 437\n";
    oss << "-------------\n";

    for (int i = 0; i < 256; ++i)
    {
        oss << "0x" << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << i;
        oss << " | " << std::dec << std::setw(3) << std::setfill('0') << i << " ";

        if (i == 0 || i == 10 || i == 9)
            oss << " ";
        else
            oss << (char)i;

        // Layout: 4 entries per row
        if ((i + 1) % 4 == 0)
            oss << "\n";
        else
            oss << "    ";
    }

    info_screen.setKind(ScreenInfo::Kind::Info);
    info_screen.setTitle("--  S Y S T E M   I N F O  --");
    info_screen.setMessage(oss.str());

    // Switch to info screen
    screen = &info_screen;
    info_screen.redraw = true;
}

bool ScreenLua::key_event(int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_F10 && action == GLFW_PRESS)
    {
        showSystemInfoScreen();
        return true;
    }

    if (action == GLFW_PRESS || action == GLFW_REPEAT)
        return lua.callKeyPressed(key, scancode, action == GLFW_REPEAT);
    else if (action == GLFW_RELEASE)
        return lua.callKeyReleased(key, scancode);
    return false;
}

bool ScreenLua::char_event(unsigned int c)
{
    return lua.callTextInput(c);
}