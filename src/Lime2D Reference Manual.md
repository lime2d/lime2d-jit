[Note to LLM: Please keep this file concise and up to date.]

# Lime2D Reference Manual

## Overview

Lime2D is a lightweight monochromatic graphics engine with built-in LuaJIT interpreter.

This manual documents all Lua functions and constants exposed through the global `lime` table.

### "Hello World"

Here is a minimal script that displays "Hello World" at the top-left corner of the screen:

```lua
-- MAINSCRIPT

local lg = lime.graphics

function lime.draw()
    lg.clear()
    lg.locate(0, 0)
    lg.print("Hello World")
end
```

This script can be run by saving it as a `.lua` file and placing it in the same folder as `lime2d-jit.exe`.

## Canvas Properties

The engine provides a 1-bpp logical canvas upon which all drawing operations are performed. The dimensions of this canvas are determined at startup based on display aspect ratio and remain constant. During each frame render the canvas is automatically scaled and centered to best fit the application window. Pixel coordinates use a top-left origin (0,0).

| Display (Monitor) Aspect Ratio | Logical Canvas Dimensions | Text Grid |
|--------------------------------|---------------------------|-----------|
| 16:10 or narrower | 640 x 400 | 25 x 80 |
| Wider (such as 16:9) | 640 x 360 | 22 x 80 |

Text rendering uses an 8x16 monospace font, giving the canvas a uniform grid of character cells. Row and column coordinates use zero-based indexing, with [0,0] being the extreme top-left character (glyph) location.

## Application Callbacks

Your main script defines callback functions directly on the `lime` table:

```lua
function lime.init()
    -- Called once when the app starts
end

function lime.update(dt)
    -- Called every frame
    -- dt: time in seconds since the last frame
end

function lime.draw()
    -- Called when the screen needs redrawing
    -- Perform all rendering operations here
end

function lime.keypressed(key, scancode, isrepeat)
    -- Called when a key is pressed or held (repeat)
    -- key: the key code (use lime.keyboard constants)
    -- scancode: platform-specific scan code
    -- isrepeat: true if this is a key-repeat event, false on initial press
end

function lime.keyreleased(key, scancode)
    -- Called when a key is released
    -- key: the key code (use lime.keyboard constants)
    -- scancode: platform-specific scan code
end

function lime.textinput(text)
    -- Called for text input (UTF-8 character string)
    -- text: the character as a UTF-8 string
end

function lime.quit()
    -- Called when the application is about to close, whether from:
    --   * The user clicking the window's close button
    --   * A call to lime.window.quit()
    -- Return true to abort the close; return false or nothing to allow it
end
```

All callbacks are optional. Only define the ones you need.

---

## lime.graphics

Graphics functions for drawing primitives, text, and images on the monochrome canvas.

### Constants

| Constant | Type | Description |
|----------|------|-------------|
| `WIDTH` | integer | Canvas width in pixels |
| `HEIGHT` | integer | Canvas height in pixels |
| `TEXT_OFFSET_Y` | integer | Vertical offset of text grid ((HEIGHT % 16) / 2) |
| `ROWS` | integer | Number of text rows (HEIGHT / 16) |
| `COLS` | integer | Number of text columns (WIDTH / 8) |

### Screen Management

#### `lime.graphics.redraw()`

Signals that the screen should be redrawn. Call this after modifying game state in `update()` to trigger a `draw()` call.

**Note:** Keyboard keypress events (including repeats) automatically raise Lime2D's internal redraw flag.

**Tip:** If you want drawing to occur for every frame simply call `redraw()` at the top of `update()`.

#### `lime.graphics.clear([inverted])`

Clears the entire canvas.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `inverted` | boolean | `false` | If `true`, fills with foreground color; if `false`, fills with background color |

#### `lime.graphics.setFgColor(r, g, b)`

Sets the foreground (on-pixel) color.

| Parameter | Type | Range | Description |
|-----------|------|-------|-------------|
| `r` | number | 0.0–1.0 | Red component |
| `g` | number | 0.0–1.0 | Green component |
| `b` | number | 0.0–1.0 | Blue component |

#### `lime.graphics.setBgColor(r, g, b)`

Sets the background (off-pixel) color.

| Parameter | Type | Range | Description |
|-----------|------|-------|-------------|
| `r` | number | 0.0–1.0 | Red component |
| `g` | number | 0.0–1.0 | Green component |
| `b` | number | 0.0–1.0 | Blue component |

### Pixel Operations

#### `lime.graphics.pset(x, y [, on])`

Sets a single pixel.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `x` | integer | — | X coordinate |
| `y` | integer | — | Y coordinate |
| `on` | boolean | `true` | Pixel state |

#### `lime.graphics.pon(x, y)`

Turns a single pixel on (foreground color).

#### `lime.graphics.poff(x, y)`

Turns a single pixel off (background color).

#### `lime.graphics.pons(coords)`

Turns multiple pixels on. Accepts a flat table of coordinate pairs.

| Parameter | Type | Description |
|-----------|------|-------------|
| `coords` | table | `{x1, y1, x2, y2, ...}` — length must be even |

#### `lime.graphics.poffs(coords)`

Turns multiple pixels off. Accepts a flat table of coordinate pairs.

| Parameter | Type | Description |
|-----------|------|-------------|
| `coords` | table | `{x1, y1, x2, y2, ...}` — length must be even |

### Line Operations

All line functions use Bresenham's algorithm.

#### `lime.graphics.lset(x1, y1, x2, y2 [, on])`

Draws a line between two points.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `x1, y1` | integer | — | Start point |
| `x2, y2` | integer | — | End point |
| `on` | boolean | `true` | Pixel state |

#### `lime.graphics.lon(x1, y1, x2, y2)`

Draws a line with pixels on.

#### `lime.graphics.loff(x1, y1, x2, y2)`

Draws a line with pixels off.

#### `lime.graphics.lsets(coords [, on])`

Draws multiple independent line segments.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `coords` | table | — | `{x1, y1, x2, y2, x3, y3, x4, y4, ...}` — length must be a multiple of 4 |
| `on` | boolean | `true` | Pixel state |

#### `lime.graphics.lsetsc(coords [, on])`

Draws a continuous polyline (each point connects to the next).

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `coords` | table | — | `{x1, y1, x2, y2, x3, y3, ...}` — length must be even, minimum 4 |
| `on` | boolean | `true` | Pixel state |

### Rectangle Operations

#### `lime.graphics.rset(x, y, w, h [, solid [, on]])`

Draws a rectangle.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `x, y` | integer | — | Top-left corner |
| `w, h` | integer | — | Width and height |
| `solid` | boolean | `true` | If `true`, filled; if `false`, edges only |
| `on` | boolean | `true` | Pixel state |

#### `lime.graphics.ron(x, y, w, h [, solid])`

Draws a rectangle with pixels on.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `solid` | boolean | `true` | If `true`, filled; if `false`, edges only |

#### `lime.graphics.roff(x, y, w, h [, solid])`

Draws a rectangle with pixels off.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `solid` | boolean | `true` | If `true`, filled; if `false`, edges only |

### Circle Operations

Circle coordinates specify the top-left corner of the bounding box, not the center.

#### `lime.graphics.cset(x, y, size [, solid [, on]])`

Draws a circle.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `x, y` | integer | — | Top-left of bounding box |
| `size` | integer | — | Diameter (bounding box is size × size) |
| `solid` | boolean | `true` | If `true`, filled; if `false`, edges only |
| `on` | boolean | `true` | Pixel state |

#### `lime.graphics.con(x, y, size [, solid])`

Draws a circle with pixels on.

#### `lime.graphics.coff(x, y, size [, solid])`

Draws a circle with pixels off.

### Text Operations

Text uses an 8×16 IBM VGA-style monospace font with 256 glyphs (code page 437 layout). Text-drawing functions are **opaque**. When a glyph is drawn, it completely overwrites all pixels within its 8×16 bounding box.

#### `lime.graphics.locate(row, col)`

Sets the text cursor position for subsequent print operations.

| Parameter | Type | Description |
|-----------|------|-------------|
| `row` | integer | Row index (0 = top) |
| `col` | integer | Column index (0 = left) |

#### `lime.graphics.print(text_or_glyph [, inverted])`

Prints text or a single glyph at the cursor position. The cursor advances automatically.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `text_or_glyph` | string or integer | — | String to print, or glyph index (0–255) |
| `inverted` | boolean | `false` | If `true`, swaps foreground and background |

#### `lime.graphics.repeat(glyph, n [, inverted])`

Prints a glyph multiple times at the cursor position. The cursor advances automatically.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `glyph` | integer | — | Glyph index (0–255) |
| `n` | integer | — | Number of repetitions |
| `inverted` | boolean | `false` | If `true`, swaps foreground and background |

#### `lime.graphics.center(text, row [, inverted])`

Prints text horizontally centered on the specified row. The cursor advances automatically.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `text` | string | — | Text to print |
| `row` | integer | — | Row index |
| `inverted` | boolean | `false` | If `true`, swaps foreground and background |

#### `lime.graphics.wrap(text, max_rows, max_cols [, scroll_amount [, convert_newline_chars [, test]]])`

Prints wrapped text with scrolling support. Breaks lines at spaces and hyphens. The cursor advances automatically.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `text` | string | — | Text to print |
| `max_rows` | integer | — | Maximum number of rows to display |
| `max_cols` | integer | — | Maximum columns per line |
| `scroll_amount` | integer | `0` | Number of lines to skip (for scrolling) |
| `convert_newline_chars` | boolean | `true` | If `true`, interprets newlines as line breaks |
| `test` | boolean | `false` | If `true`, calculates line count and clamps scroll without drawing |

**Returns:** `lines, scroll_amount` — total line count and (possibly clamped) scroll amount.

#### `lime.graphics.printInt(n [, inverted])`

Prints an integer at the cursor position. The cursor advances automatically.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `n` | integer | — | Number to print |
| `inverted` | boolean | `false` | If `true`, swaps foreground and background |

#### `lime.graphics.textFill(row, col, rows, cols, glyph [, inverted])`

Fills a rectangular region of text cells with the specified glyph.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `row` | integer | — | Top row of region |
| `col` | integer | — | Left column of region |
| `rows` | integer | — | Number of rows (minimum 1) |
| `cols` | integer | — | Number of columns (minimum 1) |
| `glyph` | integer | — | Glyph index (0–255) |
| `inverted` | boolean | `false` | If `true`, swaps foreground and background |

#### `lime.graphics.textBox(row, col, rows, cols [, border_style [, fill_glyph [, inverted]]])`

Draws a text box with the specified border style.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `row` | integer | — | Top row of border |
| `col` | integer | — | Left column of border |
| `rows` | integer | — | Total height in rows (minimum 2) |
| `cols` | integer | — | Total width in columns (minimum 2) |
| `border_style` | integer | `1` | `0` for none, `1` or `2` for CP437 box-drawing characters (single-line or double-line), `3` for single-pixel perimeter (edge) |
| `fill_glyph` | integer | `32` | Index of glyph used to fill interior region (or `0` for no-fill) |
| `inverted` | boolean | `false` | If `true`, swaps foreground and background |

#### `lime.graphics.textScrollbarV(row, col, length, current_scroll, max_scroll, visible_rows)`

Draws a vertical scrollbar using CP437 characters (176 for track, 219 for thumb).

| Parameter | Type | Description |
|-----------|------|-------------|
| `row` | integer | Top row of scrollbar |
| `col` | integer | Column position |
| `length` | integer | Height in rows (minimum 1) |
| `current_scroll` | integer | Current scroll position (0 = top) |
| `max_scroll` | integer | Maximum scroll value (minimum 1) |
| `visible_rows` | integer | Number of visible content rows (minimum 1) |

#### `lime.graphics.textScrollbarH(row, col, length, current_scroll, max_scroll, visible_cols)`

Draws a horizontal scrollbar using CP437 characters (176 for track, 219 for thumb).

| Parameter | Type | Description |
|-----------|------|-------------|
| `row` | integer | Row position |
| `col` | integer | Left column of scrollbar |
| `length` | integer | Width in columns (minimum 1) |
| `current_scroll` | integer | Current scroll position (0 = leftmost) |
| `max_scroll` | integer | Maximum scroll value (minimum 1) |
| `visible_cols` | integer | Number of visible content columns (minimum 1) |

### Image Operations

Images are 1-bit-per-pixel bitmaps where each byte represents 8 horizontal pixels (LSB = leftmost pixel).

#### `lime.graphics.defineImage(name, width, height, bytes)`

Registers an image for later drawing.

| Parameter | Type | Description |
|-----------|------|-------------|
| `name` | string | Unique identifier for the image |
| `width` | integer | Width in pixels (must be a multiple of 8) |
| `height` | integer | Height in pixels |
| `bytes` | table or string | Packed pixel data; table of integers (0–255) or binary string |

The number of bytes must equal `width * height / 8`.

#### `lime.graphics.image(name, row, col [, draw_bg [, dy]])`

Draws a previously defined image.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `name` | string | — | Image identifier from `defineImage` |
| `row` | integer | — | Row position (character cell row) |
| `col` | integer | — | Column position (character cell column) |
| `draw_bg` | boolean | `true` | If `true`, draws both on and off pixels; if `false`, only draws on pixels (transparent background) |
| `dy` | integer | `0` | Vertical pixel offset within the cell |

---

## lime.window

Window management functions.

### Constants

| Constant | Type | Description |
|----------|------|-------------|
| `WIDTH` | integer | Current window width in pixels (dynamic, changes on resize) |
| `HEIGHT` | integer | Current window height in pixels (dynamic, changes on resize) |

### Functions

#### `lime.window.toggleFullscreen()`

Toggles between windowed and fullscreen mode.

**Returns:** `boolean` — `true` if now fullscreen, `false` if now windowed.

#### `lime.window.getFullscreen()`

**Returns:** `boolean` — `true` if currently fullscreen.

#### `lime.window.setFullscreen(fullscreen)`

Sets fullscreen or windowed mode.

| Parameter | Type | Description |
|-----------|------|-------------|
| `fullscreen` | boolean | `true` for fullscreen, `false` for windowed |

#### `lime.window.setTitle(title)`

Sets the window title.

| Parameter | Type | Description |
|-----------|------|-------------|
| `title` | string | New window title |

#### `lime.window.quit([exit_code])`

Requests application exit. If a `lime.quit` callback is defined, it is invoked first. If `lime.quit` returns `true`, the quit is aborted and this function returns without exiting.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `exit_code` | integer | `0` | Process exit code |

---

## lime.keyboard

Keyboard input handling.

### Functions

#### `lime.keyboard.isDown(key)`

Checks if a key is currently pressed.

| Parameter | Type | Description |
|-----------|------|-------------|
| `key` | integer | Key code (use KEY_* constants) |

**Returns:** `boolean` — `true` if the key is held down.

#### `lime.keyboard.ctrlIsDown()`

Checks if either Control key is currently pressed.

**Returns:** `boolean`

#### `lime.keyboard.altIsDown()`

Checks if either Alt key is currently pressed.

**Returns:** `boolean`

#### `lime.keyboard.shiftIsDown()`

Checks if either Shift key is currently pressed.

**Returns:** `boolean`

### Key Constants

**Letter keys:** `KEY_A` through `KEY_Z`

**Digit keys:** `KEY_0` through `KEY_9`

**Function keys:** `KEY_F1` through `KEY_F12`

**Special keys:**

| Constant | Description |
|----------|-------------|
| `KEY_ESCAPE` | Escape |
| `KEY_ENTER` | Enter/Return |
| `KEY_TAB` | Tab |
| `KEY_SPACE` | Spacebar |
| `KEY_BACKSPACE` | Backspace |
| `KEY_DELETE` | Delete |
| `KEY_UP` | Up arrow |
| `KEY_DOWN` | Down arrow |
| `KEY_LEFT` | Left arrow |
| `KEY_RIGHT` | Right arrow |
| `KEY_HOME` | Home |
| `KEY_END` | End |
| `KEY_PAGE_UP` | Page Up |
| `KEY_PAGE_DOWN` | Page Down |
| `KEY_LEFT_SHIFT` | Left Shift |
| `KEY_RIGHT_SHIFT` | Right Shift |
| `KEY_LEFT_CONTROL` | Left Control |
| `KEY_RIGHT_CONTROL` | Right Control |
| `KEY_LEFT_ALT` | Left Alt |
| `KEY_RIGHT_ALT` | Right Alt |

### Example Usage

```lua
-- MAINSCRIPT

-- Shorthand references
local lg = lime.graphics
local lk = lime.keyboard

-- Start player at center of text grid
local player = {
    row = math.floor(lg.ROWS / 2),
    col = math.floor(lg.COLS / 2)
}

function lime.keypressed(key, scancode, isrepeat)
    if key == lk.KEY_UP then
        player.row = math.max(player.row - 1, 0)
    elseif key == lk.KEY_DOWN then
        player.row = math.min(player.row + 1, lg.ROWS - 1)
    elseif key == lk.KEY_LEFT then
        player.col = math.max(player.col - 1, 0)
    elseif key == lk.KEY_RIGHT then
        player.col = math.min(player.col + 1, lg.COLS - 1)
    elseif key == lk.KEY_ESCAPE then
        lime.window.quit()
    end
end

function lime.draw()
    lg.clear()
    lg.locate(player.row, player.col)
    lg.print(1) -- Smiley face avatar
end
```

---

## lime.time

Time-related functions.

#### `lime.time.sinceStart()`

**Returns:** `number` — seconds elapsed since the application started (high precision).

#### `lime.time.sinceEpoch()`

**Returns:** `integer` — seconds since the Unix epoch (January 1, 1970). Useful for seeding random number generators.

---

## lime.filesystem

Sandboxed filesystem access for persistent data storage. All paths are relative to an application-specific save directory.

On Windows, the base path is `%APPDATA%\Lime2D\<identity>\`.

### Functions

#### `lime.filesystem.setIdentity(name)`

Sets the application identity, which determines the save directory name. This function can only be called **once**, and must be called **before** any other `lime.filesystem` operations that access the sandbox (such as `read`, `write`, `exists`, etc.). Call it early, e.g., in `lime.init()`.

| Parameter | Type | Description |
|-----------|------|-------------|
| `name` | string | Application identifier (alphanumeric, underscores, hyphens; max 64 characters after sanitization) |

Raises an error if called more than once or after filesystem operations have already been performed.

#### `lime.filesystem.getSaveDir()`

**Returns:** `string` — the full path to the save directory.

#### `lime.filesystem.read(path)`

Reads the contents of a file.

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | string | Relative path within the save directory |

**Returns:** `string` on success, or `nil, error_message` on failure.

#### `lime.filesystem.write(path, data)`

Writes data to a file, creating parent directories as needed. Overwrites existing files.

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | string | Relative path within the save directory |
| `data` | string | Content to write |

**Returns:** `true` on success, or `false, error_message` on failure.

#### `lime.filesystem.append(path, data)`

Appends data to a file, creating the file and parent directories if needed.

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | string | Relative path within the save directory |
| `data` | string | Content to append |

**Returns:** `true` on success, or `false, error_message` on failure.

#### `lime.filesystem.exists(path)`

Checks if a path exists.

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | string | Relative path within the save directory |

**Returns:** `boolean`

#### `lime.filesystem.isFile(path)`

Checks if a path is a regular file.

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | string | Relative path within the save directory |

**Returns:** `boolean`

#### `lime.filesystem.isDirectory(path)`

Checks if a path is a directory.

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | string | Relative path within the save directory |

**Returns:** `boolean`

#### `lime.filesystem.remove(path)`

Removes a file or empty directory.

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | string | Relative path within the save directory |

**Returns:** `true` on success, or `false, error_message` on failure. Non-empty directories cannot be removed.

#### `lime.filesystem.mkdir(path)`

Creates a directory (and any necessary parent directories).

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | string | Relative path within the save directory |

**Returns:** `true` on success, or `false, error_message` on failure.

#### `lime.filesystem.list([path])`

Lists the contents of a directory.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `path` | string | `""` | Relative path (empty string for save directory root) |

**Returns:** A table of entries on success, where each entry is `{name, type}` with `type` being `"file"` or `"directory"`. Returns `nil, error_message` on failure.

#### `lime.filesystem.pathJoin(path1, path2, ...)`

Joins path components using the appropriate separator and normalizes the result.

| Parameter | Type | Description |
|-----------|------|-------------|
| `path1, path2, ...` | string | Two or more path components |

**Returns:** `string` — the joined and normalized path.

---

## lime.profiler

Simple profiling functions for measuring time spent in code sections. Useful for performance analysis and debugging (the active section name is included in fatal error messages).

### Functions

#### `lime.profiler.start(id)`

Starts timing a named section. If another section was active, it is automatically stopped and its elapsed time is accumulated.

| Parameter | Type | Description |
|-----------|------|-------------|
| `id` | string | Section identifier |

**Note:** Calling `start()` with a new section ID automatically stops the previous section. Time is accumulated across multiple `start`/`stop` cycles for the same section.

#### `lime.profiler.stop()`

Stops timing the currently active section (if any). The elapsed time is accumulated to that section's total.

#### `lime.profiler.list()`

Returns a table containing all registered section IDs.

**Returns:** `table` — array of section ID strings (order is not guaranteed).

#### `lime.profiler.get(id)`

Returns the accumulated time for a section.

| Parameter | Type | Description |
|-----------|------|-------------|
| `id` | string | Section identifier |

**Returns:** `number` — accumulated seconds for the section (high precision). Returns `0` if the section doesn't exist. If the section is currently active, includes the in-progress time.

#### `lime.profiler.reset()`

Resets all section times to zero without removing them. If a section is currently active, its start time is reset to now.

#### `lime.profiler.clear()`

Removes all registered sections and stops any active timing.

### Example Usage

```lua
local lp = lime.profiler

function app:update(dt)
    lp.start("physics")
    -- physics code here
    
    lp.start("ai")
    -- AI code here
    
    lp.start("misc")
    -- other code here
    
    lp.stop()  -- stop timing
end

function app:draw()
    lp.start("render")
    -- drawing code here
    lp.stop()
    
    -- Display profiler stats
    lg.locate(0, 0)
    lg.print(string.format("Physics: %.2fms", lp.get("physics") * 1000))
    lg.locate(1, 0)
    lg.print(string.format("AI: %.2fms", lp.get("ai") * 1000))
    lg.locate(2, 0)
    lg.print(string.format("Render: %.2fms", lp.get("render") * 1000))
    
    lp.reset()  -- reset for next frame
end
```

---

## Top-Level lime Functions

#### `lime.require(module)`

Loads and executes a Lua module, caching the result. Similar to Lua's standard `require` but searches relative to the main script directory.

| Parameter | Type | Description |
|-----------|------|-------------|
| `module` | string | Module name using dot notation (`"utils.helpers"`) or a relative path (`"utils/helpers.lua"`) |

**Returns:** The value returned by the module (typically a table), or `true` if the module returns nothing.

**Note:** In fused mode (when scripts are embedded in the distributed EXE), `lime.require` first searches relative to the main script's directory within the archive, then at the archive root, and finally falls back to disk paths relative to the EXE directory.

#### `lime.scriptDir()`

**Returns:** `string` — the absolute path to the directory containing the main script.

**Note:** In fused mode, returns the EXE directory since scripts are embedded within the executable rather than stored as separate files on disk.

#### `lime.exeDir()`

**Returns:** `string` — the absolute path to the directory containing the Lime2D executable.

#### `lime.cwd()`

**Returns:** `string` — the current working directory.

---

## Global Tables

#### `lime.argv`

A table containing the paths that were passed to the application at startup (e.g., files dragged onto the executable). Index 1 is the first path, etc. May be empty if no files were provided.

---

## Notes

**Main Script Marker:** The main script file must have `-- MAINSCRIPT` as its first line (excluding any UTF-8 BOM).

**Coordinate System:** All pixel coordinates use top-left origin (0,0). Text row/column coordinates also start at (0,0).

**Error Handling:** Out-of-bounds drawing operations will terminate the application with a fatal error. Always validate coordinates before drawing.

**Unavailable Lua Libraries:** The `os`, `io`, `debug`, `package`, `ffi`, and `jit` standard libraries are intentionally disabled. Use `lime.filesystem` for persistent storage.

**Available Lua Libraries:** `base`, `table`, `string`, `math`, `bit`, and `coroutine` are available.

---

## System Hotkeys

Lime2D reserves several function keys for built-in engine functionality. These are processed globally:

| Key | Action |
| --- | --- |
| **F10** | **System Info Screen**: Displays engine version, a CP437 character map reference, and library licenses. |
| **F11** | **Toggle Fullscreen**: Switches the application between windowed and fullscreen modes. |
| **F12** | **Console Screen**: Displays the history of all `print()` calls and system logs. |

---

## Image Asset Workflow

Lime2D provides a utility for converting text-based image data to Lua:

1. Create a text file with your image (example `sprite.txt`):
   ```
   ...##......##...
   ....##....##....
   ..############..
   .##.########.##.
   ################
   #..##########..#
   ```
   - `#` = pixel on
   - `.` = pixel off
   - Width must be a multiple of 8

2. Drag and drop the `.txt` file onto `lime2d-jit.exe`

3. A corresponding `sprite.lua` file will be generated:
   ```lua
   -- Auto-generated by Lime2D from: sprite.txt
   lime.defineImage("sprite", 16, 6, {
     24,24,
     48,12,
     252,63,
     246,111,
     255,255,
     249,159,
   })
   ```

4. Use in your script:
   ```lua
   lime.require("sprite")
   
   function lime.draw()
       lime.graphics.image("sprite", 300, 200)
   end
   ```

---

## Application Distribution

### Fused Executables

Lime2D applications can be distributed as a single, self-contained EXE file by "fusing" your scripts with the engine. A fused executable contains the standard `lime2d-jit.exe` concatenated with a zip archive containing your Lua script file(s).

#### Creating a Fused Application

1. **Create a zip archive**: Zip your script file(s). You can include directories/subdirectories for organizing modules.

2. **Fuse the archive with lime2d-jit.exe**: `copy /b lime2d-jit.exe+script.zip FusedApp.exe`

The resulting `FusedApp.exe` is fully self-contained — no DLLs, no external script files needed.

#### How Fused Mode Works

When Lime2D starts, it automatically detects if a zip archive is appended to the executable. If found:

- The main script is loaded directly from the archive (must contain `-- MAINSCRIPT` marker)
- `lime.require()` resolves modules from within the archive first, then falls back to disk
- `lime.scriptDir()` returns the EXE directory (since scripts live inside the archive)