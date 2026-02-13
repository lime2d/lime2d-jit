#ifndef SCREEN_H
#define SCREEN_H

struct Image;

// The static width, height, and pixel buffer of the Screen class define a "Canvas"
// The Canvas is a logical rectangle of pixels and is a key feature of this application
// The logical dimensions of the Canvas are set at startup and do not change
// The Renderer scales the displayed Canvas (by whole integers) as space in the window permits
// This allows for crisp, pixel-perfect magnification of the Canvas

class Screen // Abstract base class
{
public:
    const char* label;

    /* Data fields for the Canvas (all instances of Screen use the same Canvas for drawing) */
    inline static int width = 0, height = 0; // Logical width and height
    inline static unsigned char* pixels = 0; // Monochrome 1-bit-per-pixel buffer
    inline static int text_offset_y = 0; // Vertical offset of text grid for centering
    inline static int rows = 0, cols = 0; // Number of glyph rows and columns
    inline static struct Cursor // Tracks location of glyph cursor
    {
        int row;
        int col;
    }cursor{};

    bool redraw = false; // If set true then draw() will be called

    inline static class MonospaceMonochromePixelFont* font = 0;
    inline static int render_frames = 0;

    static void _init(int width, int height); // Set logical width and height of canvas
    static void _cleanup();

    int set_active_count = 0;

    Screen(const char* label);
    virtual ~Screen();

    virtual void onSetActive(bool initial); // Receives true on initial activation
    void setActive();

    virtual void update(float dt);

    void clear(bool inverted = false);

    bool inBounds(int x1, int y1, int x2, int y2); // Within logical canvas

    /* Geometry drawing */
    void pset(int x, int y, bool on = true); // Pixel on/off
    void pon(int x, int y); // Pixel on
    void poff(int x, int y); // Pixel off
    void lset(int x1, int y1, int x2, int y2, bool on = true); // Line on/off
    void lon(int x1, int y1, int x2, int y2); // Line on
    void loff(int x1, int y1, int x2, int y2); // Line off
    void rset(int x, int y, int w, int h, bool solid = true, bool on = true); // Rect on/off
    void ron(int x, int y, int w, int h, bool solid = true); // Rect on
    void roff(int x, int y, int w, int h, bool solid = true); // Rect off
    void cset(int x, int y, int size, bool solid = true, bool on = true); // Circle on/off
    void con(int x, int y, int size, bool solid = true); // Circle on
    void coff(int x, int y, int size, bool solid = true); // Circle off

    /* Glyph drawing */
    void locate(int row, int col); // Set position of cursor (top-left is [0,0])
    void print(int index, bool inverted = false); // Print a single glyph at cursor
    void print(const char* text, bool inverted = false); // Print a string of glyphs at cursor
    void repeat(int index, int n, bool inverted = false); // Print a glyph multiple times
    void center(const char* text, int row, bool inverted = false);
    int wrap(const char* text, int max_rows, int max_cols, int& scrolling, bool convert_newline_chars = true, bool test = false);
    void printInt(int n, bool inverted = false);
    void textFill(int row, int col, int rows, int cols, int glyph, bool inverted = false);
    void textBox(int row, int col, int rows, int cols, int border_style = 1, int fill_glyph = ' ', bool inverted = false);
    void scrollbarV(int row, int col, int length, int current_scroll, int max_scroll, int visible_rows);
    void scrollbarH(int row, int col, int length, int current_scroll, int max_scroll, int visible_cols);

    /* 1-bit image drawing */
    void image(Image* image, int row, int col, bool draw_bg = true, int dy = 0);

    void _draw();
private:
    int _wrap(const char* text, int max_rows, int max_cols, int& scrolling, bool convert_newline_chars, bool test);
    virtual void draw() = 0; // For drawing operations only! Doesn't necessarily get called every frame (only as needed)
public:
    virtual bool key_event(int key, int scancode, int action, int mods) = 0; // Returns true if event was handled
    virtual bool char_event(unsigned int c); // Mainly for text input scenarios
};

inline void ponUnsafe(int x, int y)
{
    int i = x + y * Screen::width;
    unsigned char& b = Screen::pixels[i >> 3];
    b |= (unsigned char)(1u << (i & 7));
}

inline void poffUnsafe(int x, int y)
{
    int i = x + y * Screen::width;
    unsigned char& b = Screen::pixels[i >> 3];
    b &= (unsigned char)~(unsigned char)(1u << (i & 7));
}

#endif