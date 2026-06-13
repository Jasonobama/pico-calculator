# Pico Calculator

基于 Raspberry Pi Pico 的简易计算器，支持加减乘除四则运算，使用 4x4 矩阵键盘输入，LCD1602 显示。

## 硬件连接

| 外设 | 引脚 | 说明 |
|------|------|------|
| LCD1602 (I2C) | SDA: GP4, SCL: GP5 | I2C0, 地址 0x27 |
| 4x4 键盘行线 | GP10, GP11, GP12, GP13 | 行扫描输出 |
| 4x4 键盘列线 | GP18, GP19, GP20, GP21 | 列扫描输入 |

## 4x4 键盘布局

```
  [1]  [2]  [3]  [+]
  [4]  [5]  [6]  [-]
  [7]  [8]  [9]  [*]
 [DEL] [0]  [=]  [/]
```

- 第 1 行：`1`, `2`, `3`, `+`
- 第 2 行：`4`, `5`, `6`, `-`
- 第 3 行：`7`, `8`, `9`, `*`
- 第 4 行：`DEL`, `0`, `=`, `/`

按键内置去抖动逻辑，长按不会重复输入。

## 程序结构

```
calculator/
├── calculator.c              # 主程序（LCD 驱动、计算逻辑、状态机）
├── CMakeLists.txt            # CMake 构建配置
├── pico_sdk_import.cmake     # Pico SDK 导入脚本
├── blink.pio                 # PIO 程序（未实际使用）
└── lib/
    ├── pico_keypad4x4.h      # 键盘库头文件（第三方）
    └── pico_keypad4x4.c      # 键盘库实现（第三方，扫描、去抖）
```

### calculator.c

主程序，包含以下模块：

- **LCD1602 驱动** — 基于 I2C 的字符液晶显示驱动，支持 4 位模式初始化、光标定位、字符串显示
- **计算引擎** — `calculate()` 函数执行四则运算，支持浮点数输入，自动去除末尾多余的零
- **状态机** — 三状态管理（`INPUT_FIRST` → `INPUT_SECOND` → `RESULT`），处理连续运算
- **按键处理** — `process_key()` 分发按键到数字输入 / 运算符 / 删除 / 等于

### lib/pico_keypad4x4

第三方 4x4 矩阵键盘扫描库：

- `pico_keypad_init()` — 初始化行列 GPIO 及字符映射表
- `pico_keypad_get_key()` — 扫描并返回当前按下的按键，内置边缘检测（仅返回按下瞬间的值，长按不重复触发）
- `pico_keypad_irq_enable()` — 中断模式支持（当前未使用）

### CMakeLists.txt 第三方库集成

项目使用第三方键盘库 `lib/pico_keypad4x4`，需要在 CMakeLists.txt 中显式声明头文件路径和源文件：

```cmake
# 告诉 CMake 去哪里找第三方头文件
include_directories(${CMAKE_CURRENT_LIST_DIR}/lib)

# 添加主程序和第三方库的源文件
add_executable(calculator
    calculator.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pico_keypad4x4.c
)
```

> **注意**：若引入其他第三方库，同样需要在 `include_directories` 中添加头文件路径，在 `add_executable` 中添加对应的 `.c` 源文件，否则编译会报找不到符号的错误。

## 构建与烧录

### 依赖

- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk) v2.2.0
- ARM GCC 工具链 14.2
- CMake >= 3.13

### 构建

```bash
mkdir build && cd build
cmake .. -G "NMake Makefiles"
nmake
```

或在 VS Code 中使用 Raspberry Pi Pico 扩展直接构建。

### 烧录

1. 按住 Pico 上的 BOOTSEL 按钮，通过 USB 连接电脑
2. 将生成的 `calculator.uf2` 拖入出现的 RPI-RP2 磁盘

或使用 picotool：

```bash
picotool load calculator.uf2
```

## 调试

通过 USB 串口输出按键信息，使用串口工具（波特率 115200）连接即可查看：

```
Key pressed: 1
Key pressed: +
Key pressed: 2
Key pressed: #
```

## 许可

MIT License
