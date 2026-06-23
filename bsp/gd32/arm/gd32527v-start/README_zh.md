# GD32F527V-START 开发板 RT-Thread 移植文档

## 一、概述

本文档记录了将 RT-Thread 实时操作系统移植到 **GD32F527V-START** 开发板的完整过程。

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
| **RT-Thread 版本** | 5.2.1 |

### BSP 目录结构

```
rt-thread/bsp/gd32/arm/gd32527v-start/
├── applications/
│   ├── SConscript
│   └── main.c                  # 应用主程序（LED 闪烁）
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
├── figures/
├── packages/                   # 下载的固件库包（pkgs --update 后生成）
│   ├── gd32-arm-cmsis-latest/  # CMSIS 核心及启动文件
│   └── gd32-arm-series-latest/ # 标准外设库
├── .config                     # menuconfig 配置
├── Kconfig                     # 顶层配置入口
├── README.md                   # BSP 说明文档
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

## 二、移植环境准备

### 2.1 硬件准备

- GD32F527V-START 开发板 × 1
- USB Type-C 数据线 × 1（供电 + GD-Link 调试 + 虚拟串口）

### 2.2 软件准备

| 工具 | 用途 | 下载地址 |
|------|------|---------|
| RT-Thread 源码 | 提供 RTOS 内核及 BSP 框架 | https://github.com/RT-Thread/rt-thread |
| env 工具 | 包管理和 scons 构建 | https://www.rt-thread.org/download.html |
| IAR Embedded Workbench | 编译调试（推荐 v8.3+） | 或 Keil MDK / ARM GCC |
| 串口助手 | 查看 FinSH 控制台输出 | 如 PuTTY、MobaXterm、SSCOM |

### 2.3 获取 RT-Thread 源码

```bash
# 从 GitHub 克隆（国内推荐 Gitee 镜像）
git clone --depth 1 https://github.com/RT-Thread/rt-thread.git

# 或从 Gitee 镜像
git clone --depth 1 https://gitee.com/rtthread/rt-thread.git
```

---

## 三、创建 BSP

### 3.1 复制官方 EVAL BSP 作为模板

```bash
cd rt-thread/bsp/gd32/arm/
cp -r gd32527I-eval gd32527v-start
cd gd32527v-start
```

### 3.2 更新软件包（拉取 GD32 固件库）

```bash
# 必须先通过 env 工具进入 RT-Thread 环境
# Windows 下：双击 env.exe 或在命令行运行 env.bat

pkgs --upgrade-force
pkgs --update
```

执行后 `packages/` 目录下会下载：
- `gd32-arm-cmsis-latest/` — CMSIS 核心、启动文件、系统初始化
- `gd32-arm-series-latest/` — 标准外设库驱动

---

## 四、修改文件清单

### 4.1 `board/board.h` — SRAM 大小

```c
// 主 SRAM 512KB，TCM SRAM 64KB（共 576KB）
#define GD32_SRAM_SIZE         512
#define GD32_SRAM_END          (0x20000000 + GD32_SRAM_SIZE * 1024)
```

> **注意**：GD32F527VST7 的 SRAM 分布为：
> - `0x10000000 - 0x1000FFFF`：64KB TCM SRAM
> - `0x20000000 - 0x2007FFFF`：512KB 主 SRAM
>
> 链接脚本只能映射连续内存，主 SRAM 为 512KB。

### 4.2 链接脚本 — 内存布局

#### `board/linker_scripts/link.ld` (GCC)

```ld
MEMORY
{
    CODE (rx) : ORIGIN = 0x08000000, LENGTH = 2048k   /* Code Flash */
    DATA (rw) : ORIGIN = 0x20000000, LENGTH =  512k   /* 主 SRAM */
}
```

#### `board/linker_scripts/link.sct` (MDK/ARMCC)

```
LR_IROM1 0x08000000 0x00200000  {
  ER_IROM1 0x08000000 0x00200000  {
   *.o (RESET, +First)
   *(InRoot$$Sections)
   .ANY (+RO)
  }
  RW_IRAM1 0x20000000 0x00080000  {  ; 512KB 主 SRAM
   .ANY (+RW +ZI)
  }
}
```

#### `board/linker_scripts/link.icf` (IAR)

```
define symbol __ICFEDIT_region_ROM_start__ = 0x08000000;
define symbol __ICFEDIT_region_ROM_end__   = 0x080FFFFF;
define symbol __ICFEDIT_region_RAM_start__ = 0x20000000;
define symbol __ICFEDIT_region_RAM_end__   = 0x2007FFFF;  /* 512KB */
```

### 4.3 `board/Kconfig` — SoC 型号 + 串口配置

修改 SoC 定义（从 EVAL 的 `SOC_GD32F527IS` 改为 V-START 的 `SOC_GD32F527VS`）：

```kconfig
config SOC_GD32F527VS
    bool
    select SOC_SERIES_GD32F5xx
    select RT_USING_COMPONENTS_INIT
    select RT_USING_USER_MAIN
    default y
```

修改 UART0 为 USART5（默认控制台从 UART0 改为 USART5）：

```kconfig
# 原 EVAL 板：UART0 PA9/PA10 → 改为默认关闭
menuconfig BSP_USING_UART0
    bool "Enable UART0"
    default n

# 新增 USART5 配置（PC6/PC7, AF8）
menuconfig BSP_USING_UART5
    bool "Enable UART5"
    default y
    if BSP_USING_UART5
        config BSP_UART5_TX_PIN
            string "UART5 TX name"
            default "PC6"
        config BSP_UART5_RX_PIN
            string "UART5 RX name"
            default "PC7"
        config BSP_UART5_AFIO
            string "UART5 alternate function (注意：GD32F527 USART5 使用 AF8)"
            default "AF8"
    endif
```

### 4.4 `rtconfig.h` — 内核配置

```c
/* 控制台设备改为 uart5 */
#define RT_CONSOLE_DEVICE_NAME "uart5"

/* SoC 型号 */
#define SOC_GD32F527VS

/* 串口配置 */
#define BSP_USING_UART5
#define BSP_UART5_TX_PIN "PC6"
#define BSP_UART5_RX_PIN "PC7"
#define BSP_UART5_AFIO "AF8"  /* 注意：USART5 on PC6/PC7 使用 AF8，不是 AF7 */
```

### 4.5 `rtconfig.py` — 工具链路径

根据实际安装的 IDE 修改工具链路径：

```python
if CROSS_TOOL == 'gcc':
    EXEC_PATH   = r'C:\Program Files\ARM GNU Toolchain'  # 根据实际路径修改
elif CROSS_TOOL == 'keil':
    EXEC_PATH   = r'C:/Keil_v5'
elif CROSS_TOOL == 'iar':
    EXEC_PATH   = r'C:/Program Files (x86)/IAR Systems/Embedded Workbench 8.3'
```

### 4.6 `applications/main.c` — LED 引脚

```c
/* GD32F527V-START 板载 LED 在 PB11 */
#define LED1_PIN GET_PIN(B, 11)
```

### 4.7 `libcpu/arm/cortex-m33/context_iar.S` — IAR 汇编兼容性修复

修复 IAR 汇编器中 `skip_push_fpu` 标签重复定义的问题：

```asm
; PendSV_Handler 中的 label（保留原名）
skip_push_fpu

; HardFault_Handler 中的 label（重命名避免冲突）
skip_push_fpu_hardfault
```

### 4.8 删除不需要的编译宏

在 IAR 工程选项中移除多余的 `GD32F4XX` 定义（从原 EVAL 模板继承而来）。

---

## 五、编译与下载

### 5.1 IAR 编译

```bash
# 在 env 工具窗口中
scons --target=iar
```

然后在 IAR 中：
1. 打开 `project.eww`
2. 确保芯片选择为 **GD32F527VST7**
3. 编译（F7）
4. 下载（Ctrl+D）

### 5.2 MDK 编译

```bash
scons --target=mdk5
```

### 5.3 GCC 编译

```bash
# 修改 rtconfig.py 中的 CROSS_TOOL 为 'gcc'
# 设置正确的 GCC 工具链路径
scons -j4
```

---

## 六、运行结果

下载成功后，板载 LED（PB11）以 500ms 间隔闪烁。

用串口助手连接 GD-Link 虚拟串口（115200-8-N-1），可看到 RT-Thread 启动信息：

```
 \ | /
- RT -     Thread Operating System
 / | \     5.2.1 build ...
 2006 - 2024 Copyright by RT-Thread team
msh />
```

在 FinSH 控制台中可执行 `help`、`list_thread`、`list_device` 等命令。

---

## 七、常见问题

### 7.1 HardFault：precise data access error

**现象**：系统启动即进入 HardFault，错误地址在 `0x2008xxxx` 范围。

**原因**：链接脚本中 SRAM 大小超过实际可用大小。
- GD32F527VST7 在 `0x20000000` 地址只有 **512KB** 主 SRAM
- 额外 64KB 在 `0x10000000`（TCM SRAM），不能与主 SRAM 连续映射

**解决**：将链接脚本中 `0x20000000` 区域的长度设为 512KB。

### 7.2 IAR 编译报错 `Duplicate label:'skip_push_fpu'`

**原因**：IAR 汇编器要求标签全局唯一，`context_iar.S` 中 `skip_push_fpu` 在 `PendSV_Handler` 和 `HardFault_Handler` 中各出现一次。

**解决**：将 `HardFault_Handler` 中的 `skip_push_fpu` 重命名为 `skip_push_fpu_hardfault`。

### 7.3 串口无输出

**排查步骤**：
1. 确认 `rtconfig.h` 中 `RT_CONSOLE_DEVICE_NAME` 为 `"uart5"`
2. 确认 `BSP_USING_UART5` 已定义
3. 确认引脚配置正确：TX=PC6, RX=PC7, **AF=AF8**（不是 AF7）
4. 确认串口助手波特率为 115200
5. 检查串口线是否接反（必要时对调 TX/RX）

### 7.4 env 工具命令找不到

**原因**：系统 PATH 中其他工具（如 w64devkit）的 `env.exe` 优先级高于 RT-Thread env。

**解决**：
```powershell
# 通过完整路径启动 env
D:\Programs\env-windows\env.bat

# 或将 env 目录加入 PATH 最前面
$env:PATH = "D:\Programs\env-windows;$env:PATH"
```

---

## 八、修改文件汇总

| 文件 | 修改内容 | 说明 |
|------|---------|------|
| `board/board.h` | `SRAM_SIZE` 448→512 | 适配 512KB 主 SRAM |
| `board/linker_scripts/link.ld` | DATA 512k→512k | GCC 链接脚本修正 |
| `board/linker_scripts/link.sct` | 0x00090000→0x00080000 | MDK 链接脚本修正 |
| `board/linker_scripts/link.icf` | 0x2008FFFF→0x2007FFFF | IAR 链接脚本修正 |
| `board/Kconfig` | SOC_GD32F527IS→SOC_GD32F527VS | 芯片型号 |
| `board/Kconfig` | 新增 UART5 配置（AF8） | 控制台串口 |
| `board/Kconfig` | UART0 default n | 关闭默认 UART0 |
| `rtconfig.h` | CONSOLE="uart5", UART5 PC6/PC7/AF8 | 控制台配置 |
| `.config` | 同步更新 | menuconfig 配置 |
| `applications/main.c` | LED1_PIN PE3→PB11 | 板载 LED 引脚 |
| `libcpu/arm/cortex-m33/context_iar.S` | skip_push_fpu_hardfault | IAR 汇编兼容性 |
| `README.md` | 重写 | 适配 V-START 板说明 |

---

## 九、参考资料

- [RT-Thread 官方文档](https://www.rt-thread.org/document/site/)
- [GD32F527 产品页](https://www.gigadevice.com/product/mcu/high-performance-mcus/gd32f5xx-series/gd32f527)
- [GD32F527 数据手册](https://download.gigadevice.com/Datasheet/GD32F527xx_Datasheet_Rev1.8.pdf)
- [GD32F527 用户手册](https://download.gigadevice.com/User_Manual/GD32F527xx_User_Manual_Rev1.4.pdf)
- [GD32F527 Demo Suites](https://www.gd32mcu.com/cn/download/8?kw=GD32F5)
- [GD32 ARM 系列 BSP 制作教程](https://github.com/RT-Thread/rt-thread/blob/master/bsp/gd32/arm/docs/GD32_ARM%E7%B3%BB%E5%88%97BSP%E5%88%B6%E4%BD%9C%E6%95%99%E7%A8%8B.md)
- [PR #10454 — 添加 GD32F527I BSP](https://github.com/RT-Thread/rt-thread/pull/10454)
