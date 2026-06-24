/*
 * Copyright (c) 2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * lua_script.h — Lua 脚本注册表类型定义
 */

#ifndef __LUA_SCRIPT_H__
#define __LUA_SCRIPT_H__

#ifdef __cplusplus
extern "C" {
#endif

/* 脚本注册表条目 */
typedef struct
{
    const char *name;       /* 脚本名称（不带 .lua 后缀） */
    const char *source;     /* Lua 源码字符串 */
} lua_script_t;

/* 脚本注册表（在 lua_script_table.inc 中定义） */
extern const lua_script_t lua_script_table[];
extern const char *lua_script_names[];

/* 运行指定脚本 */
int lua_run_script(const char *name);

/* 列出所有可用脚本 */
void lua_list_scripts(void);

#ifdef __cplusplus
}
#endif

#endif /* __LUA_SCRIPT_H__ */