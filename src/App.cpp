#include "App.h"
#include "ancillary.h"
#include "ConsoleCapture.h"
#include "FusedArchive.h"
#include "LuaHost.h"
#include "misc.h"
#include "Renderer.h"
#include "Screen.h"
#include "ScreenInfo.h"
#include "ScreenLua.h"
#include "Window.h"

#include <iostream>

static const char* kErrorLogFile = "error.log";
static fs::path g_errorLogPath;

static void clearErrorLog(bool remove)
{
    if (g_errorLogPath.empty())
        g_errorLogPath = fs::absolute(fs::current_path() / kErrorLogFile);

    if (remove)
    {
        std::error_code ec;
        fs::remove(g_errorLogPath, ec);
    }
    else
    {
        std::ofstream f(g_errorLogPath, std::ios::trunc);
    }
}

void logError(const std::string& msg)
{
    if (g_errorLogPath.empty())
        g_errorLogPath = fs::absolute(fs::current_path() / kErrorLogFile);

    std::ofstream f(g_errorLogPath, std::ios::app);
    if (!f) return;
    f << msg << "\n";
}

const std::string& Lime2DVersion()
{
    static const std::string value = [] {
        std::string release = LUA_RELEASE;
        auto pos = release.find(' ');
        std::string stripped = (pos == std::string::npos)
            ? release
            : release.substr(pos + 1);

        return "Lime2D " + stripped +
            "." + std::to_string(LIME2D_RELEASE_NUMBER) + " (with " + release +
            " + " + LUAJIT_VERSION + ")";
        }();

    return value;
}

// ============================================================================
// Path Utilities
// ============================================================================

static std::string formatPathList(const std::vector<fs::path>& paths)
{
    std::ostringstream oss;
    for (const auto& p : paths)
        oss << " - " << p.string() << "\n";
    return oss.str();
}

// ============================================================================
// File Qualification Helpers
// ============================================================================

static bool fileHasMainScriptMarker(const fs::path& p)
{
    std::ifstream f(p);
    if (!f) return false;

    std::string line;
    if (!std::getline(f, line)) return false;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (hasUtf8BomPrefix(line)) line.erase(0, 3);

    return stripAllWhitespace(line) == "--MAINSCRIPT";
}

static bool qualifiesAsMainScriptFile(const fs::path& p)
{
    if (isDotHiddenName(p)) return false;
    if (!hasExtension(p, "lua")) return false;

    std::error_code ec;
    if (!fs::exists(p, ec) || ec) return false;
    if (!fs::is_regular_file(p, ec) || ec) return false;

    return fileHasMainScriptMarker(p);
}

// ============================================================================
// File Collection Functions
// ============================================================================

static std::vector<fs::path> collectDroppedRegularFiles(
    const std::vector<fs::path>& droppedRoots,
    std::vector<std::string>* outWarnings = nullptr)
{
    PathDeduplicator dedup;

    for (const auto& rootIn : droppedRoots)
    {
        fs::path root = makeAbsNorm(rootIn);
        std::error_code ec;

        if (fs::is_regular_file(root, ec) && !ec)
        {
            // Collect even dot-hidden files here; qualification rules filter later
            dedup.tryAdd(root);
            continue;
        }

        ec.clear();
        if (!fs::is_directory(root, ec) || ec)
        {
            if (outWarnings)
            {
                std::error_code ec2;
                bool exists = fs::exists(root, ec2);
                if (!ec2 && !exists)
                    outWarnings->push_back("Dropped path does not exist: " + root.string());
            }
            continue;
        }

        if (isDotHiddenName(root))
            continue;

        walkDirectoryRecursively(
            root,
            [&](const fs::path& p) {
                if (!isDotHiddenName(p))
                    dedup.tryAdd(p);
            },
            [](const fs::path& p) {
                return !isDotHiddenName(p);
            },
            outWarnings
        );
    }

    return dedup.getSorted();
}

static std::vector<fs::path> collectMarkedMainScriptsRecursively(
    const fs::path& rootDir,
    std::vector<std::string>* outWarnings = nullptr)
{
    PathDeduplicator dedup;

    std::error_code ec;
    if (!fs::exists(rootDir, ec) || ec) return {};
    if (!fs::is_directory(rootDir, ec) || ec) return {};
    if (isDotHiddenName(rootDir)) return {};

    walkDirectoryRecursively(
        rootDir,
        [&](const fs::path& p) {
            if (isDotHiddenName(p)) return;
            if (!hasExtension(p, "lua")) return;
            if (!fileHasMainScriptMarker(p)) return;
            dedup.tryAdd(p);
        },
        [](const fs::path& p) {
            return !isDotHiddenName(p);
        },
        outWarnings
    );

    return dedup.getSorted();
}

// ============================================================================
// Main Script Resolution
// ============================================================================

static fs::path resolveDroppedMainScriptOrThrowOnAmbiguity(
    const std::vector<fs::path>& droppedRegularFiles)
{
    std::vector<fs::path> droppedMain;

    for (const auto& p : droppedRegularFiles)
    {
        if (qualifiesAsMainScriptFile(p))
            droppedMain.push_back(p);
    }

    if (droppedMain.size() > 1)
    {
        std::string msg =
            "Multiple main scripts were provided (including inside dropped folders).\n\n"
            "Only one main script is allowed.\n\n"
            "Qualifying dropped scripts:\n" + formatPathList(droppedMain) +
            "\nTip: The first line of the main script must be '-- MAINSCRIPT'.\n"
            "Note: Dot-hidden files/directories (name starts with '.') are skipped.";
        throw std::runtime_error(msg);
    }

    if (droppedMain.size() == 1)
        return droppedMain[0];

    return fs::path();
}

static fs::path scanExeDirForMainScript(
    std::vector<std::string>* outWarnings,
    std::string* outNoneMessage)
{
    std::vector<fs::path> scanned = collectMarkedMainScriptsRecursively(fs::current_path(), outWarnings);

    if (scanned.size() == 1)
        return scanned[0];

    if (scanned.size() > 1)
    {
        std::string msg =
            "Multiple main scripts were found in the EXE directory (recursive scan).\n\n"
            "Please disambiguate via command line argument or drag-and-dropping onto EXE.\n\n"
            "Qualifying scripts:\n" + formatPathList(scanned) +
            "\nTip: The first line of the main script must be '-- MAINSCRIPT'.\n"
            "Note: Dot-hidden files/directories (name starts with '.') are skipped.";
        throw std::runtime_error(msg);
    }

    if (outNoneMessage)
    {
        *outNoneMessage =
            "No main script found.\n\n"
            "How to fix:\n"
            "  1) Put a *.lua in the EXE folder with '-- MAINSCRIPT' as its first line.\n"
            "     OR\n"
            "  2) Drag & drop a qualifying *.lua onto the EXE.\n"
            "     OR\n"
            "  3) Drag & drop a folder that contains a qualifying *.lua onto the EXE.\n\n"
            "Note: Dot-hidden scripts and folders (name starts with '.') don't qualify.\n";
    }

    return fs::path();
}

// ============================================================================
// Script Execution
// ============================================================================

static void executeMainScript(const fs::path& mainScript, LuaHost& luaHost,
    const std::vector<fs::path>& startupFiles, ScreenLua& luaScreen, Window& wnd)
{
    fs::path absMainScript = fs::absolute(mainScript);

    // Set working directory to the main script's folder (for relative modules/data)
    fs::path scriptDir = absMainScript.parent_path();
    std::error_code ec;
    fs::current_path(scriptDir, ec);
    if (ec)
        throw std::runtime_error("Failed to set working directory to main script directory:\n  " +
            scriptDir.string() + "\nReason: " + ec.message());

    luaHost.init();
    luaHost.setArgv(startupFiles);
    ConsoleCapture::init();
    cout("Loading resolved script...");
    luaHost.loadAppScript(absMainScript);

    wnd.show(&luaScreen);
}

// Resolves startup files to a main script path.
// Returns empty path if info_screen should be shown instead (e.g., txt-image processing).
// Throws on errors (ambiguous scripts, etc.).
static fs::path resolveMainScript(
    const std::vector<fs::path>& startupFiles,
    std::vector<std::string>& scanWarnings,
    ScreenInfo& infoScreen,
    Window& wnd)
{
    std::vector<fs::path> droppedRegularFiles = collectDroppedRegularFiles(startupFiles, &scanWarnings);
    for (const auto& w : scanWarnings)
        logError(w);

    if (!droppedRegularFiles.empty())
    {
        // Check if dropped files contain a main script
        fs::path mainScript = resolveDroppedMainScriptOrThrowOnAmbiguity(droppedRegularFiles);
        if (!mainScript.empty())
            return mainScript;

        // No main script in dropped files - check for txt-image data
        if (processTxtImageFiles(droppedRegularFiles, scanWarnings, infoScreen, wnd))
            return fs::path(); // Info screen shown, no script to run
    }

    // Scan EXE directory
    std::string noneMessage;
    fs::path found = scanExeDirForMainScript(&scanWarnings, &noneMessage);
    for (const auto& w : scanWarnings)
        logError(w);

    if (found.empty())
        throw std::runtime_error(noneMessage);

    return found;
}

// ============================================================================
// Fused Archive Support
// ============================================================================

static bool contentHasMainScriptMarker(const std::string& content)
{
    std::string firstLine;
    size_t nlPos = content.find('\n');
    firstLine = (nlPos != std::string::npos) ? content.substr(0, nlPos) : content;
    if (!firstLine.empty() && firstLine.back() == '\r')
        firstLine.pop_back();
    if (hasUtf8BomPrefix(firstLine))
        firstLine.erase(0, 3);
    return stripAllWhitespace(firstLine) == "--MAINSCRIPT";
}

static std::string findMainScriptInArchive()
{
    auto files = FusedArchive::listFiles();
    std::vector<std::string> mainScripts;

    for (const auto& name : files)
    {
        // Must be a .lua file
        if (!hasExtension(fs::path(name), "lua"))
            continue;

        // Skip dot-hidden files (consistent with disk-based scanning)
        size_t slashPos = name.find_last_of('/');
        std::string basename = (slashPos != std::string::npos) ? name.substr(slashPos + 1) : name;
        if (!basename.empty() && basename[0] == '.')
            continue;

        // Check for MAINSCRIPT marker
        std::string content;
        if (!FusedArchive::readFile(name, content))
            continue;

        if (contentHasMainScriptMarker(content))
            mainScripts.push_back(name);
    }

    if (mainScripts.empty())
        throw std::runtime_error(
            "Fused archive contains no main script.\n\n"
            "Tip: The first line of the main script must be '-- MAINSCRIPT'.\n");

    if (mainScripts.size() > 1)
    {
        std::string msg = "Fused archive contains multiple main scripts:\n\n";
        for (const auto& s : mainScripts)
            msg += " - " + s + "\n";
        msg += "\nOnly one main script is allowed.\n";
        throw std::runtime_error(msg);
    }

    return mainScripts[0];
}

static void executeFusedScript(LuaHost& luaHost,
    const std::vector<fs::path>& startupFiles,
    ScreenLua& luaScreen, Window& wnd)
{
    std::string mainScriptName = findMainScriptInArchive();

    luaHost.init();
    luaHost.setArgv(startupFiles);
    ConsoleCapture::init();
    cout("Loading fused script: ", false);
    cout(mainScriptName.c_str());
    luaHost.loadFusedScript(mainScriptName);

    wnd.show(&luaScreen);
}

// ============================================================================
// App Implementation
// ============================================================================

App::~App()
{
    float dt = static_cast<float>(difftime(time(0), metrics.start_time));

    if (dt > 1.0f)
    {
        char rps[16], dps[16], bups[16];

        sprintf_s(rps, "%.1f", metrics.renders / dt);
        sprintf_s(dps, "%.1f", metrics.draws / dt);
        sprintf_s(bups, "%.1f", metrics.ssbo_updates / dt);
        std::cout << "Metrics:\n Renders: " << rps << "/s\n Draws:   " << dps << "/s";
        if (metrics.ssbo_updates == metrics.draws)
            std::cout << std::endl;
        else
            std::cout << "\n SSBO Updates: " << bups << "/s\n";
    }

    cout("Exiting.");
}

void App::setStartupFiles(std::vector<std::filesystem::path> files)
{
    startupFiles = std::move(files);
}

void App::setColor(float r, float g, float b, bool on)
{
    if (on) renderer.setFgColor(r, g, b);
    else renderer.setBgColor(r, g, b);
}

void App::run()
{
    metrics.start_time = time(0);

    // Bind error.log to EXE-dir (or initial CWD) BEFORE we possibly change CWD to the script dir.
    g_errorLogPath = fs::absolute(fs::current_path() / kErrorLogFile);
    clearErrorLog(true);

    try
    {
        if (FusedArchive::isFused())
        {
            executeFusedScript(lua, startupFiles, lua_screen, window);
        }
        else
        {
            std::vector<std::string> scanWarnings;
            fs::path mainScript = resolveMainScript(startupFiles, scanWarnings, info_screen, window);

            if (!mainScript.empty())
                executeMainScript(mainScript, lua, startupFiles, lua_screen, window);
        }
    }
    catch (const std::exception& e)
    {
        ConsoleCapture::init();
        cout("Unable to resolve script!");
        logError(e.what());
        info_screen.setError(e.what());
        window.show(&info_screen);
    }

    cout("Entering main loop...");

    double t = glfwGetTime();
    float dt = 1.0f / window.refresh_rate_at_startup; // Provide reasonable initial dt

    while (!window.shouldClose()) // Main loop
    {
        update(dt);

        // We could simply draw every iteration but this is more efficient
        if (screen && screen->redraw/* || metrics.buffer_swaps % window.refresh_rate_at_startup == 0*/)
            screen->_draw();

        if (Screen::render_frames)
            renderer.render();

        window.swapBuffers();
        window.pollEvents();

        double pt = t;
        dt = static_cast<float>((t = glfwGetTime()) - pt);
    }

    shutdown();
}

void App::update(float dt)
{
    if (screen) screen->update(dt);
}

void App::cleanup()
{
    cout("Performing cleanup...");

    lua.shutdown();
    FusedArchive::shutdown();

    renderer.cleanup();
    window.cleanup();
    Screen::_cleanup();

    glfwTerminate();

    cout(" Cleanup complete!");

    ConsoleCapture::release();
}

void App::shutdown(int exit_code)
{
    shutting_down = true;
    std::cout << "Application shutting down" << (exit_code ? " unexpectedly" : "") << "...\n";
    cleanup();
    exit(exit_code); // Intended sole exit point for entire application
}

void App::fatal(const char* error_msg)
{
    std::string msg = "Fatal error!";

    std::string activeSection = lua.getActiveProfilerSection();
    if (!activeSection.empty())
    {
        msg += "\n[Profiler section: ";
        msg += activeSection;
        msg += "]";
    }

    if (error_msg && *error_msg)
    {
        msg += "\n";
        msg += error_msg;
    }

    std::cerr << msg << std::endl;
    logError(msg);

    static bool try_rendering_msg = Renderer::ready;

    if (try_rendering_msg)
    {
        try_rendering_msg = false;

        try
        {
            info_screen.setKind(ScreenInfo::Kind::Error);
            info_screen.setTitle("--  F A T A L  --");
            info_screen.setMessage(msg);

            window.show(&info_screen);

            // Keep rendering until user exits (Esc/Ctrl+X) or closes the window.
            while (!window.shouldClose())
            {
                if (screen && screen->redraw)
                    screen->_draw();

                if (Screen::render_frames)
                    renderer.render();

                window.swapBuffers();
                window.pollEvents();
            }
        }
        catch (...)
        {
            // Fall through to shutdown if the fatal screen itself fails.
        }
    }

    shutdown(EXIT_FAILURE);
}

#pragma warning(push)
#pragma warning(disable: 4722)
FatalStream::~FatalStream()
{
    app.fatal(oss.str().c_str());
}
#pragma warning(pop)

/* Globally accessible objects */
App app;
Window window("Lime2D"); // Lua Integrated Monochromatic Engine
Renderer renderer;
Screen* screen = 0;
ScreenInfo info_screen("Info Screen");
ScreenInfo console_screen("Console Screen");
LuaHost lua;
ScreenLua lua_screen("Lua Screen");