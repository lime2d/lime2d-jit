#pragma once

#include "Image.h"

#include "lua.hpp"

#include <filesystem>
#include <unordered_map>

class LuaHost
{
public:
    LuaHost() = default;
    ~LuaHost();

    void init();
    void shutdown();
    void loadAppScript(const char* path);
    void loadAppScript(const std::filesystem::path& path);
    void loadFusedScript(const std::string& archivePath);

    void setArgv(const std::vector<std::filesystem::path>& files);
    void setExeDir(const std::filesystem::path& dir);

    void callOnSetActive(bool initial);
    void callUpdate(float dt);
    void callDraw();
    bool callKeyPressed(int key, int scancode, bool isrepeat);
    bool callKeyReleased(int key, int scancode);
    bool callTextInput(unsigned int c); // Mainly for text input scenarios
    bool callQuit();

    bool invokeQuitCallback(); // Invokes lime.quit callback with re-entrancy protection; returns true if quit should be aborted

    // Profiler: returns the currently active section ID, or empty string if none
    std::string getActiveProfilerSection() const { return profilerActiveSection; }

private:
    lua_State* L = nullptr;

    struct OwnedImage
    {
        Image img;
        std::vector<unsigned char> bytes;
    };

    std::unordered_map<std::string, std::unique_ptr<OwnedImage>> images;
    std::filesystem::path mainScriptDir;
    std::vector<std::filesystem::path> argvFiles;

    std::filesystem::path exeDir;

    bool quitCallbackActive = false; // Quit callback re-entrancy guard

    // ---- Sandboxed filesystem ----
    std::string appIdentity;
    std::filesystem::path saveDir;
    bool identityLocked = false;     // Set true after setIdentity has been called
    bool filesystemAccessed = false; // Set true after any sandbox read/write/query operation

    void initSaveDir();
    static std::string sanitizeIdentity(const std::string& raw);
    static std::filesystem::path getUserDataBasePath();
    bool isPathSafe(const std::string& relPath) const;
    std::filesystem::path resolveSavePath(const std::string& relPath) const;

    // ---- Profiler ----
    std::unordered_map<std::string, double> profilerSections; // Section ID -> accumulated seconds
    std::string profilerActiveSection; // Currently active section (empty = none)
    double profilerSectionStart = 0.0; // glfwGetTime() when active section started

    void profilerStopCurrentSection(); // Internal helper to stop timing the current section

    // ---- Fused EXE ----
    std::string fusedBaseDir;

private:
    static int traceback(lua_State* L);

    void openLibsMinimal();
    void registerLime();
    void registerWindowSubtable();
    void registerGraphicsSubtable();
    void registerKeyboardSubtable();
    void registerTimeSubtable();
    void registerFilesystemSubtable();
    void registerProfilerSubtable();

    bool pushLimeCallback(const char* name);
    void pcall(int nargs, int nrets);

    static LuaHost* selfFromUpvalue(lua_State* L);

    // ========================================
    // lime.window bindings
    // ========================================
    static int l_window_index(lua_State* L); // Metatable __index for dynamic WIDTH/HEIGHT
    static int l_window_toggleFullscreen(lua_State* L);
    static int l_window_getFullscreen(lua_State* L);
    static int l_window_setFullscreen(lua_State* L);
    static int l_window_setTitle(lua_State* L);
    static int l_window_quit(lua_State* L);

    // ========================================
    // lime.graphics bindings
    // ========================================
    static int l_graphics_redraw(lua_State* L);     // Set redraw flag | params: ()
    static int l_graphics_setFgColor(lua_State* L); // Set foreground color | params: (r,g,b) - values 0.0-1.0
    static int l_graphics_setBgColor(lua_State* L); // Set background color | params: (r,g,b) - values 0.0-1.0
    static int l_graphics_clear(lua_State* L);      // Clear screen/canvas | params: ([inverted=false])

    // Primitives
    static int l_graphics_pset(lua_State* L);   // Pixel on/off | params: (x,y[,on=true])
    static int l_graphics_pon(lua_State* L);    // Pixel on | params: (x,y)
    static int l_graphics_poff(lua_State* L);   // Pixel off | params: (x,y)
    static int l_graphics_pons(lua_State* L);   // Pixels on | params: ({x1,y1,x2,y2,...})
    static int l_graphics_poffs(lua_State* L);  // Pixels off | params: ({x1,y1,x2,y2,...})
    static int l_graphics_lset(lua_State* L);   // Line on/off | params: (x1,y1,x2,y2[,on=true])
    static int l_graphics_lon(lua_State* L);    // Line on | params: (x1,y1,x2,y2)
    static int l_graphics_loff(lua_State* L);   // Line off | params: (x1,y1,x2,y2)
    static int l_graphics_lsets(lua_State* L);  // Lines on/off | params: ({x1,y1,x2,y2,...}[,on=true])
    static int l_graphics_lsetsc(lua_State* L); // Lines (continuous) on/off | params: ({x1,y1,x2,y2,...}[,on=true])
    static int l_graphics_rset(lua_State* L);   // Rect on/off | params: (x,y,w,h[,solid=true[,on=true]])
    static int l_graphics_ron(lua_State* L);    // Rect on | params: (x,y,w,h[,solid=true])
    static int l_graphics_roff(lua_State* L);   // Rect off | params: (x,y,w,h[,solid=true])
    static int l_graphics_cset(lua_State* L);   // Circle on/off | params: (x,y,size[,solid=true[,on=true]]) - x,y is top-left of circle's bounding box
    static int l_graphics_con(lua_State* L);    // Circle on | params: (x,y,size[,solid=true])
    static int l_graphics_coff(lua_State* L);   // Circle off | params: (x,y,size[,solid=true])
    static int l_graphics_eset(lua_State* L);   // Ellipse on/off | params: (x,y,w,h[,solid=true[,on=true]]) - x,y is top-left of ellipse's bounding box
    static int l_graphics_eon(lua_State* L);    // Ellipse on | params: (x,y,w,h[,solid=true])
    static int l_graphics_eoff(lua_State* L);   // Ellipse off | params: (x,y,w,h[,solid=true])

    // Text
    static int l_graphics_locate(lua_State* L); // Set next print location | params: (row,col) - 0,0 is origin (top-left-most)
    static int l_graphics_print(lua_State* L);  // Print character or text | params: (char_or_string[,inverted=false])
    static int l_graphics_repeat(lua_State* L); // Print a glyph mutliple times | params: (char,n[,inverted=false])
    static int l_graphics_center(lua_State* L); // Print text centered | params: (string,row[,inverted=false])
    static int l_graphics_wrap(lua_State* L);   // Print wrapped text | params: (string,max_rows,max_cols[,scroll_amount=0[,convert_newline_chars=true[,test=false]]])
    static int l_graphics_printInt(lua_State* L); // Print integer | params: (n[,inverted=false])
    static int l_graphics_textFill(lua_State* L); // Fill region with glyph | params: textFill(row,col,rows,cols,glyph[,inverted=false])
    static int l_graphics_textBox(lua_State* L); // Draw bordered text box | params: (row,col,rows,cols[,border_style=1[,fill_glyph=32[,inverted=false]]])
    static int l_graphics_textScrollbarV(lua_State* L); // Draw vertical scrollbar (track & slider) using CP437 characters | params: (row,col,length,current_scroll,max_scroll,visible_rows)
    static int l_graphics_textScrollbarH(lua_State* L); // Draw horizontal scrollbar (track & slider) using CP437 characters | params: (row,col,length,current_scroll,max_scroll,visible_cols)


    // Images
    static int l_graphics_defineImage(lua_State* L); // Define 1-bpp image | params: (handle_as_string,w,h,{bytes}) - width must be a multiple of 8, each byte represents 8 pixels
    static int l_graphics_image(lua_State* L); // Draw image | params: (handle_as_string,row,col[,draw_bg = true[,dy = 0]])

    // ========================================
    // lime.keyboard bindings
    // ========================================
    static int l_keyboard_isDown(lua_State* L); // Query key state | params: (keycode) | returns boolean
    static int l_keyboard_ctrlIsDown(lua_State* L);
    static int l_keyboard_altIsDown(lua_State* L);
    static int l_keyboard_shiftIsDown(lua_State* L);

    // ========================================
    // lime.time bindings
    // ========================================
    static int l_time_sinceStart(lua_State* L); // Get seconds since start of app | params: () | returns number
    static int l_time_sinceEpoch(lua_State* L); // Get seconds since UNIX epoch | params: () | returns integer

    // ========================================
    // lime.filesystem bindings
    // ========================================
    static int l_fs_setIdentity(lua_State* L);
    static int l_fs_getSaveDir(lua_State* L);
    static int l_fs_read(lua_State* L);
    static int l_fs_write(lua_State* L);
    static int l_fs_append(lua_State* L);
    static int l_fs_exists(lua_State* L);
    static int l_fs_isFile(lua_State* L);
    static int l_fs_isDirectory(lua_State* L);
    static int l_fs_remove(lua_State* L);
    static int l_fs_mkdir(lua_State* L);
    static int l_fs_list(lua_State* L);
    static int l_fs_pathJoin(lua_State* L);

    // ========================================
    // lime.profiler bindings
    // ========================================
    static int l_profiler_start(lua_State* L); // Start/switch to section | params: (id_string)
    static int l_profiler_stop(lua_State* L);  // Stop current section | params: ()
    static int l_profiler_list(lua_State* L);  // List all sections | params: () | returns table of strings
    static int l_profiler_get(lua_State* L);   // Get section time | params: (id_string) | returns number (seconds)
    static int l_profiler_reset(lua_State* L); // Reset all times to 0 | params: ()
    static int l_profiler_clear(lua_State* L); // Remove all sections | params: ()

    // ========================================
    // Top-level lime bindings
    // ========================================
    static int l_require(lua_State* L);
    static int l_print(lua_State* L);
    static int l_scriptDir(lua_State* L);
    static int l_exeDir(lua_State* L);
    static int l_cwd(lua_State* L);
};