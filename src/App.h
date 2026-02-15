#ifndef APP_H
#define APP_H

#include <filesystem>
#include <luajit.h>
#include <vector>

void logError(const std::string& msg);

const std::string& Lime2DVersion();

#define LIME2D_RELEASE_NUMBER 2
#define LIME2D_VERSION Lime2DVersion()

class App
{
public:
    inline static bool shutting_down = false;

    struct Metrics
    {
        long long start_time;
        int renders;
        int draws;
        int ssbo_updates;
        int buffer_swaps;
    }metrics = {};

    ~App();

    void setStartupFiles(std::vector<std::filesystem::path> files);

    void setColor(float r, float g, float b, bool on = true);

    void run();
    void update(float dt); // Gets called every frame

    void shutdown(int exit_code = 0);
    void fatal(const char* error_msg = nullptr);

private:
    void cleanup();

    std::vector<std::filesystem::path> startupFiles;
};

class FatalStream {
public:
    FatalStream() = default;

    // Trigger the fatal error when this temporary object is destroyed
    ~FatalStream();

    // Overload the << operator to accept any type
    template <typename T>
    FatalStream& operator<<(const T& val) {
        oss << val;
        return *this;
    }

private:
    std::ostringstream oss;
};

#define APP_FATAL FatalStream() << "[" << __FUNCTION__ << "] "

/* Globally accessible objects */
extern App app;
extern class Window window;
extern class Renderer renderer;
extern class Screen* screen; // The currently active screen
extern class ScreenInfo info_screen;
extern class LuaHost lua;
extern class ScreenLua lua_screen;
extern class ScreenInfo console_screen;

#endif