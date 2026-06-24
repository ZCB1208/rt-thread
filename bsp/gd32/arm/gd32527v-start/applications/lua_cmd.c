/*
 * lua_cmd.c — Lua 脚本运行的 msh 命令
 *
 * 用法:
 *   msh /> lua_list          — 列出所有嵌入的 Lua 脚本
 *   msh /> lua_run led_blink — 运行 led_blink.lua
 */

#include <rtthread.h>
#include <rtdevice.h>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include "lua_script.h"

/* 包含自动生成的脚本注册表 */
#include "lua_script_table.inc"

int lua_run_script(const char *name)
{
    const lua_script_t *entry = lua_script_table;

    /* 按名称查找脚本 */
    while (entry->name != NULL)
    {
        if (rt_strcmp(entry->name, name) == 0)
            break;
        entry++;
    }

    if (entry->name == NULL)
    {
        rt_kprintf("Script '%s' not found. Use 'lua_list' to list all.\n", name);
        return -RT_ERROR;
    }

    /* 创建 Lua 状态 */
    lua_State *L = luaL_newstate();
    if (L == NULL)
    {
        rt_kprintf("Error: cannot create Lua state\n");
        return -RT_ENOMEM;
    }

    /* 打开标准库（会自动加载 rtext 扩展） */
    luaL_openlibs(L);

    /* 执行脚本 */
    int ret = luaL_dostring(L, entry->source);
    if (ret != LUA_OK)
    {
        rt_kprintf("Lua error: %s\n", lua_tostring(L, -1));
    }

    lua_close(L);
    return ret == LUA_OK ? RT_EOK : -RT_ERROR;
}

void lua_list_scripts(void)
{
    const lua_script_t *entry = lua_script_table;

    rt_kprintf("Available Lua scripts:\n");
    rt_kprintf("---------------------\n");

    if (entry->name == NULL)
    {
        rt_kprintf("  (none)\n");
        return;
    }

    while (entry->name != NULL)
    {
        rt_kprintf("  %s\n", entry->name);
        entry++;
    }
}

/* msh: lua_run <script_name> */
static void lua_run_cmd(int argc, char **argv)
{
    if (argc < 2)
    {
        rt_kprintf("Usage: lua_run <script_name>\n");
        rt_kprintf("       lua_list\n");
        return;
    }

    lua_run_script(argv[1]);
}
MSH_CMD_EXPORT_ALIAS(lua_run_cmd, lua_run, Run an embedded Lua script. e.g. lua_run led_blink);

/* msh: lua_list */
static void lua_list_cmd(int argc, char **argv)
{
    lua_list_scripts();
}
MSH_CMD_EXPORT_ALIAS(lua_list_cmd, lua_list, List all embedded Lua scripts.);