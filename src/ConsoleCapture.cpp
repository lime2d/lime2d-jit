#include "ConsoleCapture.h"

static std::string g_captureBuffer;
static std::unique_ptr<CapturingStreambuf> g_capturingBuf;
static std::streambuf* g_originalCoutBuf = nullptr;

int CapturingStreambuf::overflow(int c)
{
    if (c != EOF)
    {
        buffer += static_cast<char>(c);
        if (original)
            original->sputc(static_cast<char>(c));
    }
    return c;
}

std::streamsize CapturingStreambuf::xsputn(const char* s, std::streamsize n)
{
    buffer.append(s, static_cast<size_t>(n));
    if (original)
        original->sputn(s, n);
    return n;
}

namespace ConsoleCapture
{
    void init()
    {
        if (g_capturingBuf) return; // Already initialized

        g_originalCoutBuf = std::cout.rdbuf();
        g_capturingBuf = std::make_unique<CapturingStreambuf>(g_originalCoutBuf, g_captureBuffer);
        std::cout.rdbuf(g_capturingBuf.get());
    }

    const std::string& get()
    {
        return g_captureBuffer;
    }

    void clear()
    {
        g_captureBuffer.clear();
    }

    void release()
    {
        if (g_originalCoutBuf)
        {
            std::cout.rdbuf(g_originalCoutBuf);
            g_originalCoutBuf = nullptr;
        }
        g_capturingBuf.reset();
    }
}