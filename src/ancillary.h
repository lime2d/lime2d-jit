#pragma once
#include "ScreenInfo.h"
#include "Window.h"

#include <filesystem>

namespace fs = std::filesystem;

struct TxtImageGenResult
{
    bool recognized = false; // valid text-image format
    bool generated = false; // wrote the output lua file successfully

    int w{ 0 }, h{ 0 };

    fs::path sourceTxt;
    fs::path outLua;

    std::string error; // non-empty if recognized but failed to generate
};

bool tryGenerateLuaImageFromTxt(const std::filesystem::path& txtPath, TxtImageGenResult& out);
std::string formatTxtImageGenReport(
    int txtScanned,
    const std::vector<TxtImageGenResult>& results,
    const std::vector<std::string>& scanWarnings);
bool processTxtImageFiles(
    const std::vector<fs::path>& droppedRegularFiles,
    const std::vector<std::string>& scanWarnings,
    ScreenInfo& infoScreen,
    Window& wnd);