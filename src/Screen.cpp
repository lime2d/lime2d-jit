#include "App.h"
#include "Image.h"
#include "misc.h"
#include "MonospaceMonochromePixelFont.h"
#include "Screen.h"
#include "Renderer.h"

void Screen::_init(int width, int height)
{
    if (width % 8) app.fatal("Screen canvas width must be a multiple of 8");

    Screen::width = width;
    Screen::height = height;

    pixels = new unsigned char[width * height / 8] {};

    font = new MonospaceMonochromePixelFont(256, 8, 16);
    text_offset_y = (height % font->glyph_height) / 2;
    rows = height / font->glyph_height;
    cols = width / font->glyph_width;
}

Screen::Screen(const char* label) : label(label) {}

Screen::~Screen()
{
    cout(" Screen \"", false);
    cout(label, false);
    cout("\" [ok]");
}

void Screen::_cleanup()
{
    delete font; font = 0;
    delete[] pixels; pixels = 0;
    cout(" Screen Canvas [ok]");
}

void Screen::onSetActive(bool initial) {}

void Screen::setActive()
{
    (screen = this)->redraw = true;
    onSetActive(++set_active_count == 1);
}

void Screen::update(float dt) {}

void Screen::clear(bool inverted)
{
    int num_bytes = width * height / 8;
    memset(pixels, inverted ? 0xFF : 0, num_bytes);
}

bool Screen::inBounds(int x1, int y1, int x2, int y2)
{
    return x1 >= 0 && x1 < width && y1 >= 0 && y1 < height &&
        x2 >= 0 && x2 < width && y2 >= 0 && y2 < height;
}

void Screen::pset(int x, int y, bool on)
{
    if (x < 0 || x >= width || y < 0 || y >= height)
        APP_FATAL << "Out of bounds. "
        << "Coord: (" << x << "," << y << ") "
        << "Canvas: " << width << "x" << height;

    int i = x + y * width;

    // LSB first (more efficient)
    if (on) pixels[i / 8] |= (1 << (i % 8));
    else pixels[i / 8] &= ~(1 << (i % 8));
}

void Screen::pon(int x, int y)
{
    if (x < 0 || x >= width || y < 0 || y >= height)
        APP_FATAL << "Out of bounds. "
            << "Coord: (" << x << "," << y << ") "
            << "Canvas: " << width << "x" << height;

    int i = x + y * width;
    pixels[i / 8] |= (1 << (i % 8)); // LSB first
}

void Screen::poff(int x, int y)
{
    if (x < 0 || x >= width || y < 0 || y >= height)
        APP_FATAL << "Out of bounds. "
        << "Coord: (" << x << "," << y << ") "
        << "Canvas: " << width << "x" << height;

    int i = x + y * width;
    pixels[i / 8] &= ~(1 << (i % 8)); // LSB first
}

void Screen::lset(int x1, int y1, int x2, int y2, bool on)
{
    if (x1 < 0 || x1 >= width || y1 < 0 || y1 >= height ||
        x2 < 0 || x2 >= width || y2 < 0 || y2 >= height)
        APP_FATAL << "Out of bounds. "
        << "Coord: (" << x1 << "," << y1 << ")-(" << x2 << "," << y2 << ") "
        << "Canvas: " << width << "x" << height;

    int dx = std::abs(x2 - x1);
    int sx = (x1 < x2) ? 1 : -1;
    int dy = -std::abs(y2 - y1);
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx + dy;

    if (on)
    {
        for (;;)
        {
            ponUnsafe(x1, y1);
            if (x1 == x2 && y1 == y2) break;
            int e2 = err << 1;
            if (e2 >= dy) { err += dy; x1 += sx; }
            if (e2 <= dx) { err += dx; y1 += sy; }
        }
    }
    else
    {
        for (;;)
        {
            poffUnsafe(x1, y1);
            if (x1 == x2 && y1 == y2) break;
            int e2 = err << 1;
            if (e2 >= dy) { err += dy; x1 += sx; }
            if (e2 <= dx) { err += dx; y1 += sy; }
        }
    }
}

void Screen::lon(int x1, int y1, int x2, int y2)
{
    if (!inBounds(x1, y1, x2, y2))
        APP_FATAL << "Out of bounds. "
        << "Coord: (" << x1 << "," << y1 << ")-(" << x2 << "," << y2 << ") "
        << "Canvas: " << width << "x" << height;

    int dx{ std::abs(x2 - x1) }, dy{ -std::abs(y2 - y1) };
    int sx{ (x1 < x2) ? 1 : -1 }, sy{ (y1 < y2) ? 1 : -1 };
    int err = dx + dy;

    for (;;)
    {
        ponUnsafe(x1, y1);
        if (x1 == x2 && y1 == y2) break;
        int e2 = err << 1;
        if (e2 >= dy) { err += dy; x1 += sx; }
        if (e2 <= dx) { err += dx; y1 += sy; }
    }
}

void Screen::loff(int x1, int y1, int x2, int y2)
{
    if (!inBounds(x1, y1, x2, y2))
        APP_FATAL << "Out of bounds. "
        << "Coord: (" << x1 << "," << y1 << ")-(" << x2 << "," << y2 << ") "
        << "Canvas: " << width << "x" << height;

    int dx{ std::abs(x2 - x1) }, dy{ -std::abs(y2 - y1) };
    int sx{ (x1 < x2) ? 1 : -1 }, sy{ (y1 < y2) ? 1 : -1 };
    int err = dx + dy;

    for (;;)
    {
        poffUnsafe(x1, y1);
        if (x1 == x2 && y1 == y2) break;
        int e2 = err << 1;
        if (e2 >= dy) { err += dy; x1 += sx; }
        if (e2 <= dx) { err += dx; y1 += sy; }
    }
}

void Screen::rset(int x, int y, int w, int h, bool solid, bool on)
{
    if (on) ron(x, y, w, h, solid);
    else roff(x, y, w, h, solid);
}

void Screen::ron(int x, int y, int w, int h, bool solid)
{
    if (w <= 0 || h <= 0) return;

    if (!inBounds(x, y, x + w - 1, y + h - 1))
        APP_FATAL << "Out of bounds. "
        << "Coord: (" << x << "," << y << ")-(" << (x + w - 1) << "," << (y + h - 1) << ") "
        << "Canvas: " << width << "x" << height;

    if (solid)
    {
        for (int i = x; i < x + w; ++i)
            for (int j = y; j < y + h; ++j)
                ponUnsafe(i, j);
    }
    else
    {
        for (int i = x; i < x + w; ++i)
        {
            ponUnsafe(i, y);
            ponUnsafe(i, y + h - 1);
        }

        for (int j = y + 1; j < y + h - 1; ++j)
        {
            ponUnsafe(x, j);
            ponUnsafe(x + w - 1, j);
        }            
    }
}

void Screen::roff(int x, int y, int w, int h, bool solid)
{
    if (w <= 0 || h <= 0) return;

    if (!inBounds(x, y, x + w - 1, y + h - 1))
        APP_FATAL << "Out of bounds. "
        << "Coord: (" << x << "," << y << ")-(" << (x + w - 1) << "," << (y + h - 1) << ") "
        << "Canvas: " << width << "x" << height;

    if (solid)
    {
        for (int i = x; i < x + w; ++i)
            for (int j = y; j < y + h; ++j)
                poffUnsafe(i, j);
    }
    else
    {
        for (int i = x; i < x + w; ++i)
        {
            poffUnsafe(i, y);
            poffUnsafe(i, y + h - 1);
        }

        for (int j = y + 1; j < y + h - 1; ++j)
        {
            poffUnsafe(x, j);
            poffUnsafe(x + w - 1, j);
        }            
    }
}

void Screen::cset(int x, int y, int size, bool solid, bool on)
{
    if (on) con(x, y, size, solid);
    else coff(x, y, size, solid);
}

// Draw circle (x, y, and size define the logical square in which the circle sits)
void Screen::con(int x, int y, int size, bool solid)
{
    if (size <= 0) return;

    // The (x,y) params specify the top-left of the circle's bounding box
    if (!inBounds(x, y, x + size - 1, y + size - 1))
        APP_FATAL << "Out of bounds. "
        << "Coord: (" << x << "," << y << ")-(" << (x + size - 1) << "," << (y + size - 1) << ") "
        << "Canvas: " << width << "x" << height;

    // This algorithm has been heavily optimized
    float r = size / 2.0f;
    float rsq = r * r;

    // Use quadrant-mirroring technique
    int size_plus_1_by_2 = (size + 1) / 2;
    int size_minus_1 = size - 1;
    float point_5_minus_r = 0.5f - r;
    int start_x = 0;
    for (int py = size_plus_1_by_2 - 1; py >= 0 ; py--) {
        float dy = py + point_5_minus_r;
        float dy_sq = dy * dy;
        float rsq_minus_dy_sq = rsq - dy_sq;
        int y_plus_py = y + py;
        int y_minus_py_plus_size_minus_1 = y - py + size_minus_1;
        for (int px = start_x; px < size_plus_1_by_2; px++) {
            float dx = px + point_5_minus_r;
            if (dx * dx <= rsq_minus_dy_sq)
            {
                start_x = px;

                ponUnsafe(x + px, y_plus_py);
                ponUnsafe(x + px, y_minus_py_plus_size_minus_1);
                ponUnsafe(x - px + size_minus_1, y_plus_py);
                ponUnsafe(x - px + size_minus_1, y_minus_py_plus_size_minus_1);

                if (solid)
                {
                    while (++px < size_plus_1_by_2)
                    {
                        ponUnsafe(x + px, y_plus_py);
                        ponUnsafe(x + px, y_minus_py_plus_size_minus_1);
                        ponUnsafe(x - px + size_minus_1, y_plus_py);
                        ponUnsafe(x - px + size_minus_1, y_minus_py_plus_size_minus_1);
                    }
                    break;
                }
                else if (py)
                {
                    int test_py = py - 1;
                    int test_px = px + 1;

                    float test_dy = test_py + point_5_minus_r;
                    float test_dy_sq = test_dy * test_dy;
                    float rsq_minus_test_dy_sq = rsq - test_dy_sq;

                    float test_dx = test_px + point_5_minus_r;

                    if (test_dx * test_dx <= rsq_minus_test_dy_sq)
                        break;
                }
            }
        }
    }
}

// Clear circle (x, y, and size define the logical square in which the circle sits)
void Screen::coff(int x, int y, int size, bool solid)
{
    if (size <= 0) return;

    // The (x,y) params specify the top-left of the circle's bounding box
    if (!inBounds(x, y, x + size - 1, y + size - 1))
        APP_FATAL << "Out of bounds. "
        << "Coord: (" << x << "," << y << ")-(" << (x + size - 1) << "," << (y + size - 1) << ") "
        << "Canvas: " << width << "x" << height;

    // This algorithm has been heavily optimized
    float r = size / 2.0f;
    float rsq = r * r;

    // Use quadrant-mirroring technique
    int size_plus_1_by_2 = (size + 1) / 2;
    int size_minus_1 = size - 1;
    float point_5_minus_r = 0.5f - r;
    int start_x = 0;
    for (int py = size_plus_1_by_2 - 1; py >= 0; py--) {
        float dy = py + point_5_minus_r;
        float dy_sq = dy * dy;
        float rsq_minus_dy_sq = rsq - dy_sq;
        int y_plus_py = y + py;
        int y_minus_py_plus_size_minus_1 = y - py + size_minus_1;
        for (int px = start_x; px < size_plus_1_by_2; px++) {
            float dx = px + point_5_minus_r;
            if (dx * dx <= rsq_minus_dy_sq)
            {
                start_x = px;

                poffUnsafe(x + px, y_plus_py);
                poffUnsafe(x + px, y_minus_py_plus_size_minus_1);
                poffUnsafe(x - px + size_minus_1, y_plus_py);
                poffUnsafe(x - px + size_minus_1, y_minus_py_plus_size_minus_1);

                if (solid)
                {
                    while (++px < size_plus_1_by_2)
                    {
                        poffUnsafe(x + px, y_plus_py);
                        poffUnsafe(x + px, y_minus_py_plus_size_minus_1);
                        poffUnsafe(x - px + size_minus_1, y_plus_py);
                        poffUnsafe(x - px + size_minus_1, y_minus_py_plus_size_minus_1);
                    }
                    break;
                }
                else if (py)
                {
                    int test_py = py - 1;
                    int test_px = px + 1;

                    float test_dy = test_py + point_5_minus_r;
                    float test_dy_sq = test_dy * test_dy;
                    float rsq_minus_test_dy_sq = rsq - test_dy_sq;

                    float test_dx = test_px + point_5_minus_r;

                    if (test_dx * test_dx <= rsq_minus_test_dy_sq)
                        break;
                }
            }
        }
    }
}

void Screen::locate(int row, int col)
{
    MonospaceMonochromePixelFont& font = *Screen::font;
    int cols_per_row = width / font.glyph_width;

    if (col < 0 || col >= cols_per_row)
    {
        int rows = col / cols_per_row - (col < 0 ? 1 : 0);
        row += rows;
        col -= rows * cols_per_row;
    }

    int rows = height / font.glyph_height;
    row = (row % rows + rows) % rows;

    cursor.row = row; cursor.col = col;
}

void Screen::print(int index, bool inverted)
{
    MonospaceMonochromePixelFont& font = *Screen::font;
    if (index < 0 || index >= font.num_glyphs) app.fatal("Glyph index out of range.");

    int glyph_height = font.glyph_height;
    int cols_per_row = width / font.glyph_width;
    int byte_index = cursor.col + cursor.row * cols_per_row * glyph_height;
    byte_index += text_offset_y * cols_per_row;

    Glyph glyph = font.glyphs[index];
    unsigned char* row = glyph.row;
    unsigned char* pixels = Screen::pixels + byte_index;

    if (inverted)
        for (int r = 0; r < glyph_height; r++)
            pixels[r * cols_per_row] = ~row[r];
    else
        for (int r = 0; r < glyph_height; r++)
            pixels[r * cols_per_row] = row[r];

    if (++cursor.col == cols_per_row)
    {
        int rows = height / glyph_height;
        if (++cursor.row >= rows) cursor.row -= rows;
        cursor.col -= cols_per_row;
    }
}

void Screen::print(const char* text, bool inverted)
{
    while (*text) print((unsigned char)*text++, inverted);
}

void Screen::repeat(int index, int n, bool inverted)
{
    while (n-- > 0) print(index, inverted);
}

void Screen::center(const char* text, int row, bool inverted)
{
    const char* p = text;
    while (*p) p++;
    locate(row, (cols - static_cast<int>(p - text)) / 2);
    print(text, inverted);
}

int Screen::wrap(const char* text, int max_rows, int max_cols, int& scrolling, bool convert_newline_chars, bool test)
{
    // First we need to determine how much scrolling is even possible
    int s = 0;
    Cursor cursor = this->cursor;
    int lines = _wrap(text, 1024, max_cols, s, convert_newline_chars, true);
    this->cursor = cursor;
    // We can now calculate the maximum scrolling
    int max_scrolling = max(lines - max_rows, 0);
    int clamped_scrolling = scrolling = clamp(scrolling, 0, max_scrolling);
    if (test) return lines; // If testing then just return the number of lines and clamped scrolling side-effect
    // Now perform the actual drawing
    return _wrap(text, max_rows, max_cols, clamped_scrolling, convert_newline_chars, false);
}

int Screen::_wrap(const char* text, int max_rows, int max_cols, int& scrolling, bool convert_newline_chars, bool test)
{
    if (!text[0]) return 0;

    int start_row = cursor.row;
    int start_col = cursor.col;

    if (convert_newline_chars)
    {
        const char* p = text - 1;
        int num_newlines = 0;
        while (*++p) if (*p == '\n') num_newlines++;

        if (num_newlines)
        {
            int len = static_cast<int>(p - text);
            p = text - 1;
            char* buffer = new char[len + 1];
            char* section = buffer;
            char* s = buffer - 1;
            int current_row = start_row;
            int lines_drawn = 0;

            while (true)
            {
                if ((*++s = *++p) == '\n' || !*p)
                {
                    if (*p == '\n' && (p == text || (p > text && p[-1] == '\n')))
                    {
                        if (scrolling) { scrolling--; }
                        else { current_row++; lines_drawn++; }
                    }
                    else if (s > section)
                    {
                        *s = 0;
                        locate(current_row, start_col);
                        int lines = _wrap(section, max_rows - (current_row - start_row), max_cols, scrolling, false, test);
                        current_row += lines;
                        lines_drawn += lines;
                    }

                    if (!*p)
                    {
                        delete[] buffer;
                        return lines_drawn;
                    }

                    section = s + 1;
                }
            }
        }
    }

    int offset = 0;
    int len = 0;
    while (text[++len]);
    int offset_max = len - 1;

    for (int r = 0; r < max_rows; r++)
    {
        locate(start_row + r, start_col);

        if (len - offset <= max_cols) // Remainder fits on current row
        {
            if (test || scrolling)
            {
                if (scrolling) { scrolling--; r--; }
            }
            else
            {
                print(text + offset);
            }
            return r + 1;
        }

        if (text[offset + max_cols] == ' ')
        {
            if (test || scrolling)
            {
                offset += max_cols + 1;
                if (scrolling) { scrolling--; r--; }
            }
            else
            {
                for (int c = 0; c < max_cols; c++) print(text[offset++]);
                offset++;
            }
        }
        else if (text[offset + max_cols - 1] == '-') // Can print maximum characters on current row
        {
            if (test || scrolling)
            {
                offset += max_cols;
                if (scrolling) { scrolling--; r--; }
            }
            else
            {
                for (int c = 0; c < max_cols; c++) print(text[offset++]);
            }
        }
        else // Attempt to find an earlier break point
        {
            int i = max_cols;
            while (--i > 0)
            {
                if (text[offset + i] == ' ')
                {
                    if (test || scrolling)
                    {
                        offset += i + 1;
                        if (scrolling) { scrolling--; r--; }
                    }
                    else
                    {
                        for (int c = 0; c < i; c++) print(text[offset++]);
                        offset++;
                    }
                    break;
                }
                else if (text[offset + i] == '-')
                {
                    if (test || scrolling)
                    {
                        offset += i + 1;
                        if (scrolling) { scrolling--; r--; }
                    }
                    else
                    {
                        for (int c = 0; c <= i; c++) print(text[offset++]);
                    }
                    break;
                }
            }
        }
    }

    return max_rows;
}

void Screen::printInt(int n, bool inverted)
{
    char buf[32];

    for (int i = 0; i < 16; i++)
    {
        buf[i] = '0' + (n % 10);
        n /= 10;
        if (!n)
        {
            while (i >= 0) print(buf[i--], inverted);
            return;
        }
    }

    print("(Out of range)", inverted);
}

void Screen::textFill(int row, int col, int rows, int cols, int glyph, bool inverted)
{
    if (rows < 1 || cols < 1)
        APP_FATAL << "invalid size [" << rows << "x" << cols << "]";

    int erow = row + rows - 1;
    int ecol = col + cols - 1;

    if (row < 0 || col < 0 || erow >= Screen::rows || ecol >= Screen::cols)
        APP_FATAL << "out of bounds [" << row << "," << col << "]-[" << erow << "," << ecol << "]";

    for (int r = row; r <= erow; r++)
    {
        locate(r, col);
        int l = cols + 1;
        while (--l) print(glyph, inverted);
    }
}

void Screen::textBox(int row, int col, int rows, int cols, int border_style, int fill_glyph, bool inverted)
{
    if (rows < 2 || cols < 2)
        APP_FATAL << "invalid size [" << rows << "x" << cols << "]";

    if (border_style < 0 || border_style > 3)
        APP_FATAL << "invalid border style (" << border_style << ")";

    int erow = row + rows - 1;
    int ecol = col + cols - 1;

    if (row < 0 || col < 0 || erow >= Screen::rows || ecol >= Screen::cols)
        APP_FATAL << "out of bounds [" << row << "," << col << "]-[" << erow << "," << ecol << "]";

    int topleft;
    int topright;
    int bottomleft;
    int bottomright;
    int horizontal;
    int vertical;

    if (border_style == 1)
    {
        topleft = 218;
        topright = 191;
        bottomleft = 192;
        bottomright = 217;
        horizontal = 196;
        vertical = 179;
    }
    else if (border_style == 2)
    {
        topleft = 201;
        topright = 187;
        bottomleft = 200;
        bottomright = 188;
        horizontal = 205;
        vertical = 186;
    }
    else
    {
        topleft = topright = bottomleft = bottomright = horizontal = vertical = fill_glyph;
    }

    locate(row, col);
    print(topleft, inverted);
    locate(row, ecol);
    print(topright, inverted);
    locate(erow, col);
    print(bottomleft, inverted);
    locate(erow, ecol);
    print(bottomright, inverted);

    locate(row, col + 1);
    int c = col;
    while (++c < ecol) print(horizontal, inverted);

    locate(erow, col + 1);
    c = col;
    while (++c < ecol) print(horizontal, inverted);

    int r = row;
    while (++r < erow)
    {
        locate(r, col);
        print(vertical, inverted);
        locate(r, ecol);
        print(vertical, inverted);
    }

    if (fill_glyph && rows > 2 && cols > 2) textFill(row + 1, col + 1, rows - 2, cols - 2, fill_glyph, inverted);

    if (border_style == 3)
        rset(col * 8, row * 16 + text_offset_y, cols * 8, rows * 16, false, !inverted);
}

void Screen::scrollbarV(int row, int col, int length, int current_scroll, int max_scroll, int visible_rows)
{
    if (length <= 0 || max_scroll <= 0 || visible_rows <= 0)
        APP_FATAL << "invalid params (length=" << length << ", max_scroll=" << max_scroll << ", visible_rows=" << visible_rows << ")";

    if (row < 0 || row + length - 1 >= rows || col < 0 || col >= cols)
        APP_FATAL << "out of bounds [" << row << "," << col << "]-[" << row + length - 1 << "," << col << "]";

    // Draw track
    for (int r = 0; r < length; r++)
    {
        locate(row + r, col);
        print(176);
    }

    // Thumb size proportional to the visible/total ratio
    int total_lines = visible_rows + max_scroll;
    int thumb_height = max((visible_rows * length) / total_lines, 1);

    // Thumb position mapped to available travel distance
    int travel = length - thumb_height;
    int thumb_offset = (travel > 0)
        ? (int)((float)current_scroll / (float)max_scroll * (float)travel + 0.5f)
        : 0;

    // Draw thumb
    for (int i = 0; i < thumb_height; i++)
    {
        locate(row + thumb_offset + i, col);
        print(219);
    }
}

void Screen::scrollbarH(int row, int col, int length, int current_scroll, int max_scroll, int visible_cols)
{
    if (length <= 0 || max_scroll <= 0 || visible_cols <= 0)
        APP_FATAL << "invalid params (length=" << length << ", max_scroll=" << max_scroll << ", visible_cols=" << visible_cols << ")";

    if (row < 0 || row >= rows || col < 0 || col + length - 1 >= cols)
        APP_FATAL << "out of bounds [" << row << "," << col << "]-[" << row << "," << col + length - 1 << "]";

    // Draw track
    locate(row, col);
    repeat(176, length);

    // Thumb size proportional to the visible/total ratio
    int total_cols = visible_cols + max_scroll;
    int thumb_width = max((visible_cols * length) / total_cols, 1);

    // Thumb position mapped to available travel distance
    int travel = length - thumb_width;
    int thumb_offset = (travel > 0)
        ? (int)((float)current_scroll / (float)max_scroll * (float)travel + 0.5f)
        : 0;

    // Draw thumb
    locate(row, col + thumb_offset);
    repeat(219, thumb_width);
}

void Screen::image(Image* image, int row, int col, bool draw_bg, int dy)
{
    MonospaceMonochromePixelFont& font = *Screen::font;
    int x = col * font.glyph_width;
    int y = row * font.glyph_height + dy;

    if (x % 8) app.fatal("Image x not a multiple of 8");

    const int image_width = image->width;
    const int image_height = image->height;

    if (x < 0 || y < 0 || x + image_width > width || y + image_height > height) app.fatal("Image out of bounds");

    int byte_index = (x + y * width) / 8;
    byte_index += text_offset_y * width / font.glyph_width;

    const unsigned char* image_pixels = image->pixels;

    int canvas_width_by_8 = width / 8;
    int image_width_by_8 = image_width / 8;

    if (draw_bg)
    {
        for (int r = 0; r < image_height; r++)
        {
            unsigned char* cpp = pixels + byte_index + r * canvas_width_by_8;
            const unsigned char* ipp = image_pixels + r * image_width_by_8;
            for (int c = 0; c < image_width_by_8; c++) cpp[c] = ipp[c];
        }
    }
    else
    {
        for (int r = 0; r < image_height; r++)
        {
            unsigned char* cpp = pixels + byte_index + r * canvas_width_by_8;
            const unsigned char* ipp = image_pixels + r * image_width_by_8;
            for (int c = 0; c < image_width_by_8; c++) cpp[c] |= ipp[c];
        }
    }
}

void Screen::_draw()
{
    redraw = false; // Clear redraw flag
    app.metrics.draws++;
    cursor.row = cursor.col = 0;
    draw();
    renderer.uploadSSBO();
    render_frames = 3;
}

bool Screen::char_event(unsigned int c) { return false; }