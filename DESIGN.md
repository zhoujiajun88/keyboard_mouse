# 键盘鼠标模式方案设计

## 已确认需求

实现一个 Windows 后台工具，用键盘模拟鼠标操作。

核心交互：

- 使用 `Ctrl + Alt + M` 激活 / 关闭鼠标模式
- 鼠标模式开启后：
  - `W` / `A` / `S` / `D` 移动鼠标光标
  - `J` 模拟鼠标左键
  - `K` 模拟鼠标右键
  - 方向键上 / 下模拟垂直滚轮
  - 方向键左 / 右模拟水平滚轮
- 支持 `Esc` 关闭鼠标模式
- 激活 / 关闭时，在屏幕正中间显示 1 秒提示：
  - `鼠标模式开启`
  - `鼠标模式关闭`
- 常驻托盘图标
- 托盘菜单支持设置：
  - 开机自启动
  - 鼠标移动速度，单位为 `px/s`

当前阶段仍然是方案确认，确认后再进入编码。

## 编程语言与技术路线

你希望使用方案 D，即 Rust / C++ 调用 Win32 API 实现。这条路线适合做一个原生、轻量、低依赖的 Windows 工具。

### Rust 路线

推荐程度：高。

建议使用：

- `windows` crate 调用 Win32 API
- `winit` 或纯 Win32 创建隐藏窗口和消息循环
- `serde` / `toml` 或 `serde_json` 保存配置

优点：

- 生成物体积可控，运行时依赖少
- 内存安全比 C++ 更好
- 适合长期维护
- 可以比较清晰地封装 Win32 API 调用

缺点：

- Win32 API 的 Rust 绑定写起来比 C++ 啰嗦一些
- 托盘菜单、窗口过程、热键、Hook 等都需要处理 unsafe 边界
- 初版工程搭建比 C++ 略慢

### C++ 路线

推荐程度：中高。

建议使用：

- 纯 Win32 API
- MSVC / CMake 构建
- 配置保存为 JSON / INI / 注册表

优点：

- Win32 API 示例和资料最多
- 托盘图标、消息循环、窗口过程、`SendInput` 都是原生写法
- 控制力强，生成物很轻

缺点：

- 内存和资源生命周期需要更谨慎
- 工程可维护性依赖代码规范
- 后续如果做复杂设置窗口，手写 Win32 UI 会比较累

## 语言最终选择

最终选择：C++。

理由：

- 直接调用 Win32 API，资料和示例最丰富
- 托盘图标、消息循环、全局热键、低级键盘钩子、`SendInput` 都可以使用原生写法
- 对这个工具来说依赖最少，发布形态清晰
- 后续如果需要继续扩展设置窗口，也可以在现有 Win32 工程上迭代

## Win32 API 设计

### 程序形态

程序启动后：

1. 创建一个隐藏窗口
2. 进入 Windows 消息循环
3. 注册全局热键 `Ctrl + Alt + M`
4. 安装低级键盘钩子
5. 创建系统托盘图标
6. 加载配置文件
7. 根据配置决定是否启用开机自启动

不显示主窗口，只通过托盘菜单和居中提示与用户交互。

### 关键 Win32 API

建议使用这些 Win32 API：

| 功能 | API |
| --- | --- |
| 全局快捷键 | `RegisterHotKey` / `UnregisterHotKey` |
| 键盘监听 | `SetWindowsHookExW(WH_KEYBOARD_LL)` |
| 鼠标输入模拟 | `SendInput` |
| 托盘图标 | `Shell_NotifyIconW` |
| 托盘菜单 | `CreatePopupMenu` / `TrackPopupMenu` |
| 隐藏窗口 | `CreateWindowExW` |
| 消息循环 | `GetMessageW` / `TranslateMessage` / `DispatchMessageW` |
| 居中提示窗口 | `CreateWindowExW` + layered/topmost popup |
| 开机自启动 | 写入 `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` |
| 配置保存 | 本地配置文件，或注册表 |

## 模式状态设计

核心状态：

- `mouse_mode_enabled`: 当前是否处于鼠标模式
- `pressed_keys`: 当前按住的方向键集合
- `move_speed_px_per_sec`: 鼠标移动速度
- `startup_enabled`: 是否开机自启动

状态切换：

- 按 `Ctrl + Alt + M`：
  - 如果当前关闭，则开启鼠标模式
  - 如果当前开启，则关闭鼠标模式
- 按 `Esc`：
  - 如果当前开启，则关闭鼠标模式
  - 如果当前关闭，则不处理，让 `Esc` 正常传递给系统

## 键位映射

鼠标模式开启后启用以下映射：

| 键位 | 行为 |
| --- | --- |
| `W` | 鼠标上移 |
| `A` | 鼠标左移 |
| `S` | 鼠标下移 |
| `D` | 鼠标右移 |
| `J` | 鼠标左键点击 |
| `K` | 鼠标右键点击 |
| `Up` | 垂直滚轮向上 |
| `Down` | 垂直滚轮向下 |
| `Left` | 水平滚轮向左 |
| `Right` | 水平滚轮向右 |
| `Esc` | 关闭鼠标模式 |

处理规则：

- 鼠标模式开启后，以上按键应被钩子拦截，不再传递给当前应用
- 鼠标模式关闭后，所有按键恢复正常
- 未映射按键默认不拦截
- `Ctrl + Alt + M` 使用 `RegisterHotKey` 处理，不依赖键盘钩子

## 鼠标移动设计

移动方式：按住持续移动。

推荐参数：

- 默认速度：`800 px/s`
- 设置范围：`100 px/s` 到 `3000 px/s`
- 定时器间隔：`10 ms` 到 `16 ms`
- 每次移动距离：`speed_px_per_sec * delta_time_sec`
- 允许斜向移动，例如同时按住 `W + D`

实现建议：

- 键盘钩子只负责记录 `WASD` 的按下 / 抬起状态
- 移动逻辑由定时器或消息循环 tick 驱动
- 每个 tick 根据当前方向计算位移，再调用 `SendInput`
- 对小数位移做累积，避免低速度下移动不均匀

第一版可以先不做长按加速，因为用户已经可以在设置里调整 `px/s`。

## 鼠标点击与滚轮设计

### 左右键

`J`：

- 按下时发送 `MOUSEEVENTF_LEFTDOWN`
- 抬起时发送 `MOUSEEVENTF_LEFTUP`

`K`：

- 按下时发送 `MOUSEEVENTF_RIGHTDOWN`
- 抬起时发送 `MOUSEEVENTF_RIGHTUP`

这样既支持点击，也天然支持长按拖拽。

### 滚轮

方向键：

- `Up`：发送正向 `MOUSEEVENTF_WHEEL`
- `Down`：发送负向 `MOUSEEVENTF_WHEEL`
- `Left`：发送负向 `MOUSEEVENTF_HWHEEL`
- `Right`：发送正向 `MOUSEEVENTF_HWHEEL`

建议滚轮步长：

- 默认：`WHEEL_DELTA = 120`
- 后续可扩展成设置项

## 托盘图标设计

程序启动后显示托盘图标。

托盘菜单建议：

| 菜单项 | 行为 |
| --- | --- |
| 鼠标模式：开启 / 关闭 | 点击后切换模式 |
| 移动速度... | 打开速度设置 |
| 开机自启动 | 勾选 / 取消勾选 |
| 退出 | 退出程序 |

托盘图标交互：

- 左键单击：切换鼠标模式，或打开菜单。建议第一版使用“打开菜单”，避免误触切换
- 右键单击：打开菜单
- 鼠标模式开启时，可以改变托盘图标或 tooltip 文案

托盘 tooltip：

- 关闭时：`键盘鼠标模式：关闭`
- 开启时：`键盘鼠标模式：开启`

## 设置设计

第一版需要支持两个设置项。

### 开机自启动

推荐实现：

- 写入当前用户注册表：
  - `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`
- 键名：`KeyboardMouseMode`
- 值：当前 exe 的完整路径

优点：

- 不需要管理员权限
- 对普通用户足够直观

关闭自启动：

- 删除上述注册表值

### 鼠标移动速度

单位：`px/s`。

推荐默认值：

- `800 px/s`

推荐范围：

- 最小：`100 px/s`
- 最大：`3000 px/s`
- 步进：`100 px/s`

设置入口：

- 托盘菜单点击 `移动速度...`
- 打开一个简单设置窗口
- 窗口中包含：
  - 数字输入框
  - 保存按钮
  - 取消按钮

保存位置建议：

- 配置文件路径：
  - `%APPDATA%\KeyboardMouseMode\config.toml`

配置文件示例：

```toml
move_speed_px_per_sec = 800
startup_enabled = false
```

说明：

- 开机自启动的真实状态以注册表为准
- 配置中的 `startup_enabled` 用于菜单显示和启动时同步
- 如果配置和注册表不一致，启动时应以注册表检测结果为准，并回写配置

## 居中提示设计

需求：

- 激活 / 关闭时在屏幕正中间显示提示
- 显示 1 秒后自动消失
- 文案：
  - 开启：`鼠标模式开启`
  - 关闭：`鼠标模式关闭`

实现建议：

- 使用一个无边框、置顶、透明背景或半透明背景的 popup 窗口
- 窗口样式：
  - `WS_POPUP`
  - `WS_EX_TOPMOST`
  - `WS_EX_TOOLWINDOW`
  - 可选 `WS_EX_LAYERED`
- 居中位置基于当前主显示器或鼠标所在显示器计算
- 通过 `SetTimer` 1 秒后隐藏或销毁提示窗口

视觉建议：

- 背景：深色半透明
- 文字：白色
- 圆角：可选，Win32 原生圆角实现可以后置
- 尺寸：根据文字自动计算，最小约 `220 x 72`
- 字号：约 `24 px`

第一版可以先做简单矩形提示，视觉 polish 后续再迭代。

## 配置与文件结构建议

如果选择 Rust：

```text
keyboard_mouse/
  Cargo.toml
  src/
    main.rs
    app.rs
    config.rs
    hotkey.rs
    keyboard_hook.rs
    mouse.rs
    tray.rs
    toast.rs
    startup.rs
```

如果选择 C++：

```text
keyboard_mouse/
  CMakeLists.txt
  src/
    main.cpp
    app.cpp
    app.h
    config.cpp
    config.h
    hotkey.cpp
    hotkey.h
    keyboard_hook.cpp
    keyboard_hook.h
    mouse.cpp
    mouse.h
    tray.cpp
    tray.h
    toast.cpp
    toast.h
    startup.cpp
    startup.h
```

## 实现步骤

建议按以下顺序开发：

1. 创建基础项目和隐藏窗口消息循环
2. 注册 `Ctrl + Alt + M` 全局热键
3. 实现鼠标模式状态切换
4. 实现居中 1 秒提示
5. 安装低级键盘钩子
6. 实现 `WASD` 持续移动
7. 实现 `J` / `K` 左右键
8. 实现方向键滚轮
9. 实现托盘图标和菜单
10. 实现速度设置窗口
11. 实现开机自启动设置
12. 保存和加载配置
13. 测试普通窗口、浏览器、编辑器、管理员窗口等场景

## 风险与注意事项

- 低级键盘钩子和键鼠模拟可能被安全软件提示
- 如果目标程序以管理员权限运行，本工具也可能需要管理员权限才能对其生效
- 某些游戏、远程桌面、虚拟机可能拦截或忽略模拟输入
- `Ctrl + Alt + M` 仍可能与个别应用快捷键冲突，但总体风险较低
- 程序退出时必须卸载钩子、移除托盘图标、释放热键
- 鼠标模式关闭时，应清空所有按键状态，避免残留移动

## 待最终确认

已最终确认：

1. 最终语言：C++
2. 默认鼠标速度：`800 px/s`
3. 托盘左键单击：切换鼠标模式
4. 托盘右键单击：打开菜单
5. 第一版设置窗口只包含速度输入和开机自启动
