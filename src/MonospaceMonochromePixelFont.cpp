#include "App.h"
#include "MonospaceMonochromePixelFont.h"
#include <iostream>
#include "misc.h"
#include "IBM_VGA8.h"

MonospaceMonochromePixelFont::MonospaceMonochromePixelFont(int num_glyphs, int glyph_width, int glyph_height)
    : num_glyphs(num_glyphs), glyph_width(glyph_width), glyph_height(glyph_height)
{
    if (glyph_width % 8) app.fatal("Glyph width must be a multiple of 8");
    if (glyph_height > MAX_GLYPH_HEIGHT)
    {
        std::cerr << "Glyph height must be " << MAX_GLYPH_HEIGHT << " or less!\n";
        app.fatal();
    }

    glyphs = new Glyph[num_glyphs];

    for (int i = 0; i < num_glyphs; i++)
    {
        Glyph& glyph = glyphs[i];
        for (int r = 0; r < glyph_height; r++)
            glyph.row[r] = IBM_VGA8_packed[i * glyph_height + r];
    }
}

MonospaceMonochromePixelFont::~MonospaceMonochromePixelFont()
{
    delete[] glyphs;
    cout(" MonospaceMonochromePixelFont [ok]");
}