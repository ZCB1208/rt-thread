# GD32F527V-START 开发板 RT-Thread 移植文档

## 一、概述

本文档记录了将 RT-Thread 实时操作系统移植到 **GD32F527V-START** 开发板的完整过程，以及后续添加的 Lua 脚本引擎、脚本编译嵌入等扩展功能。

### 开发板信息

| 项目 | 说明 |
|------|------|
| **开发板型号** | GD32F527V-START（兆易创新官方 START 系列评估板） |
| **主控芯片** | GD32F527VST7 (LQFP100 封装) |
| **CPU 内核** | Arm Cortex-M33 @200MHz |
| **Flash** | 7680KB (Code Flash 2048KB 映射) |
| **SRAM** | 576KB = 512KB（主 SRAM 0x20000000）+ 64KB（TCM SRAM 0x10000000） |
| **外部晶振** | 25MHz (PH0/PH1 - OSC_IN/OSC_OUT) |
| **板载调试器** | GD-Link (CMSIS-DAP) |
| **控制台串口** | USART5 (PC6 TX / PC7 RX, **AF8**)，通过板载 USB 转串口连接 PC |
| **板载 LED** | PB11 |
| **RT-Thread 版本** | 5.3.0 |

### BSP 目录结构

```
rt-thread/bsp/gd32/arm/gd32527v-start/
├── applications/
│   ├── SConscript
│   ├── main.c                  # 应用主程序（LED 闪烁）
│   ├── linit.c                 # 自定义 Lua 库加载（注入 rtext 扩展）
│   ├── lua_cmd.c               # lua_run / lua_list 命令
│   ├── lua_script.h            # 脚本注册表类型定义
│   ├── lua_script_table.inc    # 自动生成的脚本表（由 lua2inc.py 生成）
│   └── scripts/
│       └── led_blink.lua       # LED 闪烁脚本示例
├── board/
│   ├── Kconfig                 # 硬件配置菜单（SoC、外设开关）
│   ├── SConscript              # 板级编译脚本
│   ├── board.c                 # 板级初始化（时钟、堆、串口）
│   ├── board.h                 # 板级头文件（SRAM 大小定义）
│   ├── gd32f5xx_libopt.h       # 固件库外设头文件选择
│   └── linker_scripts/
│       ├── link.ld             # GCC 链接脚本
│       ├── link.sct            # MDK (ARMCC) 链接脚本
│       └── link.icf            # IAR 链接脚本
├── docs/
│   ├── blog-lua-porting.md     # Lua 移植博客
│   └── blog-lua-embed.md       # Lua 脚本嵌入博客
├── figures/
├── packages/                   # 下载的固件库包（pkgs --update 后生成）
│   ├── gd32-arm-cmsis-latest/  # CMSIS 核心及启动文件
│   ├── gd32-arm-series-latest/ # 标准外设库
│   └── Lua-latest/             # Lua 5.3.4 源码（含修改的 lua2rtt.c）
├── tools/
│   └── lua2inc.py              # .lua → .inc 转换脚本
├── .config                     # menuconfig 配置
├── Kconfig                     # 顶层配置入口
├── README.md                   # BSP 说明文档
├── README_zh.md                # BSP 说明文档（中文）
├── SConscript                  # 顶层编译脚本
├── SConstruct                  # 构建入口
├── project.ewd                 # IAR 调试配置
├── project.ewp                 # IAR 工程文件
├── project.eww                 # IAR 工作空间
├── project.uvoptx              # MDK 工程选项
├── project.uvproj              # MDK 工程（旧版）
├── project.uvprojx             # MDK 工程文件
├── rtconfig.h                  # RT-Thread 内核配置
├── rtconfig.py                 # 工具链配置
├── template.ewp                # IAR 工程模板
├── template.uvoptx             # MDK 工程模板选项
├── template.uvproj             # MDK 工程模板
└── template.uvprojx            # MDK 工程模板
```

---

## 二、基础 BSP 移植

### 2.1 移植环境准备

#### 硬件
- GD32F527V-START 开发板 × 1
- USB Type-C 数据线 × 1（供电 + GD-Link 调试 + 虚拟串口）

#### 软件
| 工具 | 用途 | 下载地址 |
|------|------|---------|
| RT-Thread 源码 | 提供 RTOS 内核及 BSP 框架 | https://github.com/RT-Thread/rt-thread |
| env 工具 | 包管理和 scons 构建 | https://www.rt-thread.org/download.html |
| IAR Embedded Workbench | 编译调试（推荐 v8.3+） | 或 Keil MDK / ARM GCC |
| 串口助手 | 查看 FinSH 控制台输出 | 如 PuTTY、MobaXterm、SSCOM |

#### 获取 RT-Thread 源码

```bash
git clone --depth 1 https://github.com/RT-Thread/rt-thread.git
# 或国内镜像
git clone --depth 1 https://gitee.com/rtthread/rt-thread.git
```

### 2.2 创建 BSP

```bash
cd rt-thread/bsp/gd32/arm/
cp -r gd32527I-eval gd32527v-start
cd gd32527v-start
```

### 2.3 更新软件包

```bash
# 通过 env 工具进入 RT-Thread 环境
pkgs --upgrade-force
pkgs --update
```

### 2.4 关键修改

#### board/board.h — SRAM 大小

```c
// 主 SRAM 512KB（地址 0x20000000-0x2007FFFF）
// TCM SRAM 64KB（地址 0x10000000-0x1000FFFF，总计 576KB）
#define GD32_SRAM_SIZE         512
#define GD32_SRAM_END          (0x20000000 + GD32_SRAM_SIZE * 1024)
```

#### 链接脚本 — 内存布局（三个脚本同步修改）

| 链接脚本 | 修改前 | 修改后 |
|---------|--------|--------|
| `link.ld` (GCC) | `LENGTH = 576k` | `LENGTH = 512k` |
| `link.sct` (MDK) | `0x00090000` (576K) | `0x00080000` (512K) |
| `link.icf` (IAR) | `0x2008FFFF` | `0x2007FFFF` |

#### board/Kconfig — SoC 型号 + 串口配置

**SoC 型号**：`SOC_GD32F527IS` → `SOC_GD32F527VS`

**串口配置**：USART5 (PC6/PC7, **AF8**) 为控制台，UART0 默认关闭

```kconfig
menuconfig BSP_USING_UART0
    bool "Enable UART0"
    default n

menuconfig BSP_USING_UART5
    bool "Enable UART5"
    default y
    if BSP_USING_UART5
        config BSP_UART5_TX_PIN
            default "PC6"
        config BSP_UART5_RX_PIN
            default "PC7"
        config BSP_UART5_AFIO
            default "AF8"       /* 注意：GD32F527 上是 AF8，不是 AF7 */
    endif
```

#### rtconfig.h — 控制台设备

```c
#define RT_CONSOLE_DEVICE_NAME "uart5"
#define BSP_USING_UART5
#define BSP_UART5_TX_PIN "PC6"
#define BSP_UART5_RX_PIN "PC7"
#define BSP_UART5_AFIO "AF8"
```

#### applications/main.c — LED 引脚

```c
/* GD32F527V-START 板载 LED 在 PB11 */
#define LED1_PIN GET_PIN(B, 11)
```

#### libcpu/arm/cortex-m33/context_iar.S — IAR 汇编兼容性

修复 `skip_push_fpu` 标签重复定义：

```asm
; PendSV_Handler
skip_push_fpu

; HardFault_Handler
skip_push_fpu_hardfault    ; 重命名避免冲突
```

#### drv_gpio.c — 增加 pin_get 回调

在 `gd32_pin_ops` 中实现 `pin_get` 回调，支持 `pin num PB.12` 命令：

```c
static rt_base_t gd32_pin_get(const char *name)
{
    /* 支持 PB.12 和 PB12 两种格式 */
    ...
    return port_idx * 16 + pin_num;
}
```

---

## 三、编译与下载

### 3.1 IAR 编译

```bash
scons --target=iar
```

IAR 中打开 `project.eww`，编译（F7）并下载（Ctrl+D）。

### 3.2 运行结果

```
 \ | /
- RT -     Thread Operating System
 / | \     5.3.0 build ...
 2006 - 2024 Copyright by RT-Thread team
msh />
```

板载 LED（PB11）以 500ms 间隔闪烁。

---

## 四、Lua 脚本引擎

### 4.1 添加 Lua 包

```bash
# menuconfig 中开启 Lua 包
menuconfig
# RT-Thread online packages → language packages → [*] Lua → Version (latest)

pkgs --update
```

### 4.2 修复 API 兼容性

RT-Thread v5.x 调度器重构，`current_priority` 移到嵌套结构体中：

```diff
- rt_uint8_t prio = rt_thread_self()->current_priority + 1;
+ rt_uint8_t prio = RT_SCHED_PRIV(rt_thread_self()).current_priority + 1;
```

### 4.3 注册 RT-Thread 扩展库（rtext）

采用自定义 `linit.c` 替换 Lua 包中的 linit.c，在 `loadedlibs[]` 中增加 rtext 库：

```c
static const luaL_Reg loadedlibs[] = {
    {"_G", luaopen_base},
    /* ... 标准库 ... */
    {"rtext", luaopen_rtext},     /* ← RT-Thread 扩展 */
    {NULL, NULL}
};
```

注册的 Lua 全局函数：

| Lua 函数 | 对应 C API | 说明 |
|---------|-----------|------|
| `gpio_mode(pin, mode)` | `rt_pin_mode()` | mode: 0=input, 1=output, 2=pullup |
| `gpio_write(pin, val)` | `rt_pin_write()` | val: 0/1 |
| `gpio_read(pin)` | `rt_pin_read()` | 返回 0/1 |
| `rtos_delay(ms)` | `rt_thread_mdelay()` | 毫秒延时 |

### 4.4 交互模式测试

```bash
msh /> lua
Lua 5.3.4  Copyright (C) 1994-2017 Lua.org, PUC-Rio
> gpio_mode(28, 1)      -- PB12 output
> gpio_write(28, 1)     -- 亮
> rtos_delay(1000)
> gpio_write(28, 0)     -- 灭
> [Ctrl+D] 退出
```

---

## 五、Lua 脚本编译嵌入

### 5.1 脚本转换工具

`tools/lua2inc.py` 将 `applications/scripts/*.lua` 转换为 C 头文件：

```bash
python tools/lua2inc.py applications/scripts applications/lua_script_table.inc
```

IAR Pre-build 自动执行（在 IAR 工程 Build Actions 中配置）。

### 5.2 msh CLI 命令

| 命令 | 功能 |
|------|------|
| `lua_run led_blink` | 运行 led_blink 脚本 |
| `lua_list` | 列出所有嵌入脚本 |

示例：

```bash
msh /> lua_list
Available Lua scripts:
  led_blink

msh /> lua_run led_blink
LED blinking on PB12...
Done!
```

### 5.3 LED 闪烁脚本示例

```lua
-- applications/scripts/led_blink.lua
gpio_mode(28, 1)
print("LED blinking on PB12...")
for i = 1, 10 do
    gpio_write(28, 1)
    rtos_delay(200)
    gpio_write(28, 0)
    rtos_delay(200)
end
print("Done!")
```

---

## 六、常见问题

### 6.1 HardFault：precise data access error

**现象**：系统启动即进入 HardFault，错误地址在 `0x2008xxxx` 范围。

**原因**：链接脚本中 SRAM 大小超过实际可用大小。

**解决**：将链接脚本中 `0x20000000` 区域的长度设为 **512KB**。

### 6.2 IAR 编译报错 `Duplicate label:'skip_push_fpu'`

**原因**：IAR 汇编器要求标签全局唯一。

**解决**：将 `HardFault_Handler` 中的 `skip_push_fpu` 重命名为 `skip_push_fpu_hardfault`。

### 6.3 串口无输出

**排查步骤**：
1. 确认 `RT_CONSOLE_DEVICE_NAME` 为 `"uart5"`
2. 确认 `BSP_USING_UART5` 已定义
3. 确认引脚配置：TX=PC6, RX=PC7, **AF=AF8**（不是 AF7！）
4. 确认串口助手波特率为 115200
5. 必要时对调 TX/RX 试一下

### 6.4 `pin num PB.12` 命令无效

**原因**：GD32 驱动未实现 `pin_get` 回调。

**解决**：已在 `drv_gpio.c` 中实现 `gd32_pin_get`，支持 `PB.12` 和 `PB12` 两种格式。

### 6.5 引脚编号速查

| GPIO | 编号 |
|------|------|
| PA0 ~ PA15 | 0 ~ 15 |
| PB0 ~ PB15 | 16 ~ 31 |
| PC0 ~ PC15 | 32 ~ 47 |
| PD0 ~ PD15 | 48 ~ 63 |

PB12 = 28

---

## 七、修改文件汇总

| 类别 | 文件 | 修改内容 |
|------|------|---------|
| **基础移植** | `board/board.h` | SRAM_SIZE 448→512 |
| | `board/linker_scripts/link.ld` | DATA 576k→512k |
| | `board/linker_scripts/link.sct` | 0x00090000→0x00080000 |
| | `board/linker_scripts/link.icf` | 0x2008FFFF→0x2007FFFF |
| | `board/Kconfig` | SOC_GD32F527IS→VS，UART5+AF8 |
| | `rtconfig.h` | CONSOLE="uart5", UART5 PC6/PC7/AF8 |
| | `.config` | 同步配置 |
| | `applications/main.c` | LED1_PIN PE3→PB11 |
| | `libcpu/arm/cortex-m33/context_iar.S` | skip_push_fpu_hardfault |
| | `.github/ALL_BSP_COMPILE.json` | CI 增加 gd32527v-start |
| | `bsp/gd32/arm/README.md` | F5 系列条目 |
| **pin_get 回调** | `libraries/gd32_drivers/drv_gpio.c` | 新增 gd32_pin_get 实现 |
| **Lua 引擎** | `packages/Lua-latest/lua2rtt.c` | API 兼容性修复 + luaopen_rtext |
| | `applications/linit.c` | 自定义 linit.c（注入 rtext） |
| | `project.ewp` | 替换 linit.c 引用 |
| **脚本嵌入** | `applications/lua_cmd.c` | lua_run / lua_list 命令 |
| | `applications/lua_script.h` | 脚本注册表类型定义 |
| | `applications/lua_script_table.inc` | 自动生成 |
| | `applications/scripts/led_blink.lua` | LED 闪烁脚本 |
| | `tools/lua2inc.py` | .lua → .inc 转换工具 |
| **文档** | `README.md` | 适配 V-START 板 |
| | `README_zh.md` | 中文详细移植文档 |
| | `docs/blog-lua-porting.md` | Lua 移植博客 |
| | `docs/blog-lua-embed.md` | 脚本嵌入博客 |

---

## 八、参考资料

- [RT-Thread 官方文档](https://www.rt-thread.org/document/site/)
- [GD32F527 产品页](https://www.gigadevice.com/product/mcu/high-performance-mcus/gd32f5xx-series/gd32f527)
- [GD32F527 数据手册](https://download.gigadevice.com/Datasheet/GD32F527xx_Datasheet_Rev1.8.pdf)
- [GD32F527 用户手册](https://download.gigadevice.com/User_Manual/GD32F527xx_User_Manual_Rev1.4.pdf)
- [GD32F527 Demo Suites](https://www.gd32mcu.com/cn/download/8?kw=GD32F5)
- [GD32 ARM 系列 BSP 制作教程](https://github.com/RT-Thread/rt-thread/blob/master/bsp/gd32/arm/docs/GD32_ARM%E7%B3%BB%E5%88%97BSP%E5%88%B6%E4%BD%9C%E6%95%99%E7%A8%8B.md)
- [Lua 5.3 参考手册](https://www.lua.org/manual/5.3/)