#pragma once
#include <string>
#include <streambuf>
#include <iostream>

// Custom streambuf that captures output and optionally forwards to original
class CapturingStreambuf : public std::streambuf
{
public:
    CapturingStreambuf(std::streambuf* original, std::string& buffer)
        : original(original), buffer(buffer) {}

protected:
    int overflow(int c) override;
    std::streamsize xsputn(const char* s, std::streamsize n) override;

private:
    std::streambuf* original;
    std::string& buffer;
};

namespace ConsoleCapture
{
    void init(); // Redirects std::cout
    const std::string& get();
    void clear();
    void release();
}