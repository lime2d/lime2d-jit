#ifndef MISC_H
#define MISC_H

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) < (b)) ? (b) : (a))

#include <filesystem>
#include <fstream>
#include <unordered_set>

namespace fs = std::filesystem;

class PathDeduplicator
{
    std::unordered_set<std::string> seen;
    std::vector<fs::path> results;

public:
    bool tryAdd(const fs::path& p);
    std::vector<fs::path> getSorted();
};

// Walks a directory recursively, calling onFile for regular files and onDir for directories.
// onDir should return true to recurse into the directory, false to skip it.
template<typename FileCallback, typename DirCallback>
static void walkDirectoryRecursively(
    const fs::path& root,
    FileCallback onFile,
    DirCallback onDir,
    std::vector<std::string>* outWarnings = nullptr)
{
    std::error_code ec;
    if (!fs::exists(root, ec) || ec) return;
    if (!fs::is_directory(root, ec) || ec) return;

    std::error_code itEc;
    fs::recursive_directory_iterator it(
        root,
        fs::directory_options::skip_permission_denied,
        itEc
    );
    fs::recursive_directory_iterator end;

    for (; it != end; it.increment(itEc))
    {
        if (itEc)
        {
            if (outWarnings)
                outWarnings->push_back(
                    "Folder scan warning at: " + it->path().string() +
                    "\nReason: " + itEc.message());
            itEc.clear();
            continue;
        }

        std::error_code ec2;
        if (it->is_directory(ec2) && !ec2)
        {
            if (!onDir(it->path()))
            {
                it.disable_recursion_pending();
                continue;
            }
        }

        if (it->is_regular_file(ec2) && !ec2)
        {
            onFile(it->path());
        }
    }
}

void cout(const char* s, int n, bool newline = true);
void cout(const char* s = "", bool newline = true);
void cout(int n, bool newline = true);

void swap(int& a, int& b);
int clamp(int value, int min, int max);
float clamp(float value, float min, float max);
int wrap(int value, int min, int max);

std::string toLower(std::string s);
bool hasExtension(const std::filesystem::path& p, std::string ext);
fs::path makeAbsNorm(const fs::path& p);
std::string stripAllWhitespace(std::string s);
bool isDotHiddenName(const fs::path& p);
std::string pathToKeyUtf8(const fs::path& p);
bool readWholeFile(const fs::path& path, std::string& out);
bool hasUtf8BomPrefix(const std::string& s);

#endif