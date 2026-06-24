# 在 GD32F527V-START 上移植 Lua 脚本引擎

> 让 RT-Thread 跑上 Lua：在 GD32F527V-START 开发板上集成 Lua 5.3.4 脚本引擎的全过程。

---

## 一、背景

GD32F527V-START 是基于兆易创新 GD32F527VST7（Cortex-M33 @200MHz）的官方评估板。我们在上一篇文章中已经完成了 RT-Thread 的基础移植，实现了 FinSH 控制台和 GPIO 驱动。现在我们要为它加上 Lua 脚本引擎支持。

为什么要在嵌入式设备上用 Lua？

- **热更新能力**：脚本可以独立于固件更新
- **快速原型**：无需编译，直接在设备上测试逻辑
- **封装复杂度**：业务逻辑用脚本描述，驱动层用 C 实现
- **社区生态**：Lua 在嵌入式领域有丰富的资源和成熟的实践

---

## 二、环境准备

### 硬件

- GD32F527V-START 开发板 × 1
- USB Type-C 数据线 × 1（供电 + 调试 + 串口）

### 软件

- IAR Embedded Workbench for ARM
- RT-Thread 5.3.0（已移植好 GD32F527V-START BSP）
- Lua 5.3.4（通过 RT-Thread 包管理器）

---

## 三、添加 Lua 软件包

### 3.1 通过 env 工具添加 Lua 包

```bash
# 进入 BSP 目录
cd rt-thread/bsp/gd32/arm/gd32527v-start

# 打开 menuconfig 配置
menuconfig
```

在配置菜单中：

```
RT-Thread online packages
  └── language packages
       └── Lua: Powerful light-weight programming language
            ├── [*] Lua
            ├── Version (latest)
            └── [*] Lua 5.3.4
```

保存退出后：

```bash
pkgs --update
```

这会从远程仓库拉取 Lua 5.3.4 源码到 `packages/Lua-latest/` 目录。

### 3.2 新增的配置项

`rtconfig.h` 中会增加以下配置：

```c
#define PKG_USING_LUA
#define PKG_USING_LUA_LATEST_VERSION
#define LUA_USING_PORTING_V534
```

同时需要开启 POSIX 文件系统支持（Lua 的 `io` 库依赖）：

```c
#define RT_USING_POSIX_FS
```

---

## 四、适配 RT-Thread v5.x 调度器 API 变更

Lua 包中的 `lua2rtt.c` 使用了旧版 RT-Thread 的 API，直接访问了 `rt_thread` 结构体的 `current_priority` 字段。在 RT-Thread v5.x 中，调度器上下文被重构了：

```
RT-Thread v4.x              →  RT-Thread v5.x
thread->current_priority    →  RT_SCHED_PRIV(thread).current_priority
```

修复方法：

```diff
- rt_uint8_t prio = rt_thread_self()->current_priority + 1;
+ rt_uint8_t prio = RT_SCHED_PRIV(rt_thread_self()).current_priority + 1;
```

> `RT_SCHED_PRIV` 宏定义在 `rtsched.h` 中，展开为 `(thread)->sched_thread_ctx.sched_thread_priv`

---

## 五、注册 RT-Thread 扩展库（rtext）

为了让 Lua 脚本能调用 RT-Thread 的硬件驱动 API，我们需要注册扩展函数。

### 5.1 设计思路

不修改 Lua 包的任何源码，而是通过 Lua 官方推荐的方式——**定制 `linit.c`** 来注入我们的扩展库。

```
标准 Lua 库加载流程：
  lua_main()
    └─ pmain()
         └─ luaL_openlibs()     ← 遍历 loadedlibs[] 表
              ├─ luaopen_base()     → print, type, ...
              ├─ luaopen_io()       → io.open, io.read, ...
              ├─ luaopen_rtext()    → gpio_mode, gpio_write, ... ← 我们加的
              └─ ...
```

### 5.2 创建自定义 linit.c

将 Lua 官方的 `linit.c` 复制到 `applications/linit.c`，在 `loadedlibs[]` 中增加 `rtext` 库：

```c
static const luaL_Reg loadedlibs[] =
{
    {"_G", luaopen_base},
    {LUA_LOADLIBNAME, luaopen_package},
    /* ... 其他标准库 ... */
    {"rtext", luaopen_rtext},     /* ← RT-Thread 扩展库 */
    {NULL, NULL}
};
```

然后在 IAR 工程中将 `packages/.../linit.c` 替换为 `applications/linit.c`。

### 5.3 实现 luaopen_rtext

在 `lua2rtt.c` 中实现扩展库的打开函数，注册四个全局函数：

```c
int luaopen_rtext(lua_State *L)
{
    /* 注册为全局函数，Lua 中可直接调用 */
    lua_pushcfunction(L, lua_rt_gpio_mode);
    lua_setglobal(L, "gpio_mode");
    lua_pushcfunction(L, lua_rt_gpio_write);
    lua_setglobal(L, "gpio_write");
    lua_pushcfunction(L, lua_rt_gpio_read);
    lua_setglobal(L, "gpio_read");
    lua_pushcfunction(L, lua_rt_delay);
    lua_setglobal(L, "rtos_delay");

    /* 同时创建模块表，支持 require("rtext") */
    lua_newtable(L);
    /* ... 填入同名函数 ... */
    return 1;
}
```

### 5.4 GPIO 模式映射

为了更加直观，Lua 侧的 mode 参数做了友好映射：

| Lua 值 | 含义 | RT-Thread 宏 |
|--------|------|-------------|
| `0` | 输入 | `PIN_MODE_INPUT` |
| `1` | 输出 | `PIN_MODE_OUTPUT` |
| `2` | 上拉输入 | `PIN_MODE_INPUT_PULLUP` |

---

## 六、测试验证

### 6.1 进入 Lua 交互模式

```bash
msh /> lua

Press CTRL+D to exit Lua.
Lua 5.3.4  Copyright (C) 1994-2017 Lua.org, PUC-Rio
```

### 6.2 测试 GPIO 控制

```lua
> gpio_mode(28, 1)      -- PB12 设为输出
> gpio_write(28, 1)     -- 亮灯
> rtos_delay(1000)      -- 等待 1 秒
> gpio_write(28, 0)     -- 灭灯
```

### 6.3 测试循环闪烁

```lua
> for i = 1, 5 do
>>   gpio_write(28, 1)
>>   rtos_delay(200)
>>   gpio_write(28, 0)
>>   rtos_delay(200)
>> end
```

### 6.4 退出 Lua

按 `Ctrl + D` 返回 msh。

---

## 七、文件结构总结

新增/修改的文件：

```
applications/
├── linit.c                  ← 自定义 Lua 库初始化（替换包中的 linit.c）
packages/Lua-latest/
├── lua2rtt.c                ← 修改：增加 luaopen_rtext 实现
├── lua-5.3.4/lua.c          ← 未修改（使用 linit.c 机制注入）
project.ewp                  ← 修改：替换 linit.c 引用路径
rtconfig.h                   ← 新增 Lua 包配置宏
```

---

## 八、遇到的问题与解决

| 问题 | 原因 | 解决 |
|------|------|------|
| `current_priority` 找不到 | RT-Thread v5.x 调度器重构 | 改为 `RT_SCHED_PRIV(thread).current_priority` |
| 不想改 Lua 包源码 | 包文件在 `packages/` 下，gitignored | 用自定义 `linit.c` 替换，不修改任何包文件 |
| `lua_State` 未定义 | 缺少 Lua 头文件引用 | 在 `lua2rtt.c` 增加 `#include "lua.h"` 等 |
| `gpio_mode(28, 1)` 没反应 | `PIN_MODE_OUTPUT = 0x00` 不是 1 | 在 Lua 侧做 mode 值映射，1→OUTPUT |

---

## 九、下期预告

现在我们已经能在 Lua 交互模式下手动控制硬件了，但这还不够方便。下一篇文章将介绍：

- 如何将 Lua 脚本**编译进固件**
- 如何通过 `lua_run <script>` 命令**一键运行**
- 如何实现**开机自启脚本**