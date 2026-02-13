#ifndef MONOSPACE_MONOCHROME_PIXEL_FONT_H
#define MONOSPACE_MONOCHROME_PIXEL_FONT_H

#define MAX_GLYPH_HEIGHT 16

struct Glyph
{
    unsigned char row[MAX_GLYPH_HEIGHT];
};

class MonospaceMonochromePixelFont
{
public:
    int num_glyphs;
    int glyph_width;
    int glyph_height;

    Glyph* glyphs;

    MonospaceMonochromePixelFont(int num_glyphs, int glyph_width, int glyph_height);
    ~MonospaceMonochromePixelFont();
};

#endif