/* How to Set Up this Project for Visual Studio C++ (completed; keep this block comment for reference)
* 
* 1. Build LuaJIT `lua51.lib` (run `msvcbuild.bat static` via x64 Native Tools)
*   a) Debug:   @set LJCOMPILE=cl /nologo /c /Od /W3 /D_CRT_SECURE_NO_DEPRECATE /D_CRT_STDIO_INLINE=__inline /MDd /Z7
*   b) Release: @set LJCOMPILE=cl /nologo /c /O2 /W3 /D_CRT_SECURE_NO_DEPRECATE /D_CRT_STDIO_INLINE=__inline
* Note: - Save the vc140.pdb files and keep them with respective libs
*       - The luajit.h header gets created during lib build
* 
* 2. File Dependencies (these must be physically present in the project folder)
* lime2d-jit\
*   glad\glad.c, glad.h
*   glfw\glfw3.h, glfw3.lib, glfw3native.h
*   khr\khrplatform.h
*   luajit\lib\Debug\lua51.lib, vc140.pdb
*   luajit\lib\Release\lua51.lib, vc140.pdb
*   luajit\src\lua.h, luajit.h, lualib.h, lauxlib.h, lua.hpp, etc.
* Note: The opengl32.lib is provided by the OS
* 
* 3. Add Source Files to Project (so they compile and link into EXE)
*   a) Create filter groups "glad" and "miniz" at same level as "Source Files"
*   b) Add glad\glad.c to "glad"
*   c) Add miniz\miniz.c to "miniz"
*   d) Add the remaining "regular" source files to "Source Files"
*   e) The "Header Files" filter group isn't needed, you can delete it
* 
* 4. Project Property Pages
* General Properties
*   -> C++ Language Standard
*      > ISO C++ 20 Standard (/std:c++20)
* C/C++
*   -> General
*     -> Additional Include Directories
*        > $(ProjectDir);$(ProjectDir)luajit\src;%(AdditionalIncludeDirectories)
* Linker
*   -> General
*     -> Additional Library Directories
*        > $(ProjectDir)glfw;$(ProjectDir)luajit\lib\$(Configuration)\
*   -> Input
*     -> Additional Dependencies
*        > opengl32.lib;glfw3.lib;lua51.lib;$(CoreLibraryDependencies);%(AdditionalDependencies)
*     -> Ignore Specific Default Libraries
*        > MSVCRT <- Only for Debug Configuration
*        > LIBCMT <- Only for Release Configuration
*   -> System
*     -> SubSystem
*        > Windows (/SUBSYSTEM:WINDOWS) <- Only for Release Configuration
*/

#include "App.h"
#include "FusedArchive.h"
#include "LuaHost.h"

//#include <lua.hpp> // Not sure what this does or if it's useful
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

static void runWithExeAndArgs(const fs::path& exePath, std::vector<fs::path> startupFiles)
{
    // Set working directory to the EXE location (as your engine expects)
    if (!exePath.empty())
    {
        auto absExePath = fs::absolute(exePath);
        auto exeDir = absExePath.parent_path();
        fs::current_path(exeDir);
        lua.setExeDir(exeDir);

        FusedArchive::init(absExePath);
    }

    app.setStartupFiles(std::move(startupFiles));

    try
    {
        app.run();
    }
    catch (const std::exception& e)
    {
        // This is for failures occurring AFTER the engine is already running.
        // Startup selection errors are handled inside App::run() by showing ScreenError.
        app.fatal(e.what());
    }
}

#ifdef _DEBUG

int main(int argc, char** argv)
{
    fs::path exePath;
    if (argc > 0 && argv && argv[0])
        exePath = fs::path(argv[0]);

    std::vector<fs::path> startupFiles;
    for (int i = 1; i < argc; ++i)
        startupFiles.emplace_back(argv[i]);

    runWithExeAndArgs(exePath, std::move(startupFiles));
    return 0;
}

#else

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shellapi.h>

int main(int /*argc*/, char** /*argv*/)
{
    // Not used in Release; WinMain is the entrypoint.
    return 0;
}

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int)
{
    int argc = 0;
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!wargv || argc <= 0)
        return 0;

    fs::path exePath = fs::path(wargv[0]);

    std::vector<fs::path> startupFiles;
    for (int i = 1; i < argc; ++i)
        startupFiles.emplace_back(wargv[i]);

    LocalFree(wargv);

    runWithExeAndArgs(exePath, std::move(startupFiles));
    return 0;
}

#endif