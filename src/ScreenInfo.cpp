#include "ScreenInfo.h"

#include "App.h"
#include "Window.h"
#include "misc.h"

#include <glfw/glfw3.h>

ScreenInfo::ScreenInfo(const char* label) : Screen(label) {}

void ScreenInfo::setKind(Kind k)
{
    kind = k;
    redraw = true;
}

void ScreenInfo::setTitle(std::string t)
{
    title = std::move(t);
    redraw = true;
}

void ScreenInfo::setMessage(std::string msg)
{
    std::replace(msg.begin(), msg.end(), '\t', ' ');
    message = std::move(msg);
    scroll = 0;
    redraw = true;
}

void ScreenInfo::setScroll(int amount)
{
    scroll = amount;
    redraw = true;
}

void ScreenInfo::setInfo(std::string msg)
{
    setKind(Kind::Info);
    setTitle("--  I N F O  --");
    setMessage(std::move(msg));
}

void ScreenInfo::setError(std::string msg)
{
    setKind(Kind::Error);
    setTitle("--  E R R O R  --");
    setMessage(std::move(msg));
}

void ScreenInfo::onSetActive(bool /*initial*/)
{
    redraw = true;
}

void ScreenInfo::update(float /*dt*/)
{
    // no-op
}

bool ScreenInfo::key_event(int key, int /*scancode*/, int action, int mods)
{
    if (key == GLFW_KEY_F11 && action == GLFW_RELEASE)
    {
        window.toggleFullscreen();
        return true;
    }

    if (action != GLFW_PRESS && action != GLFW_REPEAT) return false;

    // Ctrl+X quit
    if ((mods & GLFW_MOD_CONTROL) && key == GLFW_KEY_X)
    {
        app.shutdown(0);
        return true;
    }

    if (key == GLFW_KEY_ESCAPE)
    {
        if (prev)
        {
            screen = prev;
            screen->redraw = true;
            prev = nullptr;
        }
        else
        {
            app.shutdown(0);
        }
        return true;
    }

    // Scroll long messages
    if (key == GLFW_KEY_UP)
    {
        scroll--;
        return true;
    }
    else if (key == GLFW_KEY_DOWN)
    {
        scroll++;
        return true;
    }
    else if (key == GLFW_KEY_PAGE_UP || key == GLFW_KEY_LEFT)
    {
        scroll -= 16;
        return true;
    }
    else if (key == GLFW_KEY_PAGE_DOWN || key == GLFW_KEY_RIGHT)
    {
        scroll += 16;
        return true;
    }
    else if (key == GLFW_KEY_HOME)
    {
        scroll = 0;
        return true;
    }
    else if (key == GLFW_KEY_END)
    {
        scroll = 1 << 30;
        return true;
    }

    return false;
}

void ScreenInfo::draw()
{
    clear(false);

    const int start_row = 3;
    const int start_col = 2;
    const int max_rows = rows - (start_row + 3);
    const int max_cols = cols - 4;

    if (max_rows <= 0 || max_cols <= 0)
        return;

    const char* text = message.empty() ? "(no details)" : message.c_str();

    Cursor saved = cursor;

    locate(start_row, start_col);
    int zero_scroll = 0;
    int total_lines = wrap(text, 4096, max_cols, zero_scroll, true, /*test*/true);
    int max_scroll = (total_lines > max_rows) ? (total_lines - max_rows) : 0;
    scroll = clamp(scroll, 0, max_scroll);

    // Borders
    textBox(0, 0, rows, cols);
    locate(start_row - 1, 0); print(195);
    locate(start_row - 1, cols - 1); print(180);
    locate(rows - 3, 0); print(195);
    locate(rows - 3, cols - 1); print(180);
    locate(start_row - 1, 1); repeat(196, 78);
    locate(rows - 3, 1); repeat(196, 78);

    center(title.c_str(), 1);

    const char* footer;
    if (this == &console_screen)
    {
        footer = "Esc: Back   Up/Down: Scroll   Ctrl+X: Quit";
    }
    else
    {
        footer = prev
            ? "Esc: Back   Up/Down: Scroll   F12: Console"
            : "Esc: Quit   Up/Down: Scroll   F12: Console";
    }

    center(footer, rows - 2, false);

    if (max_scroll > 0) // Draw decorated scrollbar
    {
        locate(3, cols - 1);
        print(24, true); // Up arrow
        locate(rows - 4, cols - 1);
        print(25, true); // Down arrow
        scrollbarV(4, cols - 1, rows - 8, scroll, max_scroll, max_rows);
    }

    // Draw wrapped message with scrolling applied (use a local copy because wrap mutates scrolling)
    locate(start_row, start_col);
    int s = scroll;
    wrap(text, max_rows, max_cols, s, true, /*test*/false);

    cursor = saved;
}