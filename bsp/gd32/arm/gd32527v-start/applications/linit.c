/*
 * linit.c — Lua 标准库初始化（定制版）
 *
 * 基于 Lua 5.3.4 官方 linit.c，增加了 rtext (RT-Thread Extension) 库。
 * 替换 packages/Lua-latest/lua-5.3.4/linit.c 使用。
 *
 * "If you embed Lua in your program and need to open the standard
 *  libraries, call luaL_openlibs in your program. If you need a
 *  different set of libraries, copy this file to your project and edit
 *  it to suit your needs."
 *                                    — Lua 5.3 Reference Manual
 */

#define linit_c
#define LUA_LIB

#include <stddef.h>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

/* 声明 RT-Thread 扩展库的打开函数 */
extern int luaopen_rtext(lua_State *L);

static const luaL_Reg loadedlibs[] =
{
    {"_G", luaopen_base},
    {LUA_LOADLIBNAME, luaopen_package},
    {LUA_COLIBNAME, luaopen_coroutine},
    {LUA_TABLIBNAME, luaopen_table},
    {LUA_IOLIBNAME, luaopen_io},
    {LUA_OSLIBNAME, luaopen_os},
    {LUA_STRLIBNAME, luaopen_string},
    {LUA_MATHLIBNAME, luaopen_math},
    {LUA_UTF8LIBNAME, luaopen_utf8},
    {LUA_DBLIBNAME, luaopen_debug},
#if defined(LUA_COMPAT_BITLIB)
    {LUA_BITLIBNAME, luaopen_bit32},
#endif
    /* RT-Thread 扩展库：gpio_mode, gpio_write, gpio_read, rtos_delay */
    {"rtext", luaopen_rtext},
    {NULL, NULL}
};

LUALIB_API void luaL_openlibs(lua_State *L)
{
    const luaL_Reg *lib;
    for (lib = loadedlibs; lib->func; lib++)
    {
        luaL_requiref(L, lib->name, lib->func, 1);
        lua_pop(L, 1);
    }
}
