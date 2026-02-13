#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

// Detects and reads a zip archive appended to the Lime2D executable.
// Enables "fused" distribution:  copy /b lime2d-jit.exe+script.zip FusedApp.exe
class FusedArchive
{
public:
    // Reads the EXE at the given path and checks for an appended zip.
    // If found, extracts all files into memory. Returns true on success.
    static bool init(const fs::path& exePath);

    static bool isFused();
    static bool hasFile(const std::string& name);
    static bool readFile(const std::string& name, std::string& out);
    static std::vector<std::string> listFiles();
    static void shutdown();

private:
    static bool fused_;
    static std::unordered_map<std::string, std::string> files_;

    static std::string normalizePath(const std::string& p);
};