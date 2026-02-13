#include "misc.h"

#include <iostream>

bool PathDeduplicator::tryAdd(const fs::path& p)
{
    fs::path n = makeAbsNorm(p);
    std::string key = pathToKeyUtf8(n);
    if (seen.insert(key).second)
    {
        results.push_back(n);
        return true;
    }
    return false;
}

std::vector<fs::path> PathDeduplicator::getSorted()
{
    std::sort(results.begin(), results.end());
    return std::move(results);
}

void cout(const char* s, int n, bool newline) { std::cout << s << n << (newline ? "\n" : ""); }
void cout(const char* s, bool newline) { std::cout << s << (newline ? "\n" : ""); }
void cout(int n, bool newline) { std::cout << n << (newline ? "\n" : ""); }

void swap(int& a, int& b) { int temp = a; a = b; b = temp; }

int clamp(int value, int min, int max)
{
    return value < min ? min : value > max ? max : value;
}

float clamp(float value, float min, float max)
{
    return value < min ? min : value > max ? max : value;
}

int wrap(int value, int min, int max)
{
    int range = max - min + 1;

    if (range <= 0) throw std::invalid_argument("wrap: max cannot be less than min!");
    if (range == 1) return min;

    return ((value - min) % range + range) % range + min;
}

std::string toLower(std::string s)
{
    for (char& ch : s) ch = (char)std::tolower((unsigned char)ch);
    return s;
}

bool hasExtension(const std::filesystem::path& p, std::string ext)
{
    return toLower(p.extension().string()) == "." + toLower(ext);
}

fs::path makeAbsNorm(const fs::path& p)
{
    std::error_code ec;
    fs::path abs = fs::absolute(p, ec);
    if (ec) abs = p;
    return abs.lexically_normal();
}

std::string stripAllWhitespace(std::string s)
{
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s)
        if (!std::isspace(c)) out.push_back((char)c);
    return out;
}

bool isDotHiddenName(const fs::path& p)
{
    std::string name = p.filename().string();
    return !name.empty() && name[0] == '.';
}

std::string pathToKeyUtf8(const fs::path& p)
{
#ifdef _WIN32
    std::u8string u8 = p.u8string();
    return std::string((const char*)u8.data(), (const char*)u8.data() + u8.size());
#else
    return p.string();
#endif
}

bool readWholeFile(const fs::path& path, std::string& out)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end);
    std::streamoff len = f.tellg();
    if (len < 0) return false;
    f.seekg(0, std::ios::beg);
    out.resize((size_t)len);
    if (len > 0) f.read(&out[0], len);
    return true;
}

bool hasUtf8BomPrefix(const std::string& s)
{
    return s.size() >= 3 &&
        (unsigned char)s[0] == 0xEF &&
        (unsigned char)s[1] == 0xBB &&
        (unsigned char)s[2] == 0xBF;
}