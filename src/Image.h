#pragma once

struct Image
{
    int width = 0;
    int height = 0;
    const unsigned char* pixels = nullptr; // packed 1bpp, row-major, width must be multiple of 8
};