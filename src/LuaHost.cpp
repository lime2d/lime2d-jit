#include "LuaHost.h"

#include "App.h"
#include "FusedArchive.h"
#include "misc.h"
#include "MonospaceMonochromePixelFont.h"
#include "Screen.h"
#include "Window.h"

#include <iostream>

// ============================================================================
// LuaJIT Compatibility Helpers
// ============================================================================
// LuaJIT implements the Lua 5.1 C API with selected 5.2 extensions.
// The following cover the 5.3+/5.5 API surface used by this codebase.

// lua_rawlen: Lua 5.2+ name for lua_objlen
#ifndef lua_rawlen
#define lua_rawlen(L, idx) lua_objlen(L, idx)
#endif

// luaL_tolstring replacement (Lua 5.3+). Converts any stack value to a string,
// honouring __tostring, and pushes the result. Returns a pointer to the string.
static const char* lime_tolstring(lua_State* L, int idx, size_t* len)
{
    if (luaL_callmeta(L, idx, "__tostring"))
    {
        if (!lua_isstring(L, -1))
            luaL_error(L, "'__tostring' must return a string");
    }
    else
    {
        switch (lua_type(L, idx))
        {
        case LUA_TNUMBER:
        case LUA_TSTRING:
            lua_pushvalue(L, idx);
            break;
        case LUA_TBOOLEAN:
            lua_pushstring(L, lua_toboolean(L, idx) ? "true" : "false");
            break;
        case LUA_TNIL:
            lua_pushliteral(L, "nil");
            break;
        default:
            lua_pushfstring(L, "%s: %p", luaL_typename(L, idx), lua_topointer(L, idx));
            break;
        }
    }
    return lua_tolstring(L, -1, len);
}

// End of LuaJIT Compatibility Helpers

static std::string pathToUtf8(const fs::path& p)
{
#ifdef _WIN32
    std::u8string u8 = p.u8string();
    return std::string((const char*)u8.data(), (const char*)u8.data() + u8.size());
#else
    return p.string();
#endif
}

static fs::path luaUtf8ToPath(lua_State* L, int idx)
{
    size_t len = 0;
    const char* s = luaL_checklstring(L, idx, &len);
    // Lua strings are byte strings; we interpret them as UTF-8
    const char8_t* b = reinterpret_cast<const char8_t*>(s);
    std::u8string u8(b, b + len);
    return fs::path(u8);
}

static Screen* requireScreen(lua_State* L)
{
    if (!screen) luaL_error(L, "Lime2D: no active screen");
    return screen;
}

LuaHost* LuaHost::selfFromUpvalue(lua_State* L)
{
    return static_cast<LuaHost*>(lua_touserdata(L, lua_upvalueindex(1)));
}

LuaHost::~LuaHost()
{
    shutdown();
}

void LuaHost::init()
{
    if (L) return;

    L = luaL_newstate();
    if (!L) throw std::runtime_error("luaL_newstate failed");

    openLibsMinimal();
    registerLime();

    cout(" Lua Host [initialized]");
}

void LuaHost::shutdown()
{
    if (!L) return;

    images.clear();

    // Clear profiler state
    profilerSections.clear();
    profilerActiveSection.clear();
    profilerSectionStart = 0.0;

    lua_close(L);
    L = nullptr;

    cout(" Lua Host [ok]");
}

int LuaHost::traceback(lua_State* L)
{
    const char* msg = lua_tostring(L, 1);
    if (msg) luaL_traceback(L, L, msg, 1);
    else lua_pushliteral(L, "(error object is not a string)");
    return 1;
}

void LuaHost::pcall(int nargs, int nrets)
{
    int funcIndex = lua_gettop(L) - nargs;
    lua_pushcfunction(L, &LuaHost::traceback);
    lua_insert(L, funcIndex);
    int errFuncIndex = funcIndex;

    int status = lua_pcall(L, nargs, nrets, errFuncIndex);

    lua_remove(L, errFuncIndex);

    if (status != LUA_OK)
    {
        std::string err = lua_tostring(L, -1) ? lua_tostring(L, -1) : "Lua error (non-string)";
        lua_pop(L, 1);
        throw std::runtime_error(err);
    }
}

static void openLib(lua_State* L, const char* name, lua_CFunction openf)
{
    lua_pushcfunction(L, openf);
    lua_pushstring(L, name);
    lua_call(L, 1, 0);
}

void LuaHost::openLibsMinimal()
{
    openLib(L, "", luaopen_base); // includes coroutine in LuaJIT
    openLib(L, LUA_TABLIBNAME, luaopen_table);
    openLib(L, LUA_STRLIBNAME, luaopen_string);
    openLib(L, LUA_MATHLIBNAME, luaopen_math);
    openLib(L, LUA_BITLIBNAME, luaopen_bit); // LuaJIT bit operations
}

// ============================================================================
// Registration - Main Entry Point
// ============================================================================

void LuaHost::registerLime()
{
    lua_pushcfunction(L, &LuaHost::l_print);
    lua_setglobal(L, "print"); // Override standard print func to send output to std::cout

    lua_newtable(L); // lime table

    // Register subtables
    registerWindowSubtable();
    registerGraphicsSubtable();
    registerKeyboardSubtable();
    registerTimeSubtable();
    registerFilesystemSubtable();
    registerProfilerSubtable();

    // Top-level functions
    lua_pushlightuserdata(L, this);
    lua_pushcclosure(L, &LuaHost::l_require, 1);
    lua_setfield(L, -2, "require");

    lua_pushlightuserdata(L, this);
    lua_pushcclosure(L, &LuaHost::l_scriptDir, 1);
    lua_setfield(L, -2, "scriptDir");

    lua_pushlightuserdata(L, this);
    lua_pushcclosure(L, &LuaHost::l_exeDir, 1);
    lua_setfield(L, -2, "exeDir");

    lua_pushcfunction(L, &LuaHost::l_cwd);
    lua_setfield(L, -2, "cwd");

    lua_setglobal(L, "lime");
}

// ============================================================================
// lime.window Subtable
// ============================================================================

void LuaHost::registerWindowSubtable()
{
    lua_newtable(L); // lime.window table

    // Functions
    lua_pushcfunction(L, &LuaHost::l_window_toggleFullscreen);
    lua_setfield(L, -2, "toggleFullscreen");

    lua_pushcfunction(L, &LuaHost::l_window_getFullscreen);
    lua_setfield(L, -2, "getFullscreen");

    lua_pushcfunction(L, &LuaHost::l_window_setFullscreen);
    lua_setfield(L, -2, "setFullscreen");

    lua_pushcfunction(L, &LuaHost::l_window_setTitle);
    lua_setfield(L, -2, "setTitle");

    lua_pushcfunction(L, &LuaHost::l_window_quit);
    lua_setfield(L, -2, "quit");

    // Set up metatable for dynamic WIDTH/HEIGHT access
    lua_newtable(L); // metatable
    lua_pushcfunction(L, &LuaHost::l_window_index);
    lua_setfield(L, -2, "__index");
    lua_setmetatable(L, -2);

    lua_setfield(L, -2, "window"); // lime.window = {...}
}

int LuaHost::l_window_index(lua_State* L)
{
    // Called when accessing a key that doesn't exist in lime.window
    // Args: table (lime.window), key
    const char* key = lua_tostring(L, 2);
    if (!key) return 0;

    if (strcmp(key, "WIDTH") == 0)
    {
        lua_pushinteger(L, window.width);
        return 1;
    }
    if (strcmp(key, "HEIGHT") == 0)
    {
        lua_pushinteger(L, window.height);
        return 1;
    }

    // Key not found
    lua_pushnil(L);
    return 1;
}

int LuaHost::l_window_toggleFullscreen(lua_State* L)
{
    bool fs = window.toggleFullscreen();
    lua_pushboolean(L, fs);
    return 1;
}

int LuaHost::l_window_getFullscreen(lua_State* L)
{
    lua_pushboolean(L, window.getFullscreen());
    return 1;
}

int LuaHost::l_window_setFullscreen(lua_State* L)
{
    bool fs = lua_toboolean(L, 1) != 0;
    window.setFullscreen(fs);
    return 0;
}

int LuaHost::l_window_setTitle(lua_State* L)
{
    const char* title = luaL_checkstring(L, 1);
    window.setTitle(title);
    return 0;
}

int LuaHost::l_window_quit(lua_State* L)
{
    int code = (int)luaL_optinteger(L, 1, 0);

    // Invoke lime.quit callback, matching the behavior of the window close button.
    // If lime.quit returns true, the quit is aborted.
    bool abort = false;
    std::string error;
    try
    {
        abort = lua.invokeQuitCallback();
    }
    catch (const std::exception& e)
    {
        error = e.what();
    }

    if (!error.empty())
        return luaL_error(L, "Error in lime.quit callback: %s", error.c_str());

    if (abort)
        return 0;

    app.shutdown(code);
    return 0;
}

// ============================================================================
// lime.graphics Subtable
// ============================================================================

void LuaHost::registerGraphicsSubtable()
{
    lua_newtable(L); // lime.graphics table

    // Static canvas constants
    lua_pushinteger(L, Screen::width); lua_setfield(L, -2, "WIDTH");
    lua_pushinteger(L, Screen::height); lua_setfield(L, -2, "HEIGHT");
    lua_pushinteger(L, Screen::text_offset_y); lua_setfield(L, -2, "TEXT_OFFSET_Y");
    lua_pushinteger(L, Screen::rows); lua_setfield(L, -2, "ROWS");
    lua_pushinteger(L, Screen::cols); lua_setfield(L, -2, "COLS");

    // Functions
    static const luaL_Reg graphicsFns[] = {
        {"redraw", l_graphics_redraw},
        {"setFgColor", l_graphics_setFgColor},
        {"setBgColor", l_graphics_setBgColor},
        {"clear", l_graphics_clear},

        // Primitives
        {"pset", l_graphics_pset},
        {"pon", l_graphics_pon},
        {"poff", l_graphics_poff},
        {"pons", l_graphics_pons},
        {"poffs", l_graphics_poffs},
        {"lset", l_graphics_lset},
        {"lon", l_graphics_lon},
        {"loff", l_graphics_loff},
        {"lsets", l_graphics_lsets},
        {"lsetsc", l_graphics_lsetsc},
        {"rset", l_graphics_rset},
        {"ron", l_graphics_ron},
        {"roff", l_graphics_roff},
        {"cset", l_graphics_cset},
        {"con", l_graphics_con},
        {"coff", l_graphics_coff},
        {"eset", l_graphics_eset},
        {"eon", l_graphics_eon},
        {"eoff", l_graphics_eoff},

        // Text
        {"locate", l_graphics_locate},
        {"print", l_graphics_print},
        {"repeat", l_graphics_repeat},
        {"center", l_graphics_center},
        {"wrap", l_graphics_wrap},
        {"printInt", l_graphics_printInt},
        {"textFill", l_graphics_textFill},
        {"textBox", l_graphics_textBox},
        {"textScrollbarV", l_graphics_textScrollbarV},
        {"textScrollbarH", l_graphics_textScrollbarH},

        {nullptr, nullptr}
    };
    luaL_setfuncs(L, graphicsFns, 0);

    // Image functions need 'this' pointer via upvalue
    lua_pushlightuserdata(L, this);
    lua_pushcclosure(L, &LuaHost::l_graphics_defineImage, 1);
    lua_setfield(L, -2, "defineImage");

    lua_pushlightuserdata(L, this);
    lua_pushcclosure(L, &LuaHost::l_graphics_image, 1);
    lua_setfield(L, -2, "image");

    lua_setfield(L, -2, "graphics"); // lime.graphics = {...}
}

int LuaHost::l_graphics_redraw(lua_State* L)
{
    requireScreen(L)->redraw = true;
    return 0;
}

int LuaHost::l_graphics_setFgColor(lua_State* L)
{
    float r = (float)luaL_checknumber(L, 1);
    float g = (float)luaL_checknumber(L, 2);
    float b = (float)luaL_checknumber(L, 3);
    app.setColor(r, g, b);
    return 0;
}

int LuaHost::l_graphics_setBgColor(lua_State* L)
{
    float r = (float)luaL_checknumber(L, 1);
    float g = (float)luaL_checknumber(L, 2);
    float b = (float)luaL_checknumber(L, 3);
    app.setColor(r, g, b, false);
    return 0;
}

int LuaHost::l_graphics_clear(lua_State* L)
{
    bool inverted = lua_toboolean(L, 1) != 0;
    requireScreen(L)->clear(inverted);
    return 0;
}

int LuaHost::l_graphics_pset(lua_State* L)
{
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    bool on = lua_isnone(L, 3) ? true : (lua_toboolean(L, 3) != 0);
    if (x < 0 || x >= Screen::width || y < 0 || y >= Screen::height)
        return luaL_error(L, "lime.graphics.pset: out of bounds (%d,%d)", x, y);
    if (on) ponUnsafe(x, y);
    else poffUnsafe(x, y);
    return 0;
}

int LuaHost::l_graphics_pon(lua_State* L)
{
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    if (x < 0 || x >= Screen::width || y < 0 || y >= Screen::height)
        return luaL_error(L, "lime.graphics.pon: out of bounds (%d,%d)", x, y);
    ponUnsafe(x, y);
    return 0;
}

int LuaHost::l_graphics_poff(lua_State* L)
{
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    if (x < 0 || x >= Screen::width || y < 0 || y >= Screen::height)
        return luaL_error(L, "lime.graphics.poff: out of bounds (%d,%d)", x, y);
    poffUnsafe(x, y);
    return 0;
}

static void getIntFromArrayTable(lua_State* L, int tindex, int i, lua_Integer* out)
{
    lua_rawgeti(L, tindex, i);
    *out = luaL_checkinteger(L, -1);
    lua_pop(L, 1);
}

int LuaHost::l_graphics_pons(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    int n = (int)lua_rawlen(L, 1);
    if ((n % 2) != 0)
        return luaL_error(L, "lime.graphics.pons: coordinate list length must be even");

    int w{ Screen::width }, h{ Screen::height };
    static const char* ERR_BOUNDS = "lime.graphics.pons: out of bounds (%d,%d)";

    int i = 1;
    for (; i + 7 <= n; i += 8)
    {
        lua_rawgeti(L, 1, i + 0); lua_rawgeti(L, 1, i + 1);
        lua_rawgeti(L, 1, i + 2); lua_rawgeti(L, 1, i + 3);
        lua_rawgeti(L, 1, i + 4); lua_rawgeti(L, 1, i + 5);
        lua_rawgeti(L, 1, i + 6); lua_rawgeti(L, 1, i + 7);

        int x, y;

        x = (int)lua_tointeger(L, -8); y = (int)lua_tointeger(L, -7);
        if (x < 0 || x >= w || y < 0 || y >= h)
            return luaL_error(L, ERR_BOUNDS, x, y);
        ponUnsafe(x, y);

        x = (int)lua_tointeger(L, -6); y = (int)lua_tointeger(L, -5);
        if (x < 0 || x >= w || y < 0 || y >= h)
            return luaL_error(L, ERR_BOUNDS, x, y);
        ponUnsafe(x, y);

        x = (int)lua_tointeger(L, -4); y = (int)lua_tointeger(L, -3);
        if (x < 0 || x >= w || y < 0 || y >= h)
            return luaL_error(L, ERR_BOUNDS, x, y);
        ponUnsafe(x, y);

        x = (int)lua_tointeger(L, -2); y = (int)lua_tointeger(L, -1);
        if (x < 0 || x >= w || y < 0 || y >= h)
            return luaL_error(L, ERR_BOUNDS, x, y);
        ponUnsafe(x, y);

        lua_pop(L, 8);
    }

    for (; i + 1 <= n; i += 2)
    {
        lua_rawgeti(L, 1, i);
        lua_rawgeti(L, 1, i + 1);
        int x = (int)lua_tointeger(L, -2);
        int y = (int)lua_tointeger(L, -1);
        if (x < 0 || x >= w || y < 0 || y >= h)
            return luaL_error(L, ERR_BOUNDS, x, y);
        ponUnsafe(x, y);
        lua_pop(L, 2);
    }

    return 0;
}

int LuaHost::l_graphics_poffs(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    int n = (int)lua_rawlen(L, 1);
    if ((n % 2) != 0)
        return luaL_error(L, "lime.graphics.poffs: coordinate list length must be even");

    int w{ Screen::width }, h{ Screen::height };
    static const char* ERR_BOUNDS = "lime.graphics.poffs: out of bounds (%d,%d)";

    int i = 1;
    for (; i + 7 <= n; i += 8)
    {
        lua_rawgeti(L, 1, i + 0); lua_rawgeti(L, 1, i + 1);
        lua_rawgeti(L, 1, i + 2); lua_rawgeti(L, 1, i + 3);
        lua_rawgeti(L, 1, i + 4); lua_rawgeti(L, 1, i + 5);
        lua_rawgeti(L, 1, i + 6); lua_rawgeti(L, 1, i + 7);

        int x, y;

        x = (int)lua_tointeger(L, -8); y = (int)lua_tointeger(L, -7);
        if (x < 0 || x >= w || y < 0 || y >= h)
            return luaL_error(L, ERR_BOUNDS, x, y);
        poffUnsafe(x, y);

        x = (int)lua_tointeger(L, -6); y = (int)lua_tointeger(L, -5);
        if (x < 0 || x >= w || y < 0 || y >= h)
            return luaL_error(L, ERR_BOUNDS, x, y);
        poffUnsafe(x, y);

        x = (int)lua_tointeger(L, -4); y = (int)lua_tointeger(L, -3);
        if (x < 0 || x >= w || y < 0 || y >= h)
            return luaL_error(L, ERR_BOUNDS, x, y);
        poffUnsafe(x, y);

        x = (int)lua_tointeger(L, -2); y = (int)lua_tointeger(L, -1);
        if (x < 0 || x >= w || y < 0 || y >= h)
            return luaL_error(L, ERR_BOUNDS, x, y);
        poffUnsafe(x, y);

        lua_pop(L, 8);
    }

    for (; i + 1 <= n; i += 2)
    {
        lua_rawgeti(L, 1, i);
        lua_rawgeti(L, 1, i + 1);
        int x = (int)lua_tointeger(L, -2);
        int y = (int)lua_tointeger(L, -1);
        if (x < 0 || x >= w || y < 0 || y >= h)
            return luaL_error(L, ERR_BOUNDS, x, y);
        poffUnsafe(x, y);
        lua_pop(L, 2);
    }

    return 0;
}

int LuaHost::l_graphics_lset(lua_State* L)
{
    int x1 = (int)luaL_checkinteger(L, 1);
    int y1 = (int)luaL_checkinteger(L, 2);
    int x2 = (int)luaL_checkinteger(L, 3);
    int y2 = (int)luaL_checkinteger(L, 4);
    bool on = lua_isnone(L, 5) ? true : (lua_toboolean(L, 5) != 0);
    if (x1 < 0 || x1 >= Screen::width || y1 < 0 || y1 >= Screen::height ||
        x2 < 0 || x2 >= Screen::width || y2 < 0 || y2 >= Screen::height)
        return luaL_error(L, "lime.graphics.lset: out of bounds (%d,%d)-(%d,%d)", x1, y1, x2, y2);
    requireScreen(L)->lset(x1, y1, x2, y2, on);
    return 0;
}

int LuaHost::l_graphics_lon(lua_State* L)
{
    int x1 = (int)luaL_checkinteger(L, 1);
    int y1 = (int)luaL_checkinteger(L, 2);
    int x2 = (int)luaL_checkinteger(L, 3);
    int y2 = (int)luaL_checkinteger(L, 4);
    if (x1 < 0 || x1 >= Screen::width || y1 < 0 || y1 >= Screen::height ||
        x2 < 0 || x2 >= Screen::width || y2 < 0 || y2 >= Screen::height)
        return luaL_error(L, "lime.graphics.lon: out of bounds (%d,%d)-(%d,%d)", x1, y1, x2, y2);
    requireScreen(L)->lon(x1, y1, x2, y2);
    return 0;
}

int LuaHost::l_graphics_loff(lua_State* L)
{
    int x1 = (int)luaL_checkinteger(L, 1);
    int y1 = (int)luaL_checkinteger(L, 2);
    int x2 = (int)luaL_checkinteger(L, 3);
    int y2 = (int)luaL_checkinteger(L, 4);
    if (x1 < 0 || x1 >= Screen::width || y1 < 0 || y1 >= Screen::height ||
        x2 < 0 || x2 >= Screen::width || y2 < 0 || y2 >= Screen::height)
        return luaL_error(L, "lime.graphics.loff: out of bounds (%d,%d)-(%d,%d)", x1, y1, x2, y2);
    requireScreen(L)->loff(x1, y1, x2, y2);
    return 0;
}

int LuaHost::l_graphics_lsets(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    bool on = lua_isnone(L, 2) ? true : (lua_toboolean(L, 2) != 0);

    int n = (int)lua_rawlen(L, 1);
    if ((n % 4) != 0) return luaL_error(L, "lime.graphics.lsets: list length must be a multiple of 4");

    Screen* s = requireScreen(L);
    int w{ Screen::width }, h{ Screen::height };

    for (int i = 1; i <= n; i += 4)
    {
        lua_Integer x1, y1, x2, y2;
        getIntFromArrayTable(L, 1, i, &x1);
        getIntFromArrayTable(L, 1, i + 1, &y1);
        getIntFromArrayTable(L, 1, i + 2, &x2);
        getIntFromArrayTable(L, 1, i + 3, &y2);
        if (x1 < 0 || x1 >= w || y1 < 0 || y1 >= h ||
            x2 < 0 || x2 >= w || y2 < 0 || y2 >= h)
            return luaL_error(L, "lime.graphics.lsets: out of bounds (%d,%d)-(%d,%d)", x1, y1, x2, y2);
        s->lset((int)x1, (int)y1, (int)x2, (int)y2, on);
    }
    return 0;
}

int LuaHost::l_graphics_lsetsc(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    bool on = lua_isnone(L, 2) ? true : (lua_toboolean(L, 2) != 0);

    int n = (int)lua_rawlen(L, 1);
    if ((n % 2) != 0) return luaL_error(L, "lime.graphics.lsetsc: coordinate list length must be even");
    if (n < 4) return 0;

    Screen* s = requireScreen(L);
    int w{ Screen::width }, h{ Screen::height };

    lua_Integer px, py;
    getIntFromArrayTable(L, 1, 1, &px);
    getIntFromArrayTable(L, 1, 2, &py);
    if (px < 0 || px >= w || py < 0 || py >= h)
    {
        lua_Integer x, y;
        getIntFromArrayTable(L, 1, 3, &x);
        getIntFromArrayTable(L, 1, 4, &y);
        return luaL_error(L, "lime.graphics.lsetsc: out of bounds (%d,%d)-(%d,%d)", px, py, x, y);
    }

    for (int i = 3; i <= n; i += 2)
    {
        lua_Integer x, y;
        getIntFromArrayTable(L, 1, i, &x);
        getIntFromArrayTable(L, 1, i + 1, &y);
        if (x < 0 || x >= w || y < 0 || y >= h)
            return luaL_error(L, "lime.graphics.lsetsc: out of bounds (%d,%d)-(%d,%d)", px, py, x, y);
        s->lset((int)px, (int)py, (int)x, (int)y, on);
        px = x; py = y;
    }
    return 0;
}

int LuaHost::l_graphics_rset(lua_State* L)
{
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    int w = (int)luaL_checkinteger(L, 3);
    int h = (int)luaL_checkinteger(L, 4);
    bool solid = lua_isnone(L, 5) ? true : (lua_toboolean(L, 5) != 0);
    bool on = lua_isnone(L, 6) ? true : (lua_toboolean(L, 6) != 0);
    if (w < 0) x -= (w = -w);
    if (h < 0) y -= (h = -h);
    if (x < 0 || (x + w - 1) >= Screen::width || y < 0 || (y + h - 1) >= Screen::height)
        return luaL_error(L, "lime.graphics.rset: out of bounds (%d,%d)-(%d,%d)", x, y, x + w - 1, y + h - 1);
    requireScreen(L)->rset(x, y, w, h, solid, on);
    return 0;
}

int LuaHost::l_graphics_ron(lua_State* L)
{
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    int w = (int)luaL_checkinteger(L, 3);
    int h = (int)luaL_checkinteger(L, 4);
    bool solid = lua_isnone(L, 5) ? true : (lua_toboolean(L, 5) != 0);
    if (w < 0) x -= (w = -w);
    if (h < 0) y -= (h = -h);
    if (x < 0 || (x + w - 1) >= Screen::width || y < 0 || (y + h - 1) >= Screen::height)
        return luaL_error(L, "lime.graphics.ron: out of bounds (%d,%d)-(%d,%d)", x, y, x + w - 1, y + h - 1);
    requireScreen(L)->ron(x, y, w, h, solid);
    return 0;
}

int LuaHost::l_graphics_roff(lua_State* L)
{
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    int w = (int)luaL_checkinteger(L, 3);
    int h = (int)luaL_checkinteger(L, 4);
    bool solid = lua_isnone(L, 5) ? true : (lua_toboolean(L, 5) != 0);
    if (w < 0) x -= (w = -w);
    if (h < 0) y -= (h = -h);
    if (x < 0 || (x + w - 1) >= Screen::width || y < 0 || (y + h - 1) >= Screen::height)
        return luaL_error(L, "lime.graphics.roff: out of bounds (%d,%d)-(%d,%d)", x, y, x + w - 1, y + h - 1);
    requireScreen(L)->roff(x, y, w, h, solid);
    return 0;
}

int LuaHost::l_graphics_cset(lua_State* L)
{
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    int size = (int)luaL_checkinteger(L, 3);
    bool solid = lua_isnone(L, 4) ? true : (lua_toboolean(L, 4) != 0);
    bool on = lua_isnone(L, 5) ? true : (lua_toboolean(L, 5) != 0);
    if (size < 0)
    {
        x += size;
        y += size;
        size = -size;
    }
    if (x < 0 || (x + size - 1) >= Screen::width || y < 0 || (y + size - 1) >= Screen::height)
        return luaL_error(L, "lime.graphics.cset: out of bounds (%d,%d)-(%d,%d)", x, y, x + size - 1, y + size - 1);
    requireScreen(L)->cset(x, y, size, solid, on);
    return 0;
}

int LuaHost::l_graphics_con(lua_State* L)
{
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    int size = (int)luaL_checkinteger(L, 3);
    bool solid = lua_isnone(L, 4) ? true : (lua_toboolean(L, 4) != 0);
    if (size < 0)
    {
        x += size;
        y += size;
        size = -size;
    }
    if (x < 0 || (x + size - 1) >= Screen::width || y < 0 || (y + size - 1) >= Screen::height)
        return luaL_error(L, "lime.graphics.con: out of bounds (%d,%d)-(%d,%d)", x, y, x + size - 1, y + size - 1);
    requireScreen(L)->con(x, y, size, solid);
    return 0;
}

int LuaHost::l_graphics_coff(lua_State* L)
{
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    int size = (int)luaL_checkinteger(L, 3);
    bool solid = lua_isnone(L, 4) ? true : (lua_toboolean(L, 4) != 0);
    if (size < 0)
    {
        x += size;
        y += size;
        size = -size;
    }
    if (x < 0 || (x + size - 1) >= Screen::width || y < 0 || (y + size - 1) >= Screen::height)
        return luaL_error(L, "lime.graphics.coff: out of bounds (%d,%d)-(%d,%d)", x, y, x + size - 1, y + size - 1);
    requireScreen(L)->coff(x, y, size, solid);
    return 0;
}

int LuaHost::l_graphics_eset(lua_State* L)
{
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    int w = (int)luaL_checkinteger(L, 3);
    int h = (int)luaL_checkinteger(L, 4);
    bool solid = lua_isnone(L, 5) ? true : (lua_toboolean(L, 5) != 0);
    bool on = lua_isnone(L, 6) ? true : (lua_toboolean(L, 6) != 0);
    if (w < 0) x -= (w = -w);
    if (h < 0) y -= (h = -h);
    if (x < 0 || (x + w - 1) >= Screen::width || y < 0 || (y + h - 1) >= Screen::height)
        return luaL_error(L, "lime.graphics.eset: out of bounds (%d,%d)-(%d,%d)", x, y, x + w - 1, y + h - 1);
    requireScreen(L)->eset(x, y, w, h, solid, on);
    return 0;
}

int LuaHost::l_graphics_eon(lua_State* L)
{
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    int w = (int)luaL_checkinteger(L, 3);
    int h = (int)luaL_checkinteger(L, 4);
    bool solid = lua_isnone(L, 5) ? true : (lua_toboolean(L, 5) != 0);
    if (w < 0) x -= (w = -w);
    if (h < 0) y -= (h = -h);
    if (x < 0 || (x + w - 1) >= Screen::width || y < 0 || (y + h - 1) >= Screen::height)
        return luaL_error(L, "lime.graphics.eon: out of bounds (%d,%d)-(%d,%d)", x, y, x + w - 1, y + h - 1);
    requireScreen(L)->eon(x, y, w, h, solid);
    return 0;
}

int LuaHost::l_graphics_eoff(lua_State* L)
{
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    int w = (int)luaL_checkinteger(L, 3);
    int h = (int)luaL_checkinteger(L, 4);
    bool solid = lua_isnone(L, 5) ? true : (lua_toboolean(L, 5) != 0);
    if (w < 0) x -= (w = -w);
    if (h < 0) y -= (h = -h);
    if (x < 0 || (x + w - 1) >= Screen::width || y < 0 || (y + h - 1) >= Screen::height)
        return luaL_error(L, "lime.graphics.eoff: out of bounds (%d,%d)-(%d,%d)", x, y, x + w - 1, y + h - 1);
    requireScreen(L)->eoff(x, y, w, h, solid);
    return 0;
}

int LuaHost::l_graphics_locate(lua_State* L)
{
    int row = (int)luaL_checkinteger(L, 1);
    int col = (int)luaL_checkinteger(L, 2);
    requireScreen(L)->locate(row, col);
    return 0;
}

int LuaHost::l_graphics_print(lua_State* L)
{
    bool inverted = lua_toboolean(L, 2) != 0;

    Screen* s = requireScreen(L);
    if (lua_type(L, 1) == LUA_TNUMBER)
    {
        int glyph = (int)luaL_checkinteger(L, 1);
        if (glyph < 0 || glyph >= (Screen::font)->num_glyphs)
            return luaL_error(L, "lime.graphics.print: invalid glyph index (%d)", glyph);
        s->print(glyph, inverted);
    }
    else
    {
        const char* text = luaL_checkstring(L, 1);
        s->print(text, inverted);
    }
    return 0;
}

int LuaHost::l_graphics_repeat(lua_State* L)
{
    int glyph = (int)luaL_checkinteger(L, 1);
    int n = (int)luaL_checkinteger(L, 2);
    bool inverted = lua_toboolean(L, 3) != 0;
    requireScreen(L)->repeat(glyph, n, inverted);
    return 0;
}

int LuaHost::l_graphics_center(lua_State* L)
{
    const char* text = luaL_checkstring(L, 1);
    int row = (int)luaL_checkinteger(L, 2);
    bool inverted = lua_toboolean(L, 3) != 0;
    requireScreen(L)->center(text, row, inverted);
    return 0;
}

int LuaHost::l_graphics_wrap(lua_State* L)
{
    const char* text = luaL_checkstring(L, 1);
    int max_rows = (int)luaL_checkinteger(L, 2);
    int max_cols = (int)luaL_checkinteger(L, 3);
    int scrolling = lua_isnone(L, 4) ? 0 : (int)luaL_checkinteger(L, 4);
    bool convert = lua_isnone(L, 5) ? true : (lua_toboolean(L, 5) != 0);
    bool test = lua_toboolean(L, 6) != 0;

    int lines = requireScreen(L)->wrap(text, max_rows, max_cols, scrolling, convert, test);

    lua_pushinteger(L, lines);
    lua_pushinteger(L, scrolling);
    return 2;
}

int LuaHost::l_graphics_printInt(lua_State* L)
{
    int n = (int)luaL_checkinteger(L, 1);
    bool inverted = lua_toboolean(L, 2) != 0;
    requireScreen(L)->printInt(n, inverted);
    return 0;
}

int LuaHost::l_graphics_textFill(lua_State* L)
{
    int row = (int)luaL_checkinteger(L, 1);
    int col = (int)luaL_checkinteger(L, 2);
    int nrows = (int)luaL_checkinteger(L, 3);
    int ncols = (int)luaL_checkinteger(L, 4);
    int glyph = (int)luaL_checkinteger(L, 5);
    bool inverted = lua_toboolean(L, 6) != 0;

    if (nrows < 1 || ncols < 1)
        return luaL_error(L, "lime.graphics.textFill: invalid size [%dx%d]", nrows, ncols);
    int erow = row + nrows - 1;
    int ecol = col + ncols - 1;
    if (row < 0 || col < 0 || erow >= Screen::rows || ecol >= Screen::cols)
        return luaL_error(L, "lime.graphics.textFill: out of bounds [%d,%d]-[%d,%d]", row, col, erow, ecol);
    if (glyph < 0 || glyph >= (Screen::font)->num_glyphs)
        return luaL_error(L, "lime.graphics.textFill: invalid glyph index (%d)", glyph);

    requireScreen(L)->textFill(row, col, nrows, ncols, glyph, inverted);

    return 0;
}

int LuaHost::l_graphics_textBox(lua_State* L)
{
    int row = (int)luaL_checkinteger(L, 1);
    int col = (int)luaL_checkinteger(L, 2);
    int nrows = (int)luaL_checkinteger(L, 3);
    int ncols = (int)luaL_checkinteger(L, 4);
    int border_style = (int)luaL_checkinteger(L, 5);
    int fill_glyph = lua_isnone(L, 6) ? 32 : (int)luaL_checkinteger(L, 6);
    bool inverted = lua_toboolean(L, 7) != 0;

    if (nrows < 2 || ncols < 2)
        return luaL_error(L, "lime.graphics.textBox: invalid size [%dx%d], minimum is 2x2", nrows, ncols);
    if (border_style < 0 || border_style > 3)
        return luaL_error(L, "lime.graphics.textBox: invalid border style (%d), must be 0-3", border_style);
    int erow = row + nrows - 1;
    int ecol = col + ncols - 1;
    if (row < 0 || col < 0 || erow >= Screen::rows || ecol >= Screen::cols)
        return luaL_error(L, "lime.graphics.textBox: out of bounds [%d,%d]-[%d,%d]", row, col, erow, ecol);
    if (fill_glyph < 0 || fill_glyph >= (Screen::font)->num_glyphs)
        return luaL_error(L, "lime.graphics.textBox: invalid fill glyph index (%d)", fill_glyph);

    requireScreen(L)->textBox(row, col, nrows, ncols, border_style, fill_glyph, inverted);
    return 0;
}

int LuaHost::l_graphics_textScrollbarV(lua_State* L)
{
    int row = (int)luaL_checkinteger(L, 1);
    int col = (int)luaL_checkinteger(L, 2);
    int length = (int)luaL_checkinteger(L, 3);
    int current_scroll = (int)luaL_checkinteger(L, 4);
    int max_scroll = (int)luaL_checkinteger(L, 5);
    int visible_rows = (int)luaL_checkinteger(L, 6);

    if (length <= 0 || max_scroll <= 0 || visible_rows <= 0)
        return luaL_error(L, "lime.graphics.textScrollbarV: invalid params (length=%d, max_scroll=%d, visible_rows=%d)",
            length, max_scroll, visible_rows);

    int erow = row + length - 1;
    if (row < 0 || erow >= Screen::rows || col < 0 || col >= Screen::cols)
        return luaL_error(L, "lime.graphics.textScrollbarV: out of bounds [%d,%d]-[%d,%d]",
            row, col, erow, col);

    requireScreen(L)->scrollbarV(row, col, length, current_scroll, max_scroll, visible_rows);
    return 0;
}

int LuaHost::l_graphics_textScrollbarH(lua_State* L)
{
    int row = (int)luaL_checkinteger(L, 1);
    int col = (int)luaL_checkinteger(L, 2);
    int length = (int)luaL_checkinteger(L, 3);
    int current_scroll = (int)luaL_checkinteger(L, 4);
    int max_scroll = (int)luaL_checkinteger(L, 5);
    int visible_cols = (int)luaL_checkinteger(L, 6);

    if (length <= 0 || max_scroll <= 0 || visible_cols <= 0)
        return luaL_error(L, "lime.graphics.textScrollbarH: invalid params (length=%d, max_scroll=%d, visible_cols=%d)",
            length, max_scroll, visible_cols);

    int ecol = col + length - 1;
    if (row < 0 || row >= Screen::rows || col < 0 || ecol >= Screen::cols)
        return luaL_error(L, "lime.graphics.textScrollbarH: out of bounds [%d,%d]-[%d,%d]",
            row, col, row, ecol);

    requireScreen(L)->scrollbarH(row, col, length, current_scroll, max_scroll, visible_cols);
    return 0;
}

static std::vector<unsigned char> parseByteData(lua_State* L, int idx)
{
    if (lua_type(L, idx) == LUA_TSTRING)
    {
        size_t len = 0;
        const char* s = lua_tolstring(L, idx, &len);
        return std::vector<unsigned char>((const unsigned char*)s, (const unsigned char*)s + len);
    }

    if (lua_type(L, idx) == LUA_TTABLE)
    {
        int n = (int)lua_rawlen(L, idx);
        std::vector<unsigned char> out;
        out.reserve(n);

        for (int i = 1; i <= n; i++)
        {
            lua_rawgeti(L, idx, i);
            lua_Integer v = luaL_checkinteger(L, -1);
            lua_pop(L, 1);

            if (v < 0 || v > 255) luaL_error(L, "byte[%d] out of range (0..255)", (int)i);
            out.push_back((unsigned char)v);
        }
        return out;
    }

    luaL_error(L, "bytes must be a string or an array table of integers");
    return {};
}

int LuaHost::l_graphics_defineImage(lua_State* L)
{
    LuaHost* self = selfFromUpvalue(L);

    const char* name = luaL_checkstring(L, 1);
    int w = (int)luaL_checkinteger(L, 2);
    int h = (int)luaL_checkinteger(L, 3);

    if (w <= 0 || h <= 0) return luaL_error(L, "defineImage: invalid dimensions");
    if (w % 8) return luaL_error(L, "defineImage: width must be a multiple of 8");

    std::vector<unsigned char> bytes = parseByteData(L, 4);

    size_t expected = (size_t)(w * h / 8);
    if (bytes.size() != expected)
        return luaL_error(L, "defineImage: expected %d bytes, got %d", (int)expected, (int)bytes.size());

    auto owned = std::make_unique<OwnedImage>();
    owned->bytes = std::move(bytes);
    owned->img.width = w;
    owned->img.height = h;
    owned->img.pixels = owned->bytes.data();

    self->images[std::string(name)] = std::move(owned);
    return 0;
}

int LuaHost::l_graphics_image(lua_State* L)
{
    LuaHost* self = selfFromUpvalue(L);

    const char* name = luaL_checkstring(L, 1);
    int row = (int)luaL_checkinteger(L, 2);
    int col = (int)luaL_checkinteger(L, 3);
    bool draw_bg = lua_isnone(L, 4) ? true : (lua_toboolean(L, 4) != 0);
    int dy = (int)luaL_optinteger(L, 5, 0);

    auto it = self->images.find(name);
    if (it == self->images.end())
        return luaL_error(L, "Unknown image '%s' (did you call lime.graphics.defineImage?)", name);

    MonospaceMonochromePixelFont& font = *Screen::font;
    int x = col * font.glyph_width;
    int y = row * font.glyph_height + Screen::text_offset_y + dy;

    if (x % 8)
        return luaL_error(L, "lime.graphics.image: image x not a multiple of 8 (%d)", x);

    Image* img = &it->second->img;
    int w = img->width;
    int h = img->height;

    if (x < 0 || y < 0 || x + w > Screen::width || y + h > Screen::height)
        return luaL_error(L, "lime.graphics.image: out of bounds (%d,%d)-(%d,%d)", x, y, x + w - 1, y + h - 1);

    requireScreen(L)->image(img, row, col, draw_bg, dy);
    return 0;
}

// ============================================================================
// lime.keyboard Subtable
// ============================================================================

void LuaHost::registerKeyboardSubtable()
{
    lua_newtable(L); // lime.keyboard table

    // Function
    lua_pushcfunction(L, &LuaHost::l_keyboard_isDown);
    lua_setfield(L, -2, "isDown");
    lua_pushcfunction(L, &LuaHost::l_keyboard_ctrlIsDown);
    lua_setfield(L, -2, "ctrlIsDown");
    lua_pushcfunction(L, &LuaHost::l_keyboard_altIsDown);
    lua_setfield(L, -2, "altIsDown");
    lua_pushcfunction(L, &LuaHost::l_keyboard_shiftIsDown);
    lua_setfield(L, -2, "shiftIsDown");

    // Key constants
    lua_pushinteger(L, GLFW_KEY_LEFT_SHIFT);    lua_setfield(L, -2, "KEY_LEFT_SHIFT");
    lua_pushinteger(L, GLFW_KEY_LEFT_CONTROL);  lua_setfield(L, -2, "KEY_LEFT_CONTROL");
    lua_pushinteger(L, GLFW_KEY_LEFT_ALT);      lua_setfield(L, -2, "KEY_LEFT_ALT");
    lua_pushinteger(L, GLFW_KEY_RIGHT_SHIFT);   lua_setfield(L, -2, "KEY_RIGHT_SHIFT");
    lua_pushinteger(L, GLFW_KEY_RIGHT_CONTROL); lua_setfield(L, -2, "KEY_RIGHT_CONTROL");
    lua_pushinteger(L, GLFW_KEY_RIGHT_ALT);     lua_setfield(L, -2, "KEY_RIGHT_ALT");
    lua_pushinteger(L, GLFW_KEY_UP);            lua_setfield(L, -2, "KEY_UP");
    lua_pushinteger(L, GLFW_KEY_DOWN);          lua_setfield(L, -2, "KEY_DOWN");
    lua_pushinteger(L, GLFW_KEY_LEFT);          lua_setfield(L, -2, "KEY_LEFT");
    lua_pushinteger(L, GLFW_KEY_RIGHT);         lua_setfield(L, -2, "KEY_RIGHT");
    lua_pushinteger(L, GLFW_KEY_ENTER);         lua_setfield(L, -2, "KEY_ENTER");
    lua_pushinteger(L, GLFW_KEY_ESCAPE);        lua_setfield(L, -2, "KEY_ESCAPE");
    lua_pushinteger(L, GLFW_KEY_F11);           lua_setfield(L, -2, "KEY_F11");
    lua_pushinteger(L, GLFW_KEY_TAB);           lua_setfield(L, -2, "KEY_TAB");
    lua_pushinteger(L, GLFW_KEY_SPACE);         lua_setfield(L, -2, "KEY_SPACE");
    lua_pushinteger(L, GLFW_KEY_BACKSPACE);     lua_setfield(L, -2, "KEY_BACKSPACE");
    lua_pushinteger(L, GLFW_KEY_DELETE);        lua_setfield(L, -2, "KEY_DELETE");
    lua_pushinteger(L, GLFW_KEY_HOME);          lua_setfield(L, -2, "KEY_HOME");
    lua_pushinteger(L, GLFW_KEY_END);           lua_setfield(L, -2, "KEY_END");
    lua_pushinteger(L, GLFW_KEY_PAGE_UP);       lua_setfield(L, -2, "KEY_PAGE_UP");
    lua_pushinteger(L, GLFW_KEY_PAGE_DOWN);     lua_setfield(L, -2, "KEY_PAGE_DOWN");

    // Letter keys A-Z
    for (int c = 'A'; c <= 'Z'; ++c)
    {
        char name[8];
        sprintf_s(name, "KEY_%c", c);
        lua_pushinteger(L, c);
        lua_setfield(L, -2, name);
    }

    // Digit keys 0-9
    for (int c = '0'; c <= '9'; ++c)
    {
        char name[8];
        sprintf_s(name, "KEY_%c", c);
        lua_pushinteger(L, c);
        lua_setfield(L, -2, name);
    }

    // Function keys F1-F12
    for (int i = 1; i <= 12; ++i)
    {
        char name[8];
        sprintf_s(name, "KEY_F%d", i);
        lua_pushinteger(L, GLFW_KEY_F1 + (i - 1));
        lua_setfield(L, -2, name);
    }

    lua_setfield(L, -2, "keyboard"); // lime.keyboard = {...}
}

int LuaHost::l_keyboard_isDown(lua_State* L)
{
    int key = (int)luaL_checkinteger(L, 1);
    int state = glfwGetKey(window.window, key);
    lua_pushboolean(L, state == GLFW_PRESS);
    return 1;
}

int LuaHost::l_keyboard_ctrlIsDown(lua_State* L)
{
    bool down = glfwGetKey(window.window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
        glfwGetKey(window.window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
    lua_pushboolean(L, down);
    return 1;
}

int LuaHost::l_keyboard_altIsDown(lua_State* L)
{
    bool down = glfwGetKey(window.window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS ||
        glfwGetKey(window.window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS;
    lua_pushboolean(L, down);
    return 1;
}

int LuaHost::l_keyboard_shiftIsDown(lua_State* L)
{
    bool down = glfwGetKey(window.window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
        glfwGetKey(window.window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
    lua_pushboolean(L, down);
    return 1;
}

// ============================================================================
// lime.time Subtable
// ============================================================================

void LuaHost::registerTimeSubtable()
{
    lua_newtable(L); // lime.time table

    lua_pushcfunction(L, &LuaHost::l_time_sinceStart);
    lua_setfield(L, -2, "sinceStart");

    lua_pushcfunction(L, &LuaHost::l_time_sinceEpoch);
    lua_setfield(L, -2, "sinceEpoch");

    lua_setfield(L, -2, "time"); // lime.time = {...}
}

int LuaHost::l_time_sinceStart(lua_State* L)
{
    lua_pushnumber(L, glfwGetTime());
    return 1;
}

int LuaHost::l_time_sinceEpoch(lua_State* L)
{
    lua_pushinteger(L, (lua_Integer)time(NULL));
    return 1;
}

// ============================================================================
// lime.filesystem Subtable
// ============================================================================

fs::path LuaHost::getUserDataBasePath()
{
#ifdef _WIN32
    auto getEnvVar = [](const char* name) -> std::optional<fs::path> {
        char* value = nullptr;
        size_t len = 0;
        if (_dupenv_s(&value, &len, name) == 0 && value != nullptr) {
            fs::path result(value);
            free(value);
            return result;
        }
        return std::nullopt;
        };

    if (auto appdata = getEnvVar("APPDATA"))
        return *appdata;

    if (auto userprofile = getEnvVar("USERPROFILE"))
        return *userprofile / "AppData" / "Roaming";

    return fs::current_path();
#else
    const char* xdg = std::getenv("XDG_DATA_HOME");
    if (xdg && *xdg)
        return fs::path(xdg);
    const char* home = std::getenv("HOME");
    if (home && *home)
        return fs::path(home) / ".local" / "share";
    return fs::current_path();
#endif
}

std::string LuaHost::sanitizeIdentity(const std::string& raw)
{
    std::string result;
    result.reserve(raw.size());

    for (unsigned char c : raw)
    {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '-')
        {
            result.push_back((char)c);
        }
        else if (c == ' ' || c == '.')
        {
            if (result.empty() || result.back() != '_')
                result.push_back('_');
        }
    }

    while (!result.empty() && result.back() == '_')
        result.pop_back();

    if (result.empty())
        result = "unnamed";

    if (result.size() > 64)
        result.resize(64);

    return result;
}

void LuaHost::initSaveDir()
{
    if (appIdentity.empty())
        appIdentity = "unnamed";

    fs::path base = getUserDataBasePath();
    saveDir = base / "Lime2D" / appIdentity;

    // Directory is NOT created here - it will be created lazily
    // when a write operation (write, append, mkdir) actually needs it.
    // This prevents cluttering AppData with empty folders for apps
    // that don't use the filesystem.
}

bool LuaHost::isPathSafe(const std::string& relPath) const
{
    if (relPath.empty())
        return true;

    fs::path p(relPath);
    if (p.is_absolute())
        return false;

    if (relPath.size() >= 2 && relPath[1] == ':')
        return false;

    if (!relPath.empty() && (relPath[0] == '/' || relPath[0] == '\\'))
        return false;

    fs::path normalized = p.lexically_normal();

    auto it = normalized.begin();
    if (it != normalized.end())
    {
        std::string first = it->string();
        if (first == "..")
            return false;
    }

    for (const auto& comp : normalized)
    {
        std::string s = comp.string();
        if (s == "..")
            return false;
    }

    return true;
}

fs::path LuaHost::resolveSavePath(const std::string& relPath) const
{
    if (!isPathSafe(relPath))
        return fs::path();

    if (relPath.empty())
        return saveDir;

    return saveDir / fs::path(relPath).lexically_normal();
}

void LuaHost::registerFilesystemSubtable()
{
    lua_newtable(L); // lime.filesystem table

    lua_pushlightuserdata(L, this);
    lua_pushcclosure(L, &LuaHost::l_fs_setIdentity, 1);
    lua_setfield(L, -2, "setIdentity");

    lua_pushlightuserdata(L, this);
    lua_pushcclosure(L, &LuaHost::l_fs_getSaveDir, 1);
    lua_setfield(L, -2, "getSaveDir");

    lua_pushlightuserdata(L, this);
    lua_pushcclosure(L, &LuaHost::l_fs_read, 1);
    lua_setfield(L, -2, "read");

    lua_pushlightuserdata(L, this);
    lua_pushcclosure(L, &LuaHost::l_fs_write, 1);
    lua_setfield(L, -2, "write");

    lua_pushlightuserdata(L, this);
    lua_pushcclosure(L, &LuaHost::l_fs_append, 1);
    lua_setfield(L, -2, "append");

    lua_pushlightuserdata(L, this);
    lua_pushcclosure(L, &LuaHost::l_fs_exists, 1);
    lua_setfield(L, -2, "exists");

    lua_pushlightuserdata(L, this);
    lua_pushcclosure(L, &LuaHost::l_fs_isFile, 1);
    lua_setfield(L, -2, "isFile");

    lua_pushlightuserdata(L, this);
    lua_pushcclosure(L, &LuaHost::l_fs_isDirectory, 1);
    lua_setfield(L, -2, "isDirectory");

    lua_pushlightuserdata(L, this);
    lua_pushcclosure(L, &LuaHost::l_fs_remove, 1);
    lua_setfield(L, -2, "remove");

    lua_pushlightuserdata(L, this);
    lua_pushcclosure(L, &LuaHost::l_fs_mkdir, 1);
    lua_setfield(L, -2, "mkdir");

    lua_pushlightuserdata(L, this);
    lua_pushcclosure(L, &LuaHost::l_fs_list, 1);
    lua_setfield(L, -2, "list");

    lua_pushcfunction(L, &LuaHost::l_fs_pathJoin);
    lua_setfield(L, -2, "pathJoin");

    lua_setfield(L, -2, "filesystem"); // lime.filesystem = {...}
}

int LuaHost::l_fs_setIdentity(lua_State* L)
{
    LuaHost* self = selfFromUpvalue(L);
    const char* rawId = luaL_checkstring(L, 1);

    if (self->identityLocked)
        return luaL_error(L, "lime.filesystem.setIdentity: can only be called once");

    if (self->filesystemAccessed)
        return luaL_error(L, "lime.filesystem.setIdentity: must be called before any filesystem operations");

    std::string sanitized = sanitizeIdentity(rawId ? rawId : "");
    self->appIdentity = sanitized;
    self->initSaveDir();
    self->identityLocked = true;

    return 0;
}

int LuaHost::l_fs_getSaveDir(lua_State* L)
{
    LuaHost* self = selfFromUpvalue(L);

    if (self->saveDir.empty())
        self->initSaveDir();

    std::string s = pathToUtf8(self->saveDir);
    lua_pushlstring(L, s.c_str(), s.size());
    return 1;
}

int LuaHost::l_fs_read(lua_State* L)
{
    LuaHost* self = selfFromUpvalue(L);
    const char* relPath = luaL_checkstring(L, 1);

    if (self->saveDir.empty())
        self->initSaveDir();

    self->filesystemAccessed = true;

    if (!self->isPathSafe(relPath))
    {
        lua_pushnil(L);
        lua_pushliteral(L, "invalid path (outside sandbox)");
        return 2;
    }

    fs::path fullPath = self->resolveSavePath(relPath);
    if (fullPath.empty())
    {
        lua_pushnil(L);
        lua_pushliteral(L, "invalid path");
        return 2;
    }

    std::error_code ec;
    if (!fs::exists(fullPath, ec) || ec)
    {
        lua_pushnil(L);
        lua_pushliteral(L, "file does not exist");
        return 2;
    }

    if (!fs::is_regular_file(fullPath, ec) || ec)
    {
        lua_pushnil(L);
        lua_pushliteral(L, "path is not a file");
        return 2;
    }

    std::string content;
    if (!readWholeFile(fullPath, content))
    {
        lua_pushnil(L);
        lua_pushliteral(L, "failed to read file");
        return 2;
    }

    lua_pushlstring(L, content.data(), content.size());
    return 1;
}

int LuaHost::l_fs_write(lua_State* L)
{
    LuaHost* self = selfFromUpvalue(L);
    const char* relPath = luaL_checkstring(L, 1);
    size_t dataLen = 0;
    const char* data = luaL_checklstring(L, 2, &dataLen);

    if (self->saveDir.empty())
        self->initSaveDir();

    self->filesystemAccessed = true;

    if (!self->isPathSafe(relPath))
    {
        lua_pushboolean(L, false);
        lua_pushliteral(L, "invalid path (outside sandbox)");
        return 2;
    }

    fs::path fullPath = self->resolveSavePath(relPath);
    if (fullPath.empty())
    {
        lua_pushboolean(L, false);
        lua_pushliteral(L, "invalid path");
        return 2;
    }

    std::error_code ec;
    fs::path parentDir = fullPath.parent_path();
    if (!parentDir.empty() && !fs::exists(parentDir, ec))
    {
        fs::create_directories(parentDir, ec);
        if (ec)
        {
            lua_pushboolean(L, false);
            lua_pushstring(L, ("failed to create directory: " + ec.message()).c_str());
            return 2;
        }
    }

    std::ofstream f(fullPath, std::ios::binary | std::ios::trunc);
    if (!f)
    {
        lua_pushboolean(L, false);
        lua_pushliteral(L, "failed to open file for writing");
        return 2;
    }

    if (dataLen > 0)
        f.write(data, (std::streamsize)dataLen);

    f.close();

    if (!f)
    {
        lua_pushboolean(L, false);
        lua_pushliteral(L, "write error");
        return 2;
    }

    lua_pushboolean(L, true);
    return 1;
}

int LuaHost::l_fs_append(lua_State* L)
{
    LuaHost* self = selfFromUpvalue(L);
    const char* relPath = luaL_checkstring(L, 1);
    size_t dataLen = 0;
    const char* data = luaL_checklstring(L, 2, &dataLen);

    if (self->saveDir.empty())
        self->initSaveDir();

    self->filesystemAccessed = true;

    if (!self->isPathSafe(relPath))
    {
        lua_pushboolean(L, false);
        lua_pushliteral(L, "invalid path (outside sandbox)");
        return 2;
    }

    fs::path fullPath = self->resolveSavePath(relPath);
    if (fullPath.empty())
    {
        lua_pushboolean(L, false);
        lua_pushliteral(L, "invalid path");
        return 2;
    }

    std::error_code ec;
    fs::path parentDir = fullPath.parent_path();
    if (!parentDir.empty() && !fs::exists(parentDir, ec))
    {
        fs::create_directories(parentDir, ec);
        if (ec)
        {
            lua_pushboolean(L, false);
            lua_pushstring(L, ("failed to create directory: " + ec.message()).c_str());
            return 2;
        }
    }

    std::ofstream f(fullPath, std::ios::binary | std::ios::app);
    if (!f)
    {
        lua_pushboolean(L, false);
        lua_pushliteral(L, "failed to open file for appending");
        return 2;
    }

    if (dataLen > 0)
        f.write(data, (std::streamsize)dataLen);

    f.close();

    if (!f)
    {
        lua_pushboolean(L, false);
        lua_pushliteral(L, "write error");
        return 2;
    }

    lua_pushboolean(L, true);
    return 1;
}

int LuaHost::l_fs_exists(lua_State* L)
{
    LuaHost* self = selfFromUpvalue(L);
    const char* relPath = luaL_checkstring(L, 1);

    if (self->saveDir.empty())
        self->initSaveDir();

    self->filesystemAccessed = true;

    if (!self->isPathSafe(relPath))
    {
        lua_pushboolean(L, false);
        return 1;
    }

    fs::path fullPath = self->resolveSavePath(relPath);
    if (fullPath.empty())
    {
        lua_pushboolean(L, false);
        return 1;
    }

    std::error_code ec;
    bool exists = fs::exists(fullPath, ec);
    lua_pushboolean(L, exists && !ec);
    return 1;
}

int LuaHost::l_fs_isFile(lua_State* L)
{
    LuaHost* self = selfFromUpvalue(L);
    const char* relPath = luaL_checkstring(L, 1);

    if (self->saveDir.empty())
        self->initSaveDir();

    self->filesystemAccessed = true;

    if (!self->isPathSafe(relPath))
    {
        lua_pushboolean(L, false);
        return 1;
    }

    fs::path fullPath = self->resolveSavePath(relPath);
    if (fullPath.empty())
    {
        lua_pushboolean(L, false);
        return 1;
    }

    std::error_code ec;
    bool isFile = fs::is_regular_file(fullPath, ec);
    lua_pushboolean(L, isFile && !ec);
    return 1;
}

int LuaHost::l_fs_isDirectory(lua_State* L)
{
    LuaHost* self = selfFromUpvalue(L);
    const char* relPath = luaL_checkstring(L, 1);

    if (self->saveDir.empty())
        self->initSaveDir();

    self->filesystemAccessed = true;

    if (!self->isPathSafe(relPath))
    {
        lua_pushboolean(L, false);
        return 1;
    }

    fs::path fullPath = self->resolveSavePath(relPath);
    if (fullPath.empty())
    {
        lua_pushboolean(L, false);
        return 1;
    }

    std::error_code ec;
    bool isDir = fs::is_directory(fullPath, ec);
    lua_pushboolean(L, isDir && !ec);
    return 1;
}

int LuaHost::l_fs_remove(lua_State* L)
{
    LuaHost* self = selfFromUpvalue(L);
    const char* relPath = luaL_checkstring(L, 1);

    if (self->saveDir.empty())
        self->initSaveDir();

    self->filesystemAccessed = true;

    if (!self->isPathSafe(relPath))
    {
        lua_pushboolean(L, false);
        lua_pushliteral(L, "invalid path (outside sandbox)");
        return 2;
    }

    if (!relPath || !*relPath)
    {
        lua_pushboolean(L, false);
        lua_pushliteral(L, "cannot remove save directory root");
        return 2;
    }

    fs::path fullPath = self->resolveSavePath(relPath);
    if (fullPath.empty())
    {
        lua_pushboolean(L, false);
        lua_pushliteral(L, "invalid path");
        return 2;
    }

    std::error_code ec;
    if (!fs::exists(fullPath, ec))
    {
        lua_pushboolean(L, false);
        lua_pushliteral(L, "path does not exist");
        return 2;
    }

    if (fs::is_directory(fullPath, ec) && !ec)
    {
        auto it = fs::directory_iterator(fullPath, ec);
        if (!ec && it != fs::directory_iterator())
        {
            lua_pushboolean(L, false);
            lua_pushliteral(L, "directory is not empty");
            return 2;
        }
    }

    bool removed = fs::remove(fullPath, ec);
    if (ec || !removed)
    {
        lua_pushboolean(L, false);
        lua_pushstring(L, ("failed to remove: " + ec.message()).c_str());
        return 2;
    }

    lua_pushboolean(L, true);
    return 1;
}

int LuaHost::l_fs_mkdir(lua_State* L)
{
    LuaHost* self = selfFromUpvalue(L);
    const char* relPath = luaL_checkstring(L, 1);

    if (self->saveDir.empty())
        self->initSaveDir();

    self->filesystemAccessed = true;

    if (!self->isPathSafe(relPath))
    {
        lua_pushboolean(L, false);
        lua_pushliteral(L, "invalid path (outside sandbox)");
        return 2;
    }

    fs::path fullPath = self->resolveSavePath(relPath);
    if (fullPath.empty())
    {
        lua_pushboolean(L, false);
        lua_pushliteral(L, "invalid path");
        return 2;
    }

    std::error_code ec;
    fs::create_directories(fullPath, ec);
    if (ec)
    {
        lua_pushboolean(L, false);
        lua_pushstring(L, ("failed to create directory: " + ec.message()).c_str());
        return 2;
    }

    lua_pushboolean(L, true);
    return 1;
}

int LuaHost::l_fs_list(lua_State* L)
{
    LuaHost* self = selfFromUpvalue(L);
    const char* relPath = luaL_optstring(L, 1, "");

    if (self->saveDir.empty())
        self->initSaveDir();

    self->filesystemAccessed = true;

    if (!self->isPathSafe(relPath))
    {
        lua_pushnil(L);
        lua_pushliteral(L, "invalid path (outside sandbox)");
        return 2;
    }

    fs::path fullPath = self->resolveSavePath(relPath);
    if (fullPath.empty())
    {
        lua_pushnil(L);
        lua_pushliteral(L, "invalid path");
        return 2;
    }

    std::error_code ec;
    if (!fs::exists(fullPath, ec) || ec)
    {
        lua_pushnil(L);
        lua_pushliteral(L, "path does not exist");
        return 2;
    }

    if (!fs::is_directory(fullPath, ec) || ec)
    {
        lua_pushnil(L);
        lua_pushliteral(L, "path is not a directory");
        return 2;
    }

    lua_newtable(L);
    int index = 1;

    for (auto& entry : fs::directory_iterator(fullPath, ec))
    {
        if (ec) break;

        std::string name = entry.path().filename().string();
        const char* type = "unknown";

        std::error_code ec2;
        if (entry.is_regular_file(ec2) && !ec2)
            type = "file";
        else if (entry.is_directory(ec2) && !ec2)
            type = "directory";

        lua_createtable(L, 2, 0);
        lua_pushstring(L, name.c_str());
        lua_rawseti(L, -2, 1);
        lua_pushstring(L, type);
        lua_rawseti(L, -2, 2);

        lua_rawseti(L, -2, index++);
    }

    return 1;
}

// ============================================================================
// lime.profiler Subtable
// ============================================================================

void LuaHost::profilerStopCurrentSection()
{
    if (profilerActiveSection.empty())
        return;

    double now = glfwGetTime();
    double elapsed = now - profilerSectionStart;

    // Add elapsed time to the section's accumulator
    profilerSections[profilerActiveSection] += elapsed;

    profilerActiveSection.clear();
    profilerSectionStart = 0.0;
}

void LuaHost::registerProfilerSubtable()
{
    lua_newtable(L); // lime.profiler table

    lua_pushlightuserdata(L, this);
    lua_pushcclosure(L, &LuaHost::l_profiler_start, 1);
    lua_setfield(L, -2, "start");

    lua_pushlightuserdata(L, this);
    lua_pushcclosure(L, &LuaHost::l_profiler_stop, 1);
    lua_setfield(L, -2, "stop");

    lua_pushlightuserdata(L, this);
    lua_pushcclosure(L, &LuaHost::l_profiler_list, 1);
    lua_setfield(L, -2, "list");

    lua_pushlightuserdata(L, this);
    lua_pushcclosure(L, &LuaHost::l_profiler_get, 1);
    lua_setfield(L, -2, "get");

    lua_pushlightuserdata(L, this);
    lua_pushcclosure(L, &LuaHost::l_profiler_reset, 1);
    lua_setfield(L, -2, "reset");

    lua_pushlightuserdata(L, this);
    lua_pushcclosure(L, &LuaHost::l_profiler_clear, 1);
    lua_setfield(L, -2, "clear");

    lua_setfield(L, -2, "profiler"); // lime.profiler = {...}
}

int LuaHost::l_profiler_start(lua_State* L)
{
    if (lua_isnoneornil(L, 1))
        APP_FATAL << "Identifier empty or nil!";

    LuaHost* self = selfFromUpvalue(L);

    // Stop current section first (accumulate its time)
    self->profilerStopCurrentSection();

    /*// If nil or empty string is passed, just stop (no new section)
    if (lua_isnoneornil(L, 1))
        return 0;*/

    const char* id = luaL_checkstring(L, 1);
    if (!id || !*id)
        return 0;

    std::string sectionId(id);

    // Ensure the section exists in the map (initialize to 0 if new)
    if (self->profilerSections.find(sectionId) == self->profilerSections.end())
        self->profilerSections[sectionId] = 0.0;

    // Start timing the new section
    self->profilerActiveSection = sectionId;
    self->profilerSectionStart = glfwGetTime();

    return 0;
}

int LuaHost::l_profiler_stop(lua_State* L)
{
    LuaHost* self = selfFromUpvalue(L);
    self->profilerStopCurrentSection();
    return 0;
}

int LuaHost::l_profiler_list(lua_State* L)
{
    LuaHost* self = selfFromUpvalue(L);

    lua_newtable(L);
    int index = 1;

    for (const auto& pair : self->profilerSections)
    {
        lua_pushlstring(L, pair.first.c_str(), pair.first.size());
        lua_rawseti(L, -2, index++);
    }

    return 1;
}

int LuaHost::l_profiler_get(lua_State* L)
{
    LuaHost* self = selfFromUpvalue(L);

    const char* id = luaL_checkstring(L, 1);
    std::string sectionId(id ? id : "");

    auto it = self->profilerSections.find(sectionId);
    if (it == self->profilerSections.end())
    {
        // Section doesn't exist, return 0
        lua_pushnumber(L, 0.0);
        return 1;
    }

    double accumulated = it->second;

    // If this section is currently active, add the in-progress time
    if (self->profilerActiveSection == sectionId)
    {
        double now = glfwGetTime();
        accumulated += (now - self->profilerSectionStart);
    }

    lua_pushnumber(L, accumulated);
    return 1;
}

int LuaHost::l_profiler_reset(lua_State* L)
{
    LuaHost* self = selfFromUpvalue(L);

    // Reset all accumulators to 0
    for (auto& pair : self->profilerSections)
        pair.second = 0.0;

    // If there's an active section, reset its start time to now
    if (!self->profilerActiveSection.empty())
        self->profilerSectionStart = glfwGetTime();

    return 0;
}

int LuaHost::l_profiler_clear(lua_State* L)
{
    LuaHost* self = selfFromUpvalue(L);

    self->profilerSections.clear();
    self->profilerActiveSection.clear();
    self->profilerSectionStart = 0.0;

    return 0;
}

// ============================================================================
// Top-level lime Functions
// ============================================================================

int LuaHost::l_exeDir(lua_State* L)
{
    LuaHost* self = selfFromUpvalue(L);
    std::string s = pathToUtf8(self->exeDir);
    lua_pushlstring(L, s.c_str(), s.size());
    return 1;
}

int LuaHost::l_cwd(lua_State* L)
{
    std::error_code ec;
    fs::path p = fs::current_path(ec);
    if (ec) return luaL_error(L, "lime.cwd: %s", ec.message().c_str());
    std::string s = pathToUtf8(p);
    lua_pushlstring(L, s.c_str(), s.size());
    return 1;
}

int LuaHost::l_fs_pathJoin(lua_State* L)
{
    int n = lua_gettop(L);
    if (n < 2) return luaL_error(L, "lime.pathJoin: expected at least 2 arguments");

    fs::path out = luaUtf8ToPath(L, 1);
    for (int i = 2; i <= n; ++i)
        out /= luaUtf8ToPath(L, i);

    out = out.lexically_normal();

    std::string s = pathToUtf8(out);
    lua_pushlstring(L, s.c_str(), s.size());
    return 1;
}

int LuaHost::l_scriptDir(lua_State* L)
{
    LuaHost* self = selfFromUpvalue(L);
    std::string s = pathToUtf8(self->mainScriptDir);
    lua_pushlstring(L, s.c_str(), s.size());
    return 1;
}

int LuaHost::l_require(lua_State* L)
{
    LuaHost* self = selfFromUpvalue(L);
    const char* mod = luaL_checkstring(L, 1);
    std::string modStr = mod ? mod : "";

    lua_getfield(L, LUA_REGISTRYINDEX, "LIME_REQUIRE_CACHE");
    if (lua_isnil(L, -1))
    {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, LUA_REGISTRYINDEX, "LIME_REQUIRE_CACHE");
    }
    int cacheIndex = lua_gettop(L);

    lua_getfield(L, cacheIndex, modStr.c_str());
    if (!lua_isnil(L, -1))
    {
        lua_remove(L, cacheIndex);
        return 1;
    }
    lua_pop(L, 1);

    // ---- Determine relative path for the module ----

    fs::path rel;
    fs::path modPath(modStr);
    if (modPath.is_absolute())
    {
        rel = modPath;
    }
    else
    {
        bool hasSlash = (modStr.find('/') != std::string::npos) || (modStr.find('\\') != std::string::npos);
        bool hasLuaExt = (modStr.size() >= 4 && modStr.substr(modStr.size() - 4) == ".lua");
        if (hasSlash || hasLuaExt)
        {
            rel = fs::path(modStr);
        }
        else
        {
            std::string s = modStr;
            for (char& ch : s) if (ch == '.') ch = '/';
            rel = fs::path(s + ".lua");
        }
    }

    std::string chunk;
    std::string chunkname;

    // ---- Try fused archive first ----

    if (FusedArchive::isFused() && !modPath.is_absolute())
    {
        std::string relStr = rel.generic_string();

        // Try: fusedBaseDir + relative path (handles "../" correctly via lexically_normal)
        if (!self->fusedBaseDir.empty())
        {
            std::string fullPath = fs::path(self->fusedBaseDir + relStr)
                .lexically_normal().generic_string();
            if (FusedArchive::readFile(fullPath, chunk))
                chunkname = "@" + fullPath;
        }

        // Try: just the relative path at the archive root
        if (chunk.empty())
        {
            std::string normalizedRel = fs::path(relStr)
                .lexically_normal().generic_string();
            if (FusedArchive::readFile(normalizedRel, chunk))
                chunkname = "@" + normalizedRel;
        }
    }

    // ---- Fall back to disk ----

    if (chunk.empty())
    {
        fs::path candidates[2] = {
            rel.is_absolute() ? rel : (self->mainScriptDir / rel),
            rel.is_absolute() ? rel : (fs::current_path() / rel)
        };

        fs::path found;
        for (auto& c : candidates)
        {
            std::error_code ec;
            if (fs::exists(c, ec) && fs::is_regular_file(c, ec))
            {
                found = c;
                break;
            }
        }

        if (found.empty())
        {
            lua_pop(L, 1);
            return luaL_error(L, "lime.require: module not found: %s", modStr.c_str());
        }

        if (!readWholeFile(found, chunk))
        {
            lua_pop(L, 1);
            return luaL_error(L, "lime.require: failed to read: %s", found.string().c_str());
        }

        chunkname = "@" + pathToUtf8(found);
    }

    // ---- Load and execute ----

    int status = luaL_loadbuffer(L, chunk.data(), chunk.size(), chunkname.c_str());
    if (status != LUA_OK)
    {
        lua_remove(L, cacheIndex);
        return lua_error(L);
    }

    lua_pushcfunction(L, &LuaHost::traceback);
    lua_insert(L, cacheIndex + 1);
    int errIndex = cacheIndex + 1;

    status = lua_pcall(L, 0, 1, errIndex);
    lua_remove(L, errIndex);

    if (status != LUA_OK)
    {
        lua_remove(L, cacheIndex);
        return lua_error(L);
    }

    if (lua_isnil(L, -1))
    {
        lua_pop(L, 1);
        lua_pushboolean(L, 1);
    }

    lua_pushvalue(L, -1);
    lua_setfield(L, cacheIndex, modStr.c_str());

    lua_remove(L, cacheIndex);
    return 1;
}

int LuaHost::l_print(lua_State* L)
{
    int n = lua_gettop(L); // Number of arguments
    for (int i = 1; i <= n; i++)
    {
        size_t l;
        // luaL_tolstring converts any type to a string and leaves it on the stack
        const char* s = lime_tolstring(L, i, &l);
        if (i > 1) std::cout << "\t";
        std::cout << s;
        lua_pop(L, 1); // Pop the string created by luaL_tolstring
    }
    std::cout << std::endl; // This triggers CapturingStreambuf
    return 0;
}

// ============================================================================
// Script Loading and App Callbacks
// ============================================================================

void LuaHost::loadAppScript(const char* path)
{
    loadAppScript(fs::path(path));
}

void LuaHost::loadAppScript(const std::filesystem::path& path)
{
    if (!L) throw std::runtime_error("LuaHost not initialized");

    images.clear();

    // Clear profiler state when loading a new script
    profilerSections.clear();
    profilerActiveSection.clear();
    profilerSectionStart = 0.0;

    // Reset filesystem identity tracking
    identityLocked = false;
    filesystemAccessed = false;

    quitCallbackActive = false;

    mainScriptDir = fs::absolute(path).parent_path();

    appIdentity = sanitizeIdentity(mainScriptDir.filename().string());
    initSaveDir();

    std::string chunk;
    if (!readWholeFile(path, chunk))
        throw std::runtime_error(std::string("Failed to open main script: ") + path.string());

    std::string chunkname = "@" + pathToUtf8(path);
    int status = luaL_loadbuffer(L, chunk.data(), chunk.size(), chunkname.c_str());

    if (status != LUA_OK)
    {
        std::string err = lua_tostring(L, -1) ? lua_tostring(L, -1) : "luaL_loadfile failed";
        lua_pop(L, 1);
        throw std::runtime_error(err);
    }

    pcall(0, 0);
}

void LuaHost::loadFusedScript(const std::string& archivePath)
{
    if (!L) throw std::runtime_error("LuaHost not initialized");

    images.clear();

    profilerSections.clear();
    profilerActiveSection.clear();
    profilerSectionStart = 0.0;

    identityLocked = false;
    filesystemAccessed = false;
    quitCallbackActive = false;

    // In fused mode, the "script directory" is the EXE directory
    mainScriptDir = exeDir;

    // Determine the base directory within the archive for relative requires.
    // e.g., if the main script is "myapp/main.lua", fusedBaseDir = "myapp/"
    std::string normalized = archivePath;
    for (char& c : normalized)
        if (c == '\\') c = '/';
    size_t lastSlash = normalized.find_last_of('/');
    if (lastSlash != std::string::npos)
        fusedBaseDir = normalized.substr(0, lastSlash + 1);
    else
        fusedBaseDir.clear();

    // Default identity from EXE directory name (user can override via setIdentity)
    appIdentity = sanitizeIdentity(mainScriptDir.filename().string());
    initSaveDir();

    // Read the main script from the archive
    std::string chunk;
    if (!FusedArchive::readFile(archivePath, chunk))
        throw std::runtime_error("Failed to read fused script: " + archivePath);

    std::string chunkname = "@" + archivePath;
    int status = luaL_loadbuffer(L, chunk.data(), chunk.size(), chunkname.c_str());

    if (status != LUA_OK)
    {
        std::string err = lua_tostring(L, -1) ? lua_tostring(L, -1) : "luaL_loadbuffer failed";
        lua_pop(L, 1);
        throw std::runtime_error(err);
    }

    pcall(0, 0);
}

void LuaHost::setArgv(const std::vector<std::filesystem::path>& files)
{
    argvFiles = files;
    if (!L) return;

    lua_getglobal(L, "lime");
    if (!lua_istable(L, -1))
    {
        lua_pop(L, 1);
        return;
    }

    lua_newtable(L);
    int idx = 1;
    for (const auto& p : argvFiles)
    {
        std::string s = pathToUtf8(p);
        lua_pushlstring(L, s.c_str(), s.size());
        lua_rawseti(L, -2, idx++);
    }
    lua_setfield(L, -2, "argv");
    lua_pop(L, 1);
}

void LuaHost::setExeDir(const std::filesystem::path& dir)
{
    exeDir = dir.empty() ? std::filesystem::path{} : std::filesystem::absolute(dir);
}

bool LuaHost::pushLimeCallback(const char* name)
{
    lua_getglobal(L, "lime");
    if (!lua_istable(L, -1))
    {
        lua_pop(L, 1);
        return false;
    }

    lua_getfield(L, -1, name);
    lua_remove(L, -2); // remove lime table, keep only the function (or nil)

    if (!lua_isfunction(L, -1))
    {
        lua_pop(L, 1);
        return false;
    }

    return true;
}

void LuaHost::callOnSetActive(bool initial)
{
    if (!initial) return;
    if (!pushLimeCallback("init")) return;
    pcall(0, 0);
}

void LuaHost::callUpdate(float dt)
{
    if (!pushLimeCallback("update")) return;
    lua_pushnumber(L, (lua_Number)dt);
    pcall(1, 0);
}

void LuaHost::callDraw()
{
    if (!pushLimeCallback("draw")) return;
    pcall(0, 0);
}

bool LuaHost::callKeyPressed(int key, int scancode, bool isrepeat)
{
    if (!pushLimeCallback("keypressed")) return false;

    lua_pushinteger(L, key);
    lua_pushinteger(L, scancode);
    lua_pushboolean(L, isrepeat);

    pcall(3, 1);

    bool handled = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    return handled;
}

bool LuaHost::callKeyReleased(int key, int scancode)
{
    if (!pushLimeCallback("keyreleased")) return false;

    lua_pushinteger(L, key);
    lua_pushinteger(L, scancode);

    pcall(2, 1);

    bool handled = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    return handled;
}

static void codepointToUtf8(unsigned int c, char* out)
{
    if (c < 0x80) {
        out[0] = (char)c;
        out[1] = 0;
    }
    else if (c < 0x800) {
        out[0] = (char)(0xC0 | (c >> 6));
        out[1] = (char)(0x80 | (c & 0x3F));
        out[2] = 0;
    }
    else if (c < 0x10000) {
        out[0] = (char)(0xE0 | (c >> 12));
        out[1] = (char)(0x80 | ((c >> 6) & 0x3F));
        out[2] = (char)(0x80 | (c & 0x3F));
        out[3] = 0;
    }
    else {
        out[0] = (char)(0xF0 | (c >> 18));
        out[1] = (char)(0x80 | ((c >> 12) & 0x3F));
        out[2] = (char)(0x80 | ((c >> 6) & 0x3F));
        out[3] = (char)(0x80 | (c & 0x3F));
        out[4] = 0;
    }
}

bool LuaHost::callTextInput(unsigned int c)
{
    char utf8[5] = { 0 };
    codepointToUtf8(c, utf8);

    if (!pushLimeCallback("textinput")) return false;

    lua_pushstring(L, utf8);

    pcall(1, 1);

    bool handled = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    return handled;
}

bool LuaHost::callQuit()
{
    if (!L) return false;
    if (!pushLimeCallback("quit")) return false;

    pcall(0, 1);

    bool abort = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    return abort;
}

bool LuaHost::invokeQuitCallback()
{
    if (!L || quitCallbackActive) return false;

    quitCallbackActive = true;
    bool abort = false;
    try
    {
        abort = callQuit();
    }
    catch (...)
    {
        quitCallbackActive = false;
        throw;
    }
    quitCallbackActive = false;
    return abort;
}