# CursorBeacon：Windows 多屏 / 超高分辨率下的“晃一晃放大光标”项目规划

> 工作名：**CursorBeacon（光标信标）**
> 目标平台：Windows 10 1809+ / Windows 11（x64，后续可加 arm64）
> 许可证：MIT
> 文档状态：调研 + 可落地实施规划（尚未开始编码）

---

## 1. 项目目标

做一个常驻托盘的原生 Windows 小工具：

1. 用户连续左右/上下晃动鼠标时，光标**逐步放大**，而不是瞬间切换。
2. 放大倍数随晃动强度/持续时间增长，最大可配置（默认 4x，范围 2x–8x）。
3. 停止晃动后保持一小段时间（默认 800ms），然后平滑缩回原始大小。
4. 在**超高分辨率**（4K/5K/8K）和**多屏、混合 DPI** 环境下，放大后的光标在光标所在的那块屏上大小合理、清晰、不模糊。
5. **全屏应用所在的那块屏不启用**；多屏时如果只有一块屏是全屏应用，其他屏仍然正常工作。
6. 光标放大期间如果移动到全屏应用所在屏，立即缩回。
7. 不注入进程、不 Hook 游戏、不修改游戏内存；全屏游戏/视频/演示模式下完全不干扰。
8. 崩溃、注销、锁屏、退出后不留下“光标卡在大号”的状态；提供 `--restore` 急救命令。
9. 单 exe、无运行时依赖、无管理员权限（默认 asInvoker）、无遥测。
10. **轻量**：空闲 CPU < 0.05%、常驻内存 < 16MB、安装后体积 < 2MB；不装服务、不装驱动、不联网。
11. **无感**：按用户安装、默认开机启动、无首启向导、无成功弹窗；托盘常驻，设置随时可开。
12. **自然**：普通快速移动、拖拽、框选、滚动都不触发；只有连续来回晃动才放大，停止后自动缩回。

### 非目标（第一版不做）

- 不是屏幕放大镜，不缩放屏幕内容。
- 不是 PowerToys “Find My Mouse” 的聚光灯替代品（可作为可选的第二阶段功能）。
- 不处理游戏内自绘光标（全屏场景本来就禁用；窗口化游戏的自绘光标可选做 overlay 光晕）。
- 不做跨平台；不做云同步；不做自动更新（第二阶段可选）。
- 不支持 Windows 7/8（缺少 PerMonitorV2、`GetSystemMetricsForDpi`、高精度等待定时器等；强行兼容会显著增加复杂度）。

---

## 2. 现有项目调研结论

### 2.1 横向对比（数据截至 2026-10-28）

| 项目 | 星 | 语言/许可 | 放大方式 | 连续放大 | 多屏 | 每屏全屏抑制 | 每屏 DPI | 工程化 |
|---|---:|---|---|---|---|---|---|---|
| MrBeanCpp/CursorFinder | 52 | C++/Qt, GPL-2.0 | 透明窗口叠加 | 动画，近似连续 | 已适配（v1.2.1 修复扩展屏不触发） | 只看主屏 | Qt 处理，未针对每屏校准 | 单人、无 CI/测试、2024-12 停更 |
| 0cch/ShakeToFindCursor | 30 | C++, MIT | SetSystemCursor | 否，固定 3x | 天然支持 | 未见 | 系统 DPI aware，非 PerMonitorV2 | 活跃、CI、3 人；无单元测试 |
| Nitro1231/Shake-to-Find-Cursor | 6 | C#, MIT | 绘制/叠加 | 否 | 未知 | 无 | 无 | 2020 年弃坑 |
| bwya77/Windows-shake-to-find-cursor | 4 | C#, 无 LICENSE | SetSystemCursor | 是，能量映射 | 天然支持 | 无 | 固定 32px 基准 | 有 release CI、无测试、无许可证 |
| JorgeLBJ/giant-cursor | 1 | Go, MIT | SetSystemCursor + 游戏 overlay | 是 | 天然支持 | 无 | 读取 CursorBaseSize | 分层好、有单测、很新 |
| yfaj/shake-to-find-cursor | 1 | Rust, MIT | SetSystemCursor | 是 | 天然支持 | 仅前台窗口、全局判断 | 系统 DPI aware | 单 exe、无 CI/测试、很新 |

### 2.2 可直接借鉴的部分

- **0cch**：单 exe、系统光标替换、托盘/开机自启、轮询模式；证明“轮询 + SetSystemCursor”足够轻量可靠。
- **bwya77**：用“晃动能量 → 目标尺寸”的连续映射，手感最接近 macOS；证明不需要 overlay 也能做出连续放大。
- **giant-cursor**：`Crisp arrow` 高分辨率素材、2x–8x 可调、游戏 overlay 兜底；工程结构和测试值得借鉴。
- **yfaj**：从用户当前 `.cur` 主题文件按目标尺寸加载，尊重用户光标主题；设置界面和单实例实现可参考。
- **MrBean**：叠加窗口跟随光标所在屏，`QGuiApplication::screenAt()` 的思路；但它也暴露了 overlay 在多屏/混合 DPI 下的坐标和遮挡问题。
- **nowonderwhyy**：通过 User32 修改原生光标流，连续缩放，不画 overlay；验证了系统光标路线的可行性。

### 2.3 现有项目的共同缺口

1. **没有“每屏全屏”判断**：现有实现要么完全不判断全屏，要么只看前台窗口/主屏。没有项目做到“A 屏全屏、B 屏正常，光标在 B 屏时仍可用”。
2. **没有每屏 DPI 校准**：多数用 `GetSystemMetrics(SM_CXCURSOR)`（系统 DPI）或固定 32px，在 4K 200% 屏上放大后的视觉大小不一致，甚至发虚。
3. **连续放大与“找到后缩回”缺少清晰的状态机**：有的只有固定倍率，有的缩回时机不可配置。
4. **崩溃安全不足**：系统光标被替换后进程被杀，可能留下大号光标；多数项目只有一个 `--restore` 或退出时恢复。
5. **许可证/体积/维护性**：星最高的项目停更且 GPL；手感最好的项目没有 LICENSE；工程化最好的项目只有 1 星。
6. **资源占用和“无感”程度差异大**：Qt overlay 常驻内存明显高于原生 Win32；.NET 自包含安装包 100MB+；多数项目没有针对空闲功耗、首启体验、静默安装做设计。

### 2.4 结论

技术路线采用 **系统光标替换（SetSystemCursor）为主**，不采用 overlay 作为主路径：

- Windows 只有一个全局系统光标，鼠标在哪块屏，放大后的光标就在哪块屏，天然适配多屏。
- 无窗口层级、无 DWM 合成、无全屏遮挡、无跟随延迟。
- 代价是“全局光标”不能只放大某一块屏；但我们只在光标所在屏触发，且每屏全屏状态决定是否允许触发，实际体验等价。
- 游戏内自绘光标的场景被全屏抑制覆盖；窗口化自绘光标作为第二阶段可选的 halo overlay。

### 2.5 macOS 原生实现与可复刻性

#### 2.5.1 设置路径与行为

当前 macOS（Apple 官方指南）：

- **苹果菜单 > 系统设置 > 无障碍 > 显示 > 指针**。
- 选项：摇动鼠标指针以定位、指针大小、指针轮廓颜色、指针填充颜色。
- 旧版 macOS 在 **系统偏好设置 > 无障碍 > 显示 > 光标**。

Apple 对“摇动鼠标指针以定位”的原文：

> “打开此选项以在触控板上快速移动手指或者快速移动鼠标时，让指针变得更大。”
>
> “Turn this option on to make the pointer larger when you quickly move your finger on the trackpad or quickly move the mouse.”

要点：

- 默认关闭，需要用户主动开启（辅助功能语义）。
- 系统级、全局，对所有 App 生效；没有每 App 配置。
- 放大基于用户设置的“指针大小”，不是固定倍率。
- 文案是“快速移动”，不是严格的左右晃动；具体检测算法未公开。
- 没有 overlay；是 WindowServer 合成层的光标缩放，指针形状和热区保持不变。
- 停止快速移动后自动恢复。

#### 2.5.2 底层策略（公开符号与开源实现）

- `SkyLight.framework` 私有 API：`SLSGetCursorScale` / `SLSSetCursorScale`；`CGSGetCursorScale` / `CGSSetCursorScale`（开源项目 MaCursor 使用）。
- 指针大小偏好：`com.apple.universalaccess` 域的 `UACursorScaleKey` / `mouseDriverCursorSize`，通过 `UACursorSetScale` 应用；公开实现把范围限制在 1–4。
- “摇动以定位”开关的偏好键未出现在 `libUAPreferences` 的导出符号中，属于 UniversalAccess/WindowServer 内部实现；没有公开 API。
- 结论：macOS 的“路径”是**合成器级全局光标缩放因子**。Windows 没有等价的公开 API（没有 `SetCursorScale`）。

#### 2.5.3 Windows 可复刻程度

| macOS 能力 | Windows 可复刻 | 实现方式 / 限制 |
|---|---|---|
| 全局开关、对所有 App 生效 | 是 | 自己实现全局开关；不改变 App 行为 |
| 快速移动（鼠标/触控板）触发 | 是 | Raw Input + 10Hz watchdog + 轮询兜底；触控板走轮询 |
| 指针临时变大、停止后恢复 | 是 | SetSystemCursor 预渲染帧 + 状态机 |
| 基于当前指针大小放大 | 是 | 读取 Windows 辅助功能光标大小/主题作为 base |
| 连续缩放 | 近似 | 11 档阶梯 + 非对称平滑 + ≤30Hz 换帧；不是合成器连续变换 |
| 保持形状/热区 | 是 | 读取用户 `.cur` 主题，按比例缩放热区 |
| 系统设置集成 | 部分 | 无法注入 Windows 设置页；用托盘设置 + `ms-settings:easeofaccess-mousepointer` 深链 |
| 缩放任一 App 光标 | 部分 | 只能替换标准系统光标；App 自绘光标不受影响 |
| 全屏也生效 | 不做 | 按需求，全屏屏禁用（有意差异） |
| 默认关闭（辅助功能语义） | 可选 | 专用工具默认开启；安装器可取消 |

**不能 1:1 复刻的根本原因：**

- macOS 的指针是 WindowServer 合成的一个图层，`SLSSetCursorScale` 改的是合成时的缩放因子，所以能连续、全局、保持图像质量。
- Windows 的系统光标是 GDI 位图句柄，没有“缩放因子”API；`SetSystemCursor` 只能换一张位图。要放大只能预渲染目标尺寸的位图，再整体替换。
- Windows 的辅助功能“鼠标指针大小”是持久设置，不是临时动画；直接改它会影响用户设置，不能用于抖动响应。
- 第三方 App 无法向 Windows 设置 > 辅助功能注入页面；只能提供自己的设置入口或 Control Panel applet。

**建议的复刻策略：**

1. 设置语义对齐 macOS：全局 per-user、默认值、持久化、指针大小作为基准、停止后恢复。
2. 视觉上用 SetSystemCursor 预渲染帧实现“接近连续”的缩放；不要用 overlay 做主路径。
3. 设置入口：托盘“设置 > 指针”；同页放“摇动放大”开关、“最大倍率”、“打开 Windows 指针大小/颜色设置”按钮（`ms-settings:easeofaccess-mousepointer`）。
4. 触控板：macOS 明确支持触控板快速移动；Windows Raw Input 可能收不到精密触控板的鼠标事件，必须保留 10Hz watchdog + 125Hz 轮询兜底。
5. 默认值：专用工具默认开启更符合“安装后无感”；如果追求 macOS 辅助功能的“默认关闭”，可在安装器里默认不勾选。
6. 触发阈值：Apple 文案是“快速移动”，可能比“3 次反转”更宽松；提供“macOS 手感 / 保守”两档灵敏度预设，实测后定默认值。

---

## 3. 技术选型

### 3.1 推荐栈

| 项 | 选择 | 理由 |
|---|---|---|
| 语言 | **C++20** | 直接调 Win32；无运行时；二进制最小；参考实现最多 |
| UI | 原生 Win32（托盘 + 设置对话框） | 不引入 Qt/Electron；体积可控 |
| 构建 | **CMake 3.24+**；本地 **LLVM-MinGW**，CI/可选 **MSVC 2022 Build Tools** | 当前机器无管理员，LLVM-MinGW 免安装；CMake 同时支持两种编译器 |
| SDK | Windows SDK 10.0.22621+ | PerMonitorV2、`GetSystemMetricsForDpi`、高精度定时器 |
| 测试 | **doctest**（单头文件）或 Catch2 | 纯逻辑单测；CTest 集成 |
| 配置 | JSON（`serde` 风格手写解析或 nlohmann/json） | 可读、可调试；单文件 |
| 日志 | 自写轻量 rotating log | 不引入大依赖 |
| 安装 | 绿色单 exe；可选 Inno Setup | 默认免安装 |
| 许可 | MIT | 便于复用和二次开发 |

> 如果后续更偏好 Rust：`windows-rs` 可以 1:1 映射本方案的所有模块，纯逻辑单测更容易；但需要 rustup + MSVC Build Tools。C# WinForms 开发最快，但自包含单文件 100MB+，不符合“小而精”。Go 也可行（giant-cursor 已证明），但 Win32 UI/COM 细节更啰嗦。

### 3.2 权限模型

- 清单使用 `asInvoker`，**默认不请求管理员权限**。
- Microsoft 文档对 `SetSystemCursor` 没有要求提权；0cch 要求管理员更可能是其自身实现/清单选择。
- 如果 `SetSystemCursor` 返回 `ERROR_ACCESS_DENIED`：托盘气泡提示“当前会话不允许修改系统光标，可尝试以管理员身份运行”，不自动弹 UAC。
- 开机自启用 `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`，不需要管理员。

### 3.3 兼容性目标

- Windows 10 1809（17763）及以上；Windows 11 全版本。
- x64 优先；arm64 作为第二阶段（代码层不做 x86 假设）。
- 不保证 Windows 7/8；如果必须支持，需要降级：无 PerMonitorV2、无高精度定时器、无 `GetSystemMetricsForDpi`，方案会复杂很多。

### 3.4 资源预算（硬指标）

| 指标 | 目标 | 主要手段 |
|---|---|---|
| 空闲 CPU | < 0.05%（4 核） | Raw Input 事件驱动 + 10Hz watchdog，无 125Hz 常驻轮询 |
| 晃动时 CPU | < 0.8% | 采样节流 250Hz、光标替换 ≤30Hz、帧缓存 |
| 常驻内存 | < 16MB | 固定缓冲、无 Qt/.NET 运行时、LRU 帧缓存 ≤8MB |
| GDI 对象 | < 50 | 帧缓存复用、恢复后释放 |
| 句柄 | < 100 | RAII、启动/退出审计 |
| 二进制 | < 1.5MB | 静态 CRT、LTO、/OPT:REF,ICF、无第三方大库 |
| 启动时间 | < 50ms | 无首启向导、延迟加载非关键模块 |
| 安装后体积 | < 2MB | 单 exe + 图标，无运行时 |
| 网络 | 0 请求 | 无遥测、无自动更新（手动检查可选） |

> 关键取舍：**不用 125Hz 常驻轮询作为默认路径**。鼠标不动时 Raw Input 不产生事件，CPU 接近 0；只有触摸板/RDP 等拿不到 Raw Input 的场景才降级到 125Hz 轮询。相比“始终 125Hz 轮询”，空闲唤醒次数从每秒 125 次降到每秒 10 次（watchdog），对笔记本续航更友好。

### 3.5 系统与屏幕分辨率适配矩阵

| 维度 | 支持范围 | 适配策略 |
|---|---|---|
| 操作系统 | Windows 10 1809 / 21H2 / 22H2；Windows 11 22H2 / 23H2 / 24H2+ | `RtlGetVersion` 运行时检测；缺失 API 时按能力降级 |
| 架构 | x64（首发）；arm64（第二阶段） | 纯 Win32，无 x86 假设；CI 增加 arm64 构建 |
| 分辨率 | 1080p–8K | 物理像素坐标；目标光标上限 512px；高分辨率帧优先 |
| 缩放 | 100%–300% 混合 DPI | PerMonitorV2；`GetDpiForMonitor` + `GetSystemMetricsForDpi` |
| 多屏 | 1–4 屏，主屏任意，热插拔 | 每次跨屏重算 DPI/帧；`WM_DISPLAYCHANGE` 重建缓存 |
| 光标大小 | 辅助功能 1–15 级 | 以当前屏正常光标大小为基准，不覆盖用户的放大设置 |
| 光标主题 | 系统 `.cur` / `.ani` / 高对比度 | 读取注册表；`.ani` 首帧静态；高对比度保留形状/颜色 |
| 远程/虚拟 | RDP、Hyper-V/VMware、虚拟桌面 | Raw Input 不可用时切轮询；不依赖硬件 GPU |
| 会话 | 锁屏、切换用户、睡眠/唤醒 | 会话通知恢复；唤醒后重建显示器缓存 |
| 全屏 | 独占全屏、无边框全屏、F11、演示模式 | 每屏判定；全屏屏抑制，非全屏屏正常 |
| 失败降级 | API 缺失/权限不足/主题损坏 | 逐项降级；不崩溃；托盘提示一次错误 |

**8K / 300% 的特别处理：**

- 目标光标像素上限 512×512，避免 GDI 位图过大和内存暴涨。
- 优先从 `.cur` 的高分辨率帧加载；没有高分辨率帧时，使用内置 Arrow/Hand 素材缩小，而不是把 32px 放大。
- 帧缓存按 8MB 硬上限 LRU；8K 下只保留当前倍率 ±1 档，避免缓存抖动。
- 多屏拼接后的虚拟桌面坐标可能为负或超出 16 位有符号范围，内部统一用 `int32_t`/`LONG`，不要用 `short`。

### 3.6 已验证的本地开发环境（2026-11-02）

当前机器无管理员权限、无 winget/VS Installer，因此采用按用户目录的免安装工具链：

| 工具 | 版本 | 路径 |
|---|---|---|
| LLVM-MinGW（UCRT, x86_64） | LLVM 23.1.4 | `%LOCALAPPDATA%\toolchains\llvm-mingw-20261020-ucrt-x86_64` |
| CMake | 4.4.5 | `%LOCALAPPDATA%\toolchains\cmake-4.4.5-windows-x86_64` |
| Ninja | 1.13.2 | `%LOCALAPPDATA%\toolchains\ninja-1.13.2` |

- 已加入用户 PATH；项目内也提供 `tools/env.sh`（Git Bash）、`tools/env.ps1`（PowerShell）、`tools/env.cmd`（cmd.exe）。
- 已验证：C++20 concepts/span、Win32 GUI 子系统、PerMonitorV2 manifest、`windres`、静态链接、`GetDpiForMonitor`、`GetSystemMetricsForDpi`、`DwmGetWindowAttribute`、`SHQueryUserNotificationState`、Raw Input、WTS、注册表、COM 符号。
- 冒烟测试：`tools/toolchain-check/`，构建方式见该目录 README。
- 当前机器实际有两块显示器（主屏 1920×1200 @96 DPI，左侧扩展屏 2560×1440 @96 DPI），可直接做多屏测试；混合 DPI 仍需调整缩放或外接验证。
- CI 仍建议用 `windows-2022` + MSVC 做一次编译矩阵；本地用 LLVM-MinGW 快速迭代。

---

## 4. 总体架构

### 4.1 线程模型

```
┌─────────────────────────────────────────────────────────────┐
│ UI 线程（消息循环）                                          │
│  - 托盘图标 / 菜单 / 设置对话框                              │
│  - SetWinEventHook(EVENT_SYSTEM_FOREGROUND)                  │
│  - WM_DISPLAYCHANGE / WM_SETTINGCHANGE / 会话通知            │
│  - 接收控制器事件，更新托盘状态                              │
└───────────────▲─────────────────────────────────────────────┘
                │ PostMessage / 回调
┌───────────────┴─────────────────────────────────────────────┐
│ 控制器线程（worker）                                         │
│  - 消费采样队列                                              │
│  - ShakeDetector → ScaleController → 状态机                  │
│  - 决定是否/以多大尺寸调用 CursorEffector                    │
│  - 每 100–200ms 复查光标所在屏的全屏状态                     │
└───────────────▲─────────────────────────────────────────────┘
                │ lock-free SPSC ring buffer（或小 mutex 队列）
┌───────────────┴─────────────────────────────────────────────┐
│ 输入线程（高优先级，事件驱动）                               │
│  - Raw Input（RIDEV_INPUTSINK）消息窗口：移动/按键           │
│  - 10Hz watchdog：触摸板/RDP/无 Raw Input 时兜底             │
│  - 必要时降级到 125Hz 轮询；只采样，不做重操作               │
└─────────────────────────────────────────────────────────────┘
```

- 输入线程优先级：`THREAD_PRIORITY_HIGHEST`；不轻易用 `TIME_CRITICAL`，避免影响系统。
- 主路径：Raw Input（`RIDEV_INPUTSINK`），鼠标不动时零事件、零 CPU；移动时按设备上报频率接收，最多节流到 250Hz。
- 兜底：10Hz watchdog 检测“光标位置变了但没有 Raw Input 事件”（触摸板/RDP/远程桌面），自动切到 125Hz 轮询；连续 3 秒无移动再回落到 watchdog。
- 定时器：仅在轮询兜底时使用 `CreateWaitableTimerExW(..., CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, ...)`（Win10 1803+）；失败退化为 `Sleep(8)`。不调用 `timeBeginPeriod`。
- 队列：固定容量 512 的 SPSC 环形队列，满时丢最旧样本；控制器批量消费。
- 所有 `SetSystemCursor` 调用集中在控制器线程，UI 线程只发命令。

### 4.2 数据流

```
GetPhysicalCursorPos ──▶ Sample{t,x,y,buttons}
                              │
                              ▼
                     ShakeDetector（纯逻辑）
                              │ energy 0..1, reversals, amplitude
                              ▼
                     ScaleController（纯逻辑）
                              │ target_scale, hold_until
                              ▼
                     StateMachine（纯逻辑）
                              │ apply(scale) / restore()
                              ▼
              CursorRenderer（缓存帧） ──▶ CursorEffector（SetSystemCursor）
                              ▲
              MonitorService / FullscreenService（每屏状态）
```

### 4.3 模块职责

| 模块 | 职责 | 是否可单测 |
|---|---|---|
| `core/shake_detector` | 从采样序列计算晃动能量、方向反转、幅度、速度 | 是（纯逻辑） |
| `core/scale_controller` | 能量 → 目标倍率、平滑、保持、缩回 | 是 |
| `core/state_machine` | Idle/Shaking/Holding/Shrinking/Suppressed | 是 |
| `platform/pointer_source` | Raw Input 事件驱动、轮询兜底、watchdog、坐标/按键 | 否（薄封装） |
| `platform/cursor_theme` | 读取注册表主题、映射 OCR ID | 部分 |
| `platform/cursor_renderer` | 加载/缩放/热区/帧缓存 | 部分（尺寸/热区计算可单测） |
| `platform/cursor_effector` | 应用/恢复系统光标、脏标记 | 否 |
| `platform/monitors` | 枚举显示器、DPI、光标所在屏 | 几何计算可单测 |
| `platform/fullscreen` | 每屏全屏判定 | 几何/规则可单测 |
| `lifecycle/crash_guard` | 脏标记、异常处理、`--restore` | 部分 |
| `ui/tray`, `ui/settings` | 托盘、设置 | 手动 |
| `config` | 读写 JSON、默认值、校验 | 是 |

---

## 5. 关键设计

### 5.1 指针采样（Raw Input 优先 + 轮询兜底）

#### 5.1.1 目标与模式

采样必须同时满足：

- 空闲时几乎不耗电：不能每秒唤醒 125 次。
- 晃动时足够密：至少 125Hz，最好 250Hz。
- 对鼠标、触摸板、远程桌面都可用。
- 不注入、不 Hook 输入链。

因此采用三档自适应：

| 模式 | 触发条件 | 频率 | 用途 |
|---|---|---|---|
| `raw` | Raw Input 注册成功且检测到鼠标设备 | 事件驱动，节流 ≤250Hz | 默认主路径 |
| `poll` | Raw Input 不可用，或 watchdog 发现触摸板/RDP 移动 | 125Hz | 兜底 |
| `watchdog` | 空闲/静止 | 10Hz | 低成本发现“位置变了” |

配置项 `sampling_mode`：`auto`（默认）/ `rawinput` / `polling`。

#### 5.1.2 Raw Input 主路径

- 创建一个 `HWND_MESSAGE` 消息窗口，`RegisterRawInputDevices` 注册 `usUsagePage=1, usUsage=2`（鼠标），`dwFlags=RIDEV_INPUTSINK`，这样应用不在前台也能收到 `WM_INPUT`。
- `WM_INPUT` 中 `GetRawInputData` 取 `RAWMOUSE`：
  - `usFlags & MOUSE_MOVE_RELATIVE`：相对移动；
  - `usButtonFlags`：按下/抬起；
  - 每次事件取 `QueryPerformanceCounter` 时间戳。
- 为了拿到**绝对屏幕坐标**（用于幅度、跨屏、DPI），在事件到来时节流读取 `GetPhysicalCursorPos`：距上次采样 ≥4ms 才读一次，把设备 1000Hz 上报压到 ≤250Hz。
- 按键状态同时用 `GetAsyncKeyState` 兜底（某些远程桌面/注入输入不产生标准 Raw Input 按钮标志）。
- Raw Input 的优点：
  - 鼠标不动 → 没有 `WM_INPUT` → 零 CPU、零唤醒。
  - 不经过系统输入链，不会拖慢其他应用。
  - 不受 `WH_MOUSE_LL` 被系统摘掉的影响。

#### 5.1.3 轮询兜底

- 当 `RegisterRawInputDevices` 失败、`GetRawInputDeviceList` 没有鼠标设备、或 watchdog 判定 Raw Input 不可用时，启动 125Hz 采样：
  - `CreateWaitableTimerExW` + `CREATE_WAITABLE_TIMER_HIGH_RESOLUTION`，周期 8ms；失败用 `Sleep(8)`。
  - 每次 `GetPhysicalCursorPos` + `GetAsyncKeyState` + QPC。
- 轮询只作为兜底，不默认常驻。

#### 5.1.4 10Hz watchdog

- 始终运行一个 10Hz（100ms）的低成本定时器，只做一件事：
  - 读 `GetPhysicalCursorPos`，与上次位置比较。
  - 如果位置变化、但最近 500ms 没有 Raw Input 事件 → 判定“当前设备不是标准鼠标”（触摸板/RDP/注入），临时切到 125Hz 轮询 3 秒；期间继续观察。
- 10Hz 每秒 10 次唤醒，对续航影响远小于 125Hz；同时保证触摸板用户不会完全失效。
- watchdog 在锁屏/全屏抑制/用户禁用时暂停。

#### 5.1.5 采样数据与节流

```cpp
struct Sample {
    int64_t t_us;      // QueryPerformanceCounter 换算为微秒
    int32_t x, y;      // 物理屏幕坐标
    uint8_t buttons;   // bit0 L, bit1 R, bit2 M
    uint32_t dpi;      // 光标所在显示器的 effective DPI
};
```

- 坐标使用 `GetPhysicalCursorPos`，避免 PerMonitorV2 下 `GetCursorPos` 的坐标语义歧义；失败再退回 `GetCursorPos`。
- 显示器列表每秒缓存一次；光标所在屏用 `MonitorFromPoint` 每次采样计算（很便宜）；DPI 从缓存取。
- 检测器最多消费 250Hz；Raw Input 高于 250Hz 的部分在采样层合并（保留最新位置 + 时间戳）。
- 采样线程只写无锁队列；不做日志、不分配内存、不调用可能阻塞的 API。
- 光标不可见（`GetCursorInfo().flags == 0`）时，不采样、不放大。

#### 5.1.6 为什么不用低级鼠标 Hook

- `WH_MOUSE_LL` 回调运行在系统输入链路上，回调慢会影响全系统输入；高负载时可能被系统摘掉。
- Raw Input / 轮询都不注入、不拦截，对游戏反作弊更友好。
- 我们的全屏抑制意味着游戏场景本来就不启用，更没有必要承担 Hook 的风险。

### 5.2 晃动检测算法

目标：连续晃动 → 能量快速上升；普通快速移动/拖拽/瞬移 → 不触发。

#### 5.2.1 采样窗口与速度

- 保留最近 **600ms** 的样本（Raw Input 事件驱动，检测器最多 250Hz，约 150 个点）。
- 用相邻样本计算瞬时速度：`v = Δp / Δt`，丢弃 `Δt < 2ms` 或 `> 40ms` 的异常点。
- 速度、幅度都以 **DIP（96 DPI 逻辑像素）** 为单位，按光标所在屏的 DPI 归一化：
  - `dip_per_px = 96.0 / dpi`
  - 这样在 4K 200% 屏上，同样的“手感”触发阈值一致。

#### 5.2.2 方向反转（任何方向，不只左右）

```text
v_ref = 最近一次“显著速度”向量（|v| >= min_speed_dip）
对当前显著速度 v_cur：
    angle = acos(dot(normalize(v_cur), normalize(v_ref)))
    if angle >= 120° 且 |v_cur| >= min_speed_dip:
        reversals += 1
        v_ref = v_cur
```

- 用 120° 而不是 90°，避免抖动/斜向移动被误判。
- 只统计“显著速度”样本，低速微抖不参与。
- 反转点之间要求有最小位移（默认 24 DIP），避免原地高频抖动。

#### 5.2.3 能量计算

在最近 600ms 窗口内统计：

```text
reversal_score = clamp((reversals - 1) / 2.0, 0, 1)      // 3 次反转达到 1
amplitude_dip  = 窗口内最大包围盒对角线（或主轴跨度）
amplitude_score = clamp(amplitude_dip / 48.0, 0, 1)
speed_score    = clamp(peak_speed_dip_s / 600.0, 0, 1)

raw_energy = reversal_score * amplitude_score * speed_score
```

- 触发条件：默认 `reversals >= 3`、`amplitude_dip >= 80`、`peak_speed >= 800 DIP/s`、`path_length >= 250 DIP`；同时 `net_displacement/path <= 0.50` 且 `net_displacement/amplitude <= 0.35`（来回晃动会回到原点附近，普通导航/锯齿移动不会）。
- 平滑：
  - 上升：`energy = energy + (raw - energy) * 0.35`（快速响应）
  - 下降：`energy = energy + (raw - energy) * 0.08`（缓慢释放，避免闪烁）
- 灵敏度设置 1–10 映射到 `min_amplitude`、`min_speed`、`required_reversals` 的缩放系数（高灵敏度 = 阈值降低）。

#### 5.2.4 误触发抑制

- **按键按下**：任何鼠标键按下 → `energy = 0`、状态机进入 Suppressed；松开后再恢复。拖拽/框选/画图不会触发。
- **瞬移**：单次位移 > 500 DIP 且 `Δt < 20ms`（`SetCursorPos`、RDP 切换、多屏跳转）→ reset 窗口。
- **静止**：连续 150ms 无显著移动 → 能量快速衰减。
- **全屏**：光标所在屏被全屏窗口占用 → 不采样能量（或采样但直接丢弃）。
- **设置窗口打开 / 用户禁用 / 会话锁定** → 丢弃。

#### 5.2.5 触发时序（验收）

- 满足反转条件后持续 **250ms** 激活，随后开始放大（总延迟约 250–400ms）。
- 持续晃动 **700ms 内**达到最大倍率。
- 停止晃动后保持 `hold_ms`（默认 800ms），然后开始缩回。
- 缩回过程 **300ms 内**完成并调用 `SPI_SETCURSORS`。

### 5.3 倍率控制

```text
target_scale = 1 + (max_scale - 1) * smoothstep(0.15, 0.95, energy)

if energy > 0.12:
    hold_until = now + hold_ms
    target_scale = max(target_scale, last_peak_scale)   // 不因轻微回落而抖动
else if now < hold_until:
    target_scale = last_peak_scale
else:
    target_scale = 1.0
```

- `max_scale` 默认 4.0，可配 2.0–8.0。
- `smoothstep` 避免小能量就跳到 2x。
- 实际应用倍率做指数平滑：
  - 放大：`tau = 60ms`
  - 缩小：`tau = 180ms`
- 预生成“倍率阶梯”（1.0, 1.25, 1.5, 1.75, 2.0, 2.5, 3.0, 4.0, 5.0, 6.0, 8.0），动画只在阶梯之间切换，避免每次采样都渲染新帧。
- 输出量化到阶梯值，最多 **30Hz** 更新，减少 `SetSystemCursor` 调用次数；阶梯帧按当前 DPI 缓存，跨屏时重建。
- 如果系统开启“减少动态效果”（`SystemParametersInfo(SPI_GETCLIENTAREAANIMATION)` 或“动画效果”设置），跳过平滑动画，直接切到目标倍率，减少视觉干扰。
- **激活**：能量高于 `start_energy` 持续 250ms 才开始放大，避免单次快速移动误触发。
- **冷却**：恢复后 600ms 内忽略新的晃动，避免阈值边界反复 grow/shrink。
- 小于 1.10x 不应用（避免无意义的光标替换）。
- 每次应用前比较“量化后的目标像素尺寸 + 当前 DPI 桶”，相同则跳过。

> “找到后自动缩小”没有可靠的视觉信号，采用 macOS 同款策略：**晃动停止 + 保持时间**。另外提供“按下鼠标键立即缩回”和 `hold_ms` 可配置。

### 5.4 光标主题与渲染

#### 5.4.1 读取用户当前光标主题

从注册表读取（值名到 OCR ID 的映射）：

| 注册表值名 | OCR ID | 说明 |
|---|---|---|
| `Arrow` | `OCR_NORMAL` (32512) | 正常选择 |
| `IBeam` | `OCR_IBEAM` (32513) | 文本选择 |
| `Wait` | `OCR_WAIT` (32514) | 忙 |
| `Crosshair` | `OCR_CROSS` (32515) | 精确选择 |
| `UpArrow` | `OCR_UP` (32516) | 备选选择 |
| `SizeNWSE` | `OCR_SIZENWSE` (32642) | 对角调整 1 |
| `SizeNESW` | `OCR_SIZENESW` (32643) | 对角调整 2 |
| `SizeWE` | `OCR_SIZEWE` (32644) | 水平调整 |
| `SizeNS` | `OCR_SIZENS` (32645) | 垂直调整 |
| `SizeAll` | `OCR_SIZEALL` (32646) | 移动 |
| `No` | `OCR_NO` (32648) | 不可用 |
| `Hand` | `OCR_HAND` (32649) | 链接选择 |
| `AppStarting` | `OCR_APPSTARTING` (32650) | 后台工作 |
| `Help` | `OCR_HELP` (32651) | 帮助选择 |

- 路径为空或文件不存在 → 使用系统 stock cursor（`IDC_ARROW` 等）。
- 用 `RegNotifyChangeKeyValue` 监听 `HKCU\Control Panel\Cursors`，用户切换主题/改光标大小时自动重建缓存。

#### 5.4.2 基准尺寸

```text
dpi = GetDpiForMonitor(monitor_of(cursor), MDT_EFFECTIVE_DPI)
base_px = GetSystemMetricsForDpi(SM_CXCURSOR, dpi)   // Win10 1607+
if base_px <= 0: base_px = GetSystemMetrics(SM_CXCURSOR)
// 兼容用户通过辅助功能调大的光标：
if registry CursorBaseSize exists:
    base_px = max(base_px, CursorBaseSize)
target_px = clamp(round(base_px * scale), 8, 512)
```

- 每次光标跨屏/DPI 变化时，重新计算 `base_px`，从对应 DPI 的帧缓存取图。
- 这是与现有项目最大的差异之一：**放大倍率相对“当前屏的正常光标”**，而不是相对主屏/固定 32px。

#### 5.4.3 生成放大帧

优先级从高到低：

1. **从用户当前 `.cur` 文件按目标尺寸加载**：
   ```cpp
   HCURSOR h = (HCURSOR)LoadImageW(nullptr, path, IMAGE_CURSOR,
                                   target_w, target_h, LR_LOADFROMFILE);
   ```
   Windows 会从 `.cur` 内嵌的多尺寸帧中选择/缩放到目标尺寸。
2. **`CopyImage` 缩放**：
   ```cpp
   HCURSOR h = (HCURSOR)CopyImage(original, IMAGE_CURSOR,
                                  target_w, target_h, 0);
   ```
3. **高分辨率内置素材**（可选）：对 Arrow/Hand 使用项目自带的高分辨率箭头/手型素材（参考 giant-cursor 的 `Crisp arrow`），按目标尺寸缩小；这样即使系统主题只有 32px，也能保持锐利。
4. **失败兜底**：不替换该 OCR ID，记录日志。

细节：

- **热区（hotspot）必须按比例缩放**。如果走 `LoadImageW`，验证返回值的热区是否正确；如果走 `CopyImage`，用 `GetIconInfo` 取原热区，按 `target/original` 比例重建。
- **`.ani` 动画光标**：`LoadImageW` 可能只得到首帧。第一版策略：
  - Arrow/IBeam/Hand 等常用光标通常是 `.cur`，正常处理。
  - `.ani`（Wait/AppStarting）在放大期间使用首帧静态图；缩回后恢复系统动画。
  - 在 README 中明确此限制。
- **帧缓存与内存预算**：按 `(OCR ID, dpi 桶, 阶梯倍率)` 缓存 `HCURSOR`；LRU 总预算 **8MB**。默认只放大 `Arrow / IBeam / Hand / Crosshair` 四种最常用光标（`enlarge_cursor_set` 可配），避免 14 种光标 × 多档倍率把内存吃满；`enlarge_all_cursors=true` 才处理全部 OCR ID。
- **按需生成**：启动时只预生成当前 DPI 的阶梯；跨屏时生成新 DPI 的相邻帧，超出预算立即淘汰旧帧。应用时 `CopyIcon` 再传给 `SetSystemCursor`（函数会接管并销毁句柄）。
- **热路径零分配**：应用/恢复路径不创建 STL 容器、不拼字符串、不写日志（除非出错）。
- **透明度/反色**：优先保留 32-bit alpha；Windows 的反色光标（inverted cursor）在替换为彩色帧后可能失去反色效果，作为已知限制记录，后续可做单色掩码兼容。

#### 5.4.4 应用与恢复

```cpp
// 应用
for (id : ids) {
    HCURSOR copy = CopyIcon(cached[id]);
    if (!SetSystemCursor(copy, id)) {
        DestroyIcon(copy);
        log_error(...);
    }
}

// 恢复：重新加载用户光标方案
SystemParametersInfoW(SPI_SETCURSORS, 0, nullptr, SPIF_SENDCHANGE);
```

- 只对当前主题中存在的 OCR ID 应用；某个失败不影响其他。
- 应用前写脏标记（见 5.8），恢复成功后删除。
- 恢复后清空“当前已应用倍率”，避免状态不一致。

### 5.5 显示器与 DPI

- 枚举：`EnumDisplayMonitors` + `GetMonitorInfoW`，缓存 `HMONITOR`、设备名、`rcMonitor`、`rcWork`、主屏标志。
- DPI：`GetDpiForMonitor(hmon, MDT_EFFECTIVE_DPI, &dpiX, &dpiY)`；链接 `Shcore.lib`。
- 光标所在屏：`MonitorFromPoint(cursor_px, MONITOR_DEFAULTTONEAREST)`。
- 监听 `WM_DISPLAYCHANGE`、`WM_SETTINGCHANGE`（`SPI_SETWORKAREA`），重建显示器缓存。
- 缩放期间每 100ms 检查一次“光标所在屏/该屏 DPI”是否变化；变化则切换到对应 DPI 的帧。
- 坐标统一使用物理像素；如果需要与 UI 逻辑坐标互转，使用 `PhysicalToLogicalPointForPerMonitorDPI` / `LogicalToPhysicalPointForPerMonitorDPI`。

### 5.6 每屏全屏检测（核心差异点）

#### 5.6.1 判定对象

对光标所在屏 `M`，判断 `M` 是否被一个全屏窗口占用。

#### 5.6.2 数据来源

1. **`SHQueryUserNotificationState()`**（全局）：
   - `QUNS_RUNNING_D3D_FULL_SCREEN` (3)：独占全屏 D3D 应用。
   - `QUNS_BUSY` (2)：全屏应用或演示设置。
   - `QUNS_PRESENTATION_MODE` (4)：演示模式 → 全局抑制。
   - `QUNS_NOT_PRESENT` (1)：锁屏/屏保/非活动会话 → 恢复并全局抑制。
   - 这些状态是**全局**的，只能作为辅助信号，必须结合前台窗口所在屏做每屏判断。

2. **前台窗口几何**：
   - `GetForegroundWindow()`。
   - 跳过：`GetShellWindow()`、类名 `Progman`/`WorkerW`、本进程窗口、`IsWindowVisible == false`、`IsIconic == true`。
   - 边界优先 `DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS)`（屏幕坐标，排除不可见边框）；失败用 `GetWindowRect`。
   - `MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST)` 必须等于 `M`。
   - 跳过被 DWM cloak 的窗口：`DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED)` 非 0。

3. **z 序补充扫描**（可选，防止全屏窗口被小弹窗挡住）：
   - 从 `GetForegroundWindow()` 开始，用 `GetWindow(GW_HWNDPREV)`/`GetNextWindow` 向下遍历最多 8 个可见窗口。
   - 只检查与 `M` 相交且覆盖 `M` 的窗口。
   - 目的：全屏游戏/视频后面弹出一个通知/对话框时，仍能识别全屏应用。

#### 5.6.3 全屏规则

```text
covers_monitor(rect, M) =
    rect.left   <= M.rcMonitor.left   + 2 &&
    rect.top    <= M.rcMonitor.top    + 2 &&
    rect.right  >= M.rcMonitor.right  - 2 &&
    rect.bottom >= M.rcMonitor.bottom - 2

covers_work(rect, M) = 同理，与 rcWork 比较

style = GetWindowLong(GWL_STYLE)
popup_or_borderless = (style & WS_POPUP) || !(style & WS_CAPTION)

is_fullscreen_on(M) =
    window_on(M) &&
    covers_monitor &&
    (popup_or_borderless || !covers_work || quns_fullscreen_global)
```

- `quns_fullscreen_global = (QUNS_RUNNING_D3D_FULL_SCREEN || QUNS_BUSY)`。
- 该规则能覆盖：
  - 无边框全屏游戏/视频（覆盖 `rcMonitor`，无标题栏）。
  - 独占全屏 D3D（QUNS 为 3）。
  - F11 浏览器全屏（覆盖 `rcMonitor`）。
  - 普通最大化窗口：只覆盖 `rcWork`，有标题栏 → 不是全屏。
  - 任务栏自动隐藏的最大化窗口：覆盖 `rcMonitor` 和 `rcWork`，但有标题栏且 QUNS 通常不是 busy → 不是全屏。
- 误判兜底：设置里提供“全屏检测灵敏度”或进程白名单/黑名单：
  - 白名单：这些进程窗口中即使全屏也允许放大（例如你自己的演示工具）。
  - 黑名单：这些进程即使检测不到全屏也禁用（例如某些游戏启动器）。

#### 5.6.4 何时复查

- 前台窗口变化：`SetWinEventHook(EVENT_SYSTEM_FOREGROUND, ...)`（`WINEVENT_OUTOFCONTEXT`）。
- 最小化/恢复：`EVENT_SYSTEM_MINIMIZESTART/END`。
- 显示器变化：`WM_DISPLAYCHANGE`。
- 工作区变化：`WM_SETTINGCHANGE` + `SPI_SETWORKAREA`。
- 放大期间：每 150–200ms 轮询一次光标所在屏的全屏状态。
- 光标跨屏：采样线程检测到 `MonitorFromPoint` 变化时，立即发事件给控制器。

#### 5.6.5 抑制行为

| 场景 | 行为 |
|---|---|
| 光标在非全屏屏，晃动 | 正常放大 |
| 光标在全屏屏，晃动 | 不放大；检测器 reset |
| 放大中，光标移入全屏屏 | 100ms 内恢复原始光标 |
| 放大中，光标所在屏出现全屏窗口 | 100ms 内恢复 |
| 全屏在 A 屏，光标在 B 屏晃动 | B 屏正常放大 |
| 演示模式 / 锁屏 | 全局恢复并抑制 |
| 全屏窗口最小化 | 下一次复查后恢复可用 |

> 因为系统光标是全局的，光标从 B 屏移到 A 屏的瞬间，大号光标可能在全屏屏上出现一帧；100ms 内恢复。若全屏应用隐藏了系统光标，则不可见。这个短暂窗口是可接受的，且比 overlay 方案的层级问题更可靠。

### 5.7 状态机

```text
        ┌──────────┐   sample/energy>=start   ┌──────────┐
        │   Idle   │ ───────────────────────▶ │ Shaking  │
        └────┬─────┘                          └────┬─────┘
             │                                     │ energy 下降
             │ 全屏/按键/禁用                       ▼
             │                               ┌──────────┐
             │                               │ Holding  │
             │                               └────┬─────┘
             │                                    │ hold 到期 / 点击 / 进入全屏
             ▼                                    ▼
        ┌──────────┐   shrink 完成            ┌──────────┐
        │Suppressed│ ◀─────────────────────── │ Shrinking│
        └──────────┘                          └──────────┘
```

事件：

- `Sample(sample)`：采样。
- `ForegroundChanged` / `DisplayChanged` / `SessionLocked`。
- `SettingsChanged`。
- `Tick(now)`：用于 hold/动画/复查。
- `Exit`。

守卫（任一为真则 Suppressed）：

- 用户禁用。
- 设置对话框打开（可选；建议打开时暂停）。
- 鼠标键按下。
- 光标所在屏全屏。
- 演示模式/锁屏。
- 正在切换光标主题（缓存重建中）。

动作：

- `EnterShaking`：开始应用倍率。
- `EnterHolding`：保持当前峰值。
- `EnterShrinking`：按 `tau=180ms` 缩回；完成时 `SPI_SETCURSORS`。
- `EnterSuppressed`：立即恢复（若已放大），重置检测器。
- `LeaveSuppressed`：回到 Idle。

### 5.8 生命周期与崩溃安全

- **单实例**：`CreateMutexW(nullptr, TRUE, L"Local\\CursorBeacon.Singleton")`；第二个实例通过 `FindWindow`/自定义消息唤起设置窗口后退出。
- **脏标记**：`%LOCALAPPDATA%\CursorBeacon\restore.needed`。
  - 第一次调用 `SetSystemCursor` 前写入（包含 PID、时间、当前倍率）。
  - `SPI_SETCURSORS` 成功后删除。
  - 启动时若发现脏标记且拿到单实例锁：先恢复光标，再删除标记，然后正常启动。
- **异常处理**：
  - `SetUnhandledExceptionFilter`
  - `std::set_terminate`
  - `std::atexit`
  - 托盘“恢复光标”菜单
  - CLI `--restore`
  - 以上路径都只做“尽力恢复”，不做复杂操作。
- **会话通知**：`WTSRegisterSessionNotification` + `WM_WTSSESSION_CHANGE`；锁屏/注销/切换用户前恢复。
- **睡眠/唤醒**：`WM_POWERBROADCAST`；唤醒后重建显示器/DPI 缓存。
- **主题变化**：`RegNotifyChangeKeyValue` 监听 `HKCU\Control Panel\Cursors`；变化时重建帧缓存，若当前已放大则重新应用。

> 第一版不做独立 watchdog 进程；靠脏标记 + 启动恢复 + `--restore` 覆盖 99% 场景。第二阶段可加一个极小的 watchdog 子进程，用命名事件监控主进程退出。

### 5.9 配置、托盘、设置界面

#### 配置 schema

```json
{
  "version": 1,
  "enabled": true,
  "max_scale": 4.0,
  "sensitivity": 5,
  "hold_ms": 800,
  "grow_tau_ms": 60,
  "shrink_tau_ms": 180,
  "max_update_hz": 30,
  "min_apply_scale": 1.10,
  "sampling_mode": "auto",
  "enlarge_cursor_set": ["arrow", "ibeam", "hand", "crosshair"],
  "enlarge_all_cursors": false,
  "max_frame_cache_mb": 8,
  "eco_watchdog_hz": 10,
  "follow_reduce_motion": true,
  "pause_on_fullscreen": true,
  "pause_on_click": true,
  "start_with_windows": true,
  "show_first_run_tip": false,
  "portable": false,
  "fullscreen_allowlist": [],
  "fullscreen_denylist": [],
  "log_level": "info"
}
```

- `sampling_mode`：`auto`（默认）/ `rawinput` / `polling`。
- `enlarge_cursor_set`：默认只放大 4 种常用光标，兼顾覆盖率和内存；也可直接设为 `"all"`。
- `max_frame_cache_mb`：帧缓存硬上限，超过后 LRU 淘汰。
- `follow_reduce_motion`：跟随系统“减少动态效果”，开启时跳过平滑动画。
- `show_first_run_tip`：默认 `false`，安装后完全无感。
- `portable`：配置放 exe 同目录，不写 `%APPDATA%`。
- 路径：`%APPDATA%\CursorBeacon\config.json`（portable 模式为 exe 同目录）。
- 读取失败/字段缺失 → 使用默认值并写回；未知字段保留（向前兼容）。
- 修改后立即生效（重建检测器/控制器参数，不需要重启）。

#### 托盘菜单

- 启用 / 暂停（勾选）。
- 设置…
- 开机启动（勾选）。
- 恢复光标（急救）。
- 关于 / 版本。
- 退出。

#### 设置对话框

原生 Win32 控件（不做自绘 UI）：

- 最大放大倍数：滑块 2x–8x，旁边显示“当前屏约 XX px”。
- 灵敏度：1–10。
- 保持时间：0–2000ms。
- 全屏时暂停：复选框（默认开）。
- 按键时立即缩回：复选框（默认开）。
- 跟随系统“减少动态效果”：复选框（默认开）。
- 放大范围：常用光标（Arrow/IBeam/Hand/Crosshair）/ 全部光标（下拉，默认常用）。
- 开机启动：复选框。
- 测试按钮：模拟 1 秒晃动，预览放大效果。
- 恢复光标按钮。
- 语言：简体中文 / English（第一版可只做中文，字符串资源化）。

#### 开机启动

- `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`，值为 `"C:\path\CursorBeacon.exe" --autostart`。
- 启动参数 `--autostart` 不弹任何窗口，只进托盘。

#### 安装与首次运行（无感）

- 按用户安装到 `%LOCALAPPDATA%\Programs\CursorBeacon`，安装器 `PrivilegesRequired=lowest`，不弹 UAC。
- 默认勾选“开机启动”，开始菜单创建快捷方式，不创建桌面快捷方式。
- 首次启动：不弹向导、不弹成功提示、不写“欢迎”页；托盘图标直接出现。
- 首次使用提示默认关闭（`show_first_run_tip=false`）；需要时可在设置里打开一次性气泡。
- 卸载：先恢复系统光标，再删除 `HKCU\...\Run` 和安装目录；询问是否保留 `%APPDATA%\CursorBeacon\config.json`。
- 提供 portable zip：`CursorBeacon.exe --portable`，配置放 exe 同目录，不写注册表（读取光标主题除外）。
- 无自动更新、无遥测、无网络请求；关于页可手动“检查更新”（可选，第二阶段）。

### 5.10 日志与诊断

- 日志：`%LOCALAPPDATA%\CursorBeacon\logs\cursorbeacon.log`，1MB × 3 轮转。
- 记录：启动版本、显示器列表/DPI、光标跨屏、全屏判定结果、状态迁移、SetSystemCursor 错误、恢复结果。
- 不记录鼠标轨迹全文（隐私）；只记录统计值。
- CLI：
  - `--restore`：恢复光标后退出。
  - `--dump-monitors`：打印所有显示器名称、rcMonitor、rcWork、DPI、主屏；当前光标位置、所在屏；前台窗口标题/类名/进程名/是否全屏；光标主题路径和基准尺寸。
  - `--simulate-shake`：向检测器注入合成轨迹，预览放大（不移动真实鼠标）。
  - `--stats`：运行 60 秒后打印 CPU、内存、GDI、句柄、采样次数、光标替换次数、Raw Input/轮询模式，用于功耗和泄漏验收。
  - `--scale N`、`--sensitivity N`、`--config PATH`。
  - `--verbose`。

### 5.11 “无感”体验设计原则

1. **不打断**：安装后不弹向导、不弹成功提示；托盘图标是唯一入口。只有出错时才提示一次。
2. **不添乱**：普通快速移动、拖拽、框选、滚动、触摸板双指操作都不触发；只有明确的三次以上来回晃动才放大。
3. **可预期**：停止晃动 800ms 后缩回；点击立即缩回；进入全屏立即缩回；`--restore` 永远可用。
4. **不改变习惯**：保留用户光标主题、形状、热区、辅助功能光标大小；不接管点击/命中测试，放大只影响视觉。
5. **不打扰视觉**：倍率阶梯平滑过渡；跟随“减少动态效果”；不闪烁、不跳位、不遮挡焦点。
6. **零配置可用**：默认 4x、800ms、全屏暂停、点击缩回、开机启动（安装器可取消），开箱即用。
7. **零维护**：主题变化自动跟随；崩溃/锁屏/退出自动恢复；无自动更新、无遥测、无网络。
8. **可发现**：托盘 tooltip 写明“晃动鼠标放大光标”；设置里有“测试”按钮；README 有 10 秒上手 GIF。
9. **可关闭**：托盘一键暂停；设置里可禁用；卸载时恢复光标并清理自启。
10. **低存在感**：空闲 CPU <0.05%、内存 <16MB、无后台窗口、无服务、无驱动。

---

## 6. 项目结构

```text
CursorBeacon/
├─ CMakeLists.txt
├─ CMakePresets.json
├─ LICENSE
├─ README.md
├─ docs/
│  ├─ DESIGN.md              # 本文件
│  ├─ TESTING.md             # 测试矩阵与手动步骤
│  └─ COMPATIBILITY.md       # 已知限制（.ani、反色光标、RDP）
├─ src/
│  ├─ main.cpp
│  ├─ app/
│  │  ├─ application.h/.cpp          # 生命周期、线程、消息循环
│  │  └─ controller.h/.cpp           # 状态机 + 采样消费 + 定时复查
│  ├─ core/
│  │  ├─ types.h                     # Sample/Vec2/Scale/Config
│  │  ├─ shake_detector.h/.cpp       # 纯逻辑
│  │  ├─ scale_controller.h/.cpp     # 纯逻辑
│  │  └─ state_machine.h/.cpp        # 纯逻辑
│  ├─ platform/
│  │  ├─ pointer_source.h/.cpp       # Raw Input + 轮询兜底 + 10Hz watchdog
│  │  ├─ raw_input.h/.cpp            # 消息窗口、WM_INPUT、设备注册
│  │  ├─ cursor_theme.h/.cpp         # 注册表主题、OCR 映射
│  │  ├─ cursor_renderer.h/.cpp      # 帧生成、热区、缓存
│  │  ├─ cursor_effector.h/.cpp      # SetSystemCursor / SPI_SETCURSORS
│  │  ├─ monitors.h/.cpp             # EnumDisplayMonitors / DPI
│  │  ├─ fullscreen.h/.cpp           # 每屏全屏判定
│  │  └─ dpi.h/.cpp                  # DPI 工具
│  ├─ lifecycle/
│  │  ├─ single_instance.h/.cpp
│  │  ├─ startup.h/.cpp
│  │  ├─ crash_guard.h/.cpp          # 脏标记、异常处理、--restore
│  │  └─ session.h/.cpp              # WTS 会话通知
│  ├─ ui/
│  │  ├─ tray.h/.cpp
│  │  └─ settings_dialog.h/.cpp
│  ├─ config/
│  │  └─ config.h/.cpp
│  └─ util/
│     ├─ log.h/.cpp
│     ├─ registry.h/.cpp
│     └─ strings.h/.cpp
├─ resources/
│  ├─ app.rc
│  ├─ app.manifest                  # PerMonitorV2 + asInvoker
│  └─ icons/
├─ tests/
│  ├─ test_shake_detector.cpp
│  ├─ test_scale_controller.cpp
│  ├─ test_fullscreen_rules.cpp
│  ├─ test_config.cpp
│  └─ test_main.cpp
├─ third_party/
│  └─ doctest/doctest.h
└─ .github/workflows/build.yml
```

### 清单文件（关键片段）

```xml
<assembly manifestVersion="1.0" xmlns="urn:schemas-microsoft-com:asm.v1">
  <application xmlns="urn:schemas-microsoft-com:asm.v3">
    <windowsSettings>
      <dpiAware xmlns="http://schemas.microsoft.com/SMI/2005/WindowsSettings">true/pm</dpiAware>
      <dpiAwareness xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">PerMonitorV2, PerMonitor</dpiAwareness>
    </windowsSettings>
  </application>
  <trustInfo xmlns="urn:schemas-microsoft-com:asm.v3">
    <security>
      <requestedPrivileges>
        <requestedExecutionLevel level="asInvoker" uiAccess="false"/>
      </requestedPrivileges>
    </security>
  </trustInfo>
</assembly>
```

### CMake 关键配置

```cmake
cmake_minimum_required(VERSION 3.24)
project(CursorBeacon LANGUAGES CXX)
enable_language(RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(CursorBeacon WIN32 ${SOURCES} resources/app.rc)
target_compile_definitions(CursorBeacon PRIVATE
    _WIN32_WINNT=0x0A00 WINVER=0x0A00 NTDDI_VERSION=0x0A000000)
target_link_libraries(CursorBeacon PRIVATE
    user32 gdi32 shcore dwmapi shell32 ole32 wtsapi32 advapi32)

if(MSVC)
    target_compile_options(CursorBeacon PRIVATE /W4 /permissive- /utf-8 /EHsc)
    set_property(TARGET CursorBeacon PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    target_compile_options(CursorBeacon PRIVATE -Wall -Wextra -Wpedantic)
    target_link_options(CursorBeacon PRIVATE -static)
endif()

option(CURSORBEACON_BUILD_TESTS "Build tests" ON)
if(CURSORBEACON_BUILD_TESTS)
    enable_testing()
    add_executable(cursorbeacon_tests ${TEST_SOURCES})
    target_include_directories(cursorbeacon_tests PRIVATE third_party/doctest)
    add_test(NAME unit COMMAND cursorbeacon_tests)
endif()
```

本地 LLVM-MinGW 构建命令（Git Bash）：

```bash
source tools/env.sh
cmake -S . -B build -G Ninja \
  -DCMAKE_CXX_COMPILER="$CXX" \
  -DCMAKE_RC_COMPILER="$(cygpath -m "$LLVM_MINGW")/bin/windres.exe" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

> `CMAKE_RC_COMPILER` 必须用正斜杠路径（`cygpath -m`），否则 CMake 会把 Windows 反斜杠当成转义字符。MSVC 构建时把编译器换成 `cl.exe`，并省略 `-static`。

---

## 7. 实施里程碑

> 估算按 1 名熟悉 Win32 的开发者、每天 6 小时有效编码计算。

### M0：骨架与安全网（0.5–1 天）

- 创建 CMake 工程、manifest、日志、配置加载、单实例。
- `--restore`、`--dump-monitors` 占位。
- 脏标记与启动恢复。
- 验收：能编译出单 exe；`--restore` 可调用 `SPI_SETCURSORS`；`--dump-monitors` 正确输出。

### M1：采样 + 检测 + 倍率（2–3 天）

- `PointerSource`：Raw Input 消息窗口 + 轮询兜底 + 10Hz watchdog；物理坐标、按键、DPI。
- `ShakeDetector` + `ScaleController` + `StateMachine`（纯逻辑）。
- 单元测试覆盖 5.2 / 5.3 的所有场景。
- 用一个 mock effector 打印状态迁移，不真正改光标。
- 验收：合成轨迹测试全绿；真实鼠标晃动时日志显示 energy/scale 符合预期；空闲 60 分钟 CPU <0.05%。

### M2：光标渲染与应用（2–3 天）

- `CursorTheme`：注册表路径、OCR 映射、主题变更监听。
- `CursorRenderer`：目标尺寸、`LoadImageW`/`CopyImage`、热区、倍率阶梯、LRU 帧缓存（≤8MB）。
- `CursorEffector`：应用/恢复/脏标记/错误处理。
- 默认只放大 Arrow/IBeam/Hand/Crosshair；`enlarge_all_cursors` 作为可选项。
- 单屏实机测试：放大与恢复；崩溃/杀进程后重启恢复；8 小时 GDI/句柄无增长。
- 验收：放大后形状不变、热区不偏；`--restore` 100% 恢复；连续 50 次强制结束进程后下次启动能恢复；内存 <16MB。

### M3：多屏 + DPI + 每屏全屏（2 天）

- `Monitors`：枚举、缓存、光标所在屏、跨屏事件。
- 每屏 DPI 帧选择；跨屏时热切换。
- `Fullscreen`：QUNS + 前台窗口 + 几何 + z 序补充；每屏判定。
- 抑制/恢复逻辑；跨屏进入全屏屏立即恢复。
- 多屏手动测试矩阵（见第 8 节）。
- 验收：2 屏/3 屏、100%/150%/200% 混合 DPI 下，缩放大小一致；A 屏全屏时 B 屏仍可用；移入 A 屏 100ms 内恢复。

### M4：托盘 + 设置 + 开机启动（1–2 天）

- 托盘菜单、设置对话框、配置持久化、开机自启。
- 测试按钮、恢复按钮。
- 中英文资源（可先中文）。
- 验收：所有设置即时生效；重启后保留；开机启动无窗口弹出。

### M5：打磨与发布（2 天）

- 日志完善、README、GIF/截图、兼容性文档。
- CI（windows-2022）：Debug/Release 构建 + ctest。
- 静默按用户安装包（Inno Setup，`PrivilegesRequired=lowest`，默认开机启动）、portable zip、可选代码签名、SHA256。
- 功耗/内存 8 小时浸泡测试；`powercfg /energy` 检查无异常定时器。
- 验收：干净 Windows 10/11 虚机可运行；无运行时依赖；安装/卸载无 UAC、无首启窗口；杀软误报说明。

**总计：约 9–13 个工作日。**

---

## 8. 测试计划

### 8.1 单元测试（必须自动化）

| 编号 | 场景 | 期望 |
|---|---|---|
| S01 | 鼠标静止 2s | 不触发，energy=0 |
| S02 | 直线快速移动 1000 DIP/s | 不触发（无反转） |
| S03 | 3 次反转、幅度 100 DIP、速度 600 DIP/s | 触发，scale 单调上升 |
| S04 | 2 次反转 | 不触发 |
| S05 | 小幅高频抖动（幅度 8 DIP） | 不触发 |
| S06 | 慢速大幅晃动（速度 150 DIP/s） | 低灵敏度不触发；高灵敏度可触发 |
| S07 | 按住左键晃动 | Suppressed，不放大 |
| S08 | `SetCursorPos` 瞬移 800 DIP | 窗口 reset，不触发 |
| S09 | 晃动停止 | hold_ms 后开始缩回，300ms 内完成 |
| S10 | 缩回中再次晃动 | 立即重新放大，不抖动 |
| S11 | 同一轨迹在 96/144/192 DPI | 触发阈值一致（DIP 归一化） |
| S12 | 96 DPI 下 3x，144 DPI 下 3x | target_px 分别为 3×base96、3×base144 |
| F01 | 最大化窗口覆盖 rcWork、有标题栏 | 不是全屏 |
| F02 | 无边框窗口覆盖 rcMonitor | 是全屏 |
| F03 | 全屏在 A 屏，光标在 B 屏 | B 屏允许 |
| F04 | 光标从 B 屏移到 A 屏 | 立即 Suppressed |
| F05 | 演示模式 | 全局 Suppressed |
| F06 | 锁屏 | 全局 Suppressed + 恢复 |
| C01 | 配置读写/缺字段/未知字段 | 默认值 + 向前兼容 |
| C02 | 量化倍率边界 1.0/1.1/8.0/9.0 | 正确 clamp |
| C03 | 热区缩放 | 目标热区 = 原热区 × 目标尺寸/原尺寸（四舍五入） |

### 8.2 手动测试矩阵

**显示器：**

- 单屏 1080p 100%。
- 单屏 4K 200%。
- 双屏：1080p 100% + 4K 200%，主屏分别为两者。
- 三屏：4K 150% + 2K 100% + 1080p 100%。
- 显示器热插拔、分辨率/缩放改变。
- 光标跨屏时正在放大。

**全屏/应用：**

- 浏览器 F11 全屏视频（B 站/YouTube）。
- 窗口化游戏、无边框全屏游戏、独占全屏游戏。
- PowerPoint 演示模式。
- 全屏远程桌面/虚拟机窗口。
- 全屏应用最小化/恢复。
- A 屏全屏游戏、B 屏正常办公，光标在 B 屏晃动。

**系统状态：**

- 锁屏/解锁、切换用户、注销。
- 睡眠/唤醒。
- RDP 连接/断开。
- 更换光标主题、调整辅助功能光标大小（1–15）。
- 高对比度主题、反色光标。
- 杀进程/断电模拟（脏标记恢复）。
- 多语言系统（中文/英文）。

### 8.3 性能与验收指标

| 指标 | 目标 |
|---|---|
| 空闲 CPU（Raw Input 可用） | < 0.05%（4 核） |
| 晃动时 CPU | < 0.8% |
| 工作集 | < 16MB |
| 帧缓存 | ≤ 8MB |
| GDI 对象 / 句柄 | < 50 / < 100 |
| 二进制 | < 1.5MB（含内置素材） |
| 安装后体积 | < 2MB |
| 启动时间 | < 50ms |
| 触发延迟 | < 400ms（含 250ms 持续激活） |
| 达到最大倍率 | < 700ms 持续晃动 |
| 缩回完成 | < 300ms |
| 跨入全屏屏恢复 | < 100ms |
| 强制结束进程后恢复 | 下次启动 100%（脏标记） |
| 连续 8 小时运行 | 无句柄/GDI 泄漏、内存无增长 |

### 8.4 无感与自然操作测试

| 编号 | 场景 | 期望 |
|---|---|---|
| U01 | 普通快速移动 2000 DIP/s | 不触发 |
| U02 | 按住左键拖拽/框选 | 不触发 |
| U03 | 连续晃动 3 次 | 150ms 内触发 |
| U04 | 停止晃动 | 800ms 后缩回，无闪烁 |
| U05 | 晃动后点击 | 立即缩回 |
| U06 | 全屏视频/游戏 | 不触发 |
| U07 | A 屏全屏、B 屏晃动 | B 屏触发 |
| U08 | 首次安装/开机启动 | 无窗口、无弹窗、托盘可用 |
| U09 | 卸载 | 光标恢复、自启项删除 |
| U10 | 4K 200% + 8x | 光标 ≤512px、清晰、热区不偏 |

### 8.5 CI

- GitHub Actions `windows-2022`：
  - `cmake --preset x64-debug` + build + `ctest`.
  - Release build，上传 artifact。
  - 可选 clang-format 检查。
- 不在 CI 里做真实光标替换（无桌面会话）；只跑纯逻辑测试。

---

## 9. 风险与对策

| 风险 | 影响 | 对策 |
|---|---|---|
| 进程被杀后光标留在大号 | 用户困扰 | 脏标记 + 启动恢复 + `--restore` + 托盘恢复；第二阶段 watchdog |
| 杀毒软件误报 | 无法安装/运行 | 开源、可选代码签名、发布 SHA256、无注入/无 Hook 游戏、说明文档 |
| 全屏误判（最大化窗口被当成全屏） | 功能不触发 | 几何规则以 `rcMonitor` 为准 + 标题栏/WS_POPUP + QUNS 辅助 + 白/黑名单 |
| 全屏漏判（某些无边框游戏） | 游戏中被放大 | QUNS_D3D + z 序扫描 + 用户黑名单；全屏时系统光标通常被隐藏，影响有限 |
| 混合 DPI 下大小不一致 | 体验差 | 每屏 `GetDpiForMonitor` + `GetSystemMetricsForDpi` + 跨屏帧切换 |
| 放大后模糊 | 体验差 | 优先 `.cur` 原生高分辨率帧 + 内置高分辨率 Arrow/Hand 素材 |
| `.ani` 动画光标 | 放大后不动 | 第一版首帧静态；文档说明；第二阶段解析 ANI |
| 反色光标失效 | 深色背景下看不清 | 记录限制；第二阶段做单色掩码兼容 |
| `SetSystemCursor` 权限失败 | 功能不可用 | asInvoker 运行；错误提示；不自动提权 |
| 光标主题切换导致缓存失效 | 形状错误 | `RegNotifyChangeKeyValue` 监听并重建 |
| 系统光标全局性导致跨屏一帧大光标 | 轻微视觉瑕疵 | 100ms 复查 + 移入全屏屏立即恢复；全屏应用通常隐藏光标 |
| 轮询漏掉极快反转 | 灵敏度下降 | 125–250Hz 可调；人晃动频率低，实际足够 |
| Windows 更新改变行为 | 兼容性 | CI 矩阵 + 兼容性文档 + 快速发版 |

---

## 10. 与现有项目相比的改进点

| 能力 | 现有最好水平 | CursorBeacon |
|---|---|---|
| 连续放大 | bwya77/giant-cursor 有 | 能量映射 + 非对称平滑 + 量化更新 |
| 多屏 | 系统光标类天然支持 | 系统光标 + 每屏 DPI 帧 + 跨屏热切换 |
| 每屏全屏抑制 | **没有项目做到** | QUNS + 前台窗口 + 几何 + z 序，按光标所在屏判定 |
| 混合 DPI | 多数固定 32px / 系统 DPI | 每屏 `GetDpiForMonitor` + `GetSystemMetricsForDpi` |
| 崩溃恢复 | 少数有 `--restore` | 脏标记 + 异常处理 + 启动恢复 + 会话通知 |
| 测试 | giant-cursor 有部分单测 | 检测/倍率/全屏规则/配置全单测 + CI |
| 权限 | 0cch 要管理员 | 默认 asInvoker |
| 空闲 CPU | 多数 120–125Hz 常驻轮询 | Raw Input 事件驱动 + 10Hz watchdog，目标 <0.05% |
| 常驻内存 | Qt/.NET 方案明显更高 | 目标 <16MB，帧缓存 ≤8MB |
| 安装体验 | 多数需手动配置/自启 | 按用户静默安装，默认开机启动，无首启窗口 |
| 体积 | 0cch 215KB | 目标 < 1.5MB（含素材），无运行时 |
| 许可 | 部分无/LICENSE 不清 | MIT |
| 主题尊重 | yfaj/0cch v1.4.1 读取主题 | 读取主题 + 高分辨率回退 + 主题变更监听 |

---

## 11. 验收标准（Definition of Done）

功能：

- [ ] 连续晃动约 250–400ms 内开始放大，普通导航/锯齿移动不触发。
- [ ] 持续晃动 700ms 内达到配置的最大倍率。
- [ ] 停止晃动后保持 `hold_ms`，然后 300ms 内恢复。
- [ ] 按下鼠标键立即恢复。
- [ ] 4K 200% 屏上放大后的视觉大小与 1080p 100% 屏一致（按 DPI 归一化）。
- [ ] 多屏任意一块屏上晃动都能触发（该屏非全屏时）。
- [ ] 全屏所在屏不触发；其他屏正常。
- [ ] 放大中移入全屏屏，100ms 内恢复。
- [ ] 锁屏/注销/退出后光标恢复。
- [ ] 强制结束进程后，下次启动自动恢复。

质量：

- [ ] `ctest` 全绿。
- [ ] 8 小时运行无句柄/GDI 泄漏、内存无增长。
- [ ] 空闲 CPU < 0.05%（Raw Input 可用时），晃动时 < 0.8%。
- [ ] 常驻内存 < 16MB，帧缓存 ≤ 8MB。
- [ ] 单 exe，无 VC++ 运行时依赖（静态 CRT），安装后体积 < 2MB。
- [ ] Windows 10 22H2 / Windows 11 23H2+ 实机通过。
- [ ] 安装/卸载全程无 UAC、无首启窗口。

体验：

- [ ] 普通快速移动、拖拽、框选不触发。
- [ ] 放大后光标形状、主题、热区不变。
- [ ] 4K 200% + 8x 下光标清晰、不跳位。
- [ ] 停止晃动 800ms 后自动缩回；点击立即缩回。
- [ ] 开机启动后无任何窗口/弹窗，只在托盘出现。

文档：

- [ ] README（安装、使用、FAQ、已知限制）。
- [ ] `docs/TESTING.md` 手动测试步骤。
- [ ] `docs/COMPATIBILITY.md`（.ani、反色、RDP、游戏）。
- [ ] LICENSE（MIT）。

---

## 12. 假设与待确认项

以下按默认值推进；如果你有不同偏好，实施前调整：

1. **语言/栈**：默认 C++20 + Win32 + CMake + MSVC。若你更希望 Rust/C#，架构不变，模块可 1:1 映射。
2. **系统版本**：默认 Windows 10 1809+ / Windows 11，不支持 Win7/8。
3. **权限**：默认 asInvoker，不请求管理员。
4. **“找到后缩回”**：默认“停止晃动 + 800ms 保持”，并支持按键立即缩回；不做摄像头/眼动等花哨检测。
5. **项目名/许可证**：默认 CursorBeacon / MIT，可改名。
6. **第一版不做**：游戏自绘光标 overlay、自动更新、arm64、安装包签名。可作为第二阶段。
7. **采样策略**：默认 Raw Input 优先，触摸板/RDP 自动降级轮询；不做低级鼠标 Hook。
8. **放大范围**：默认只放大 Arrow/IBeam/Hand/Crosshair 四种光标；`enlarge_all_cursors` 可选。
9. **无感体验**：默认无首启提示、无成功弹窗；安装器默认开机启动但可取消。
10. **减少动态效果**：默认跟随系统设置；开启时跳过平滑动画。

---

## 13. 建议的实施顺序

1. 本地环境已就绪：LLVM-MinGW + CMake + Ninja（见 3.6）；MSVC Build Tools 作为 CI/可选的第二编译器。
2. 初始化仓库、CMake、manifest、日志、单实例、`--restore`。
3. 先做 M1 的 Raw Input/轮询采样 + 纯逻辑 + 单测（不碰系统光标），确认检测手感和阈值，并记录空闲 CPU/唤醒次数。
4. 再做 M2 的单屏光标替换，重点验证热区、恢复、脏标记。
5. 然后 M3 的多屏/DPI/每屏全屏，这是本项目的核心竞争力。
6. 最后 M4/M5 的 UI、配置、打包、文档。

> 第一版的目标不是“功能最多”，而是“在多屏 + 高 DPI + 全屏场景下不犯错”。只要 M3 的每屏判定和 DPI 处理做扎实，就已经明显优于现有项目。

---

## 14. 当前实现状态（2026-11-02）

**已完成：M0 / M1 / M2，以及 M3 的前台窗口每屏全屏判定。**

| 模块 | 状态 | 说明 |
|---|---|---|
| CMake / manifest / 工具链 | 完成 | LLVM-MinGW + CMake + Ninja；PerMonitorV2 + asInvoker |
| ShakeDetector | 完成 | 600ms 窗口、默认 3 次反转（高灵敏度 2/低灵敏度 4）、速度/幅度门槛、path 与净位移过滤、DIP 归一化、按键/瞬移抑制 |
| ScaleController | 完成 | 能量映射、倍率阶梯、非对称平滑、800ms 保持、逐级缩回 |
| StateMachine | 完成 | Idle/Shaking/Holding/Shrinking/Suppressed |
| PointerSource | 完成 | Raw Input + 10Hz watchdog + 125Hz 轮询兜底；实测 SetCursorPos 合成移动会切轮询 |
| Monitors / DPI | 完成 | EnumDisplayMonitors、GetDpiForMonitor、每 5s 刷新 |
| 每屏全屏 | 部分完成 | 前台窗口 + ExtendedFrameBounds + QUNS；z 序后台扫描/事件钩子待做 |
| 光标主题/渲染/替换 | 完成 | 读取 `.cur` 主题、4 种常用光标、`SetSystemCursor`、热区由 Windows 缩放 |
| 崩溃恢复 | 完成 | 脏标记 + `atexit`/异常回调 + `--restore` |
| CLI | 完成 | `--run`、`--stats`、`--test-scale`、`--dump-monitors`、`--dump-cursors`、`--restore`、`--stop`、`--self-test`、`--duration` |
| 托盘/设置/开机启动 | 未开始 | M4 |
| 安装器/CI/签名 | 未开始 | M5 |

**实测结果：**

- 3 台显示器，均为 144 DPI（主屏 2880×1800，扩展屏 2560×1440 ×2）。
- Raw Input 注册成功；合成移动走 watchdog → 125Hz 轮询，700 个采样点正常进入检测器。
- 4 到 5Hz 晃动触发到 4x，实际替换为 192px 光标；**无需管理员权限**。
- 停止晃动后保持 800ms，然后 3x→2.5x→2x→1.75x→1.5x→1.25x→恢复，`SPI_SETCURSORS` 执行成功。
- Release 单 exe 927KB；空闲 working set ≈ 10.8MB；空闲 CPU ≈ 0.05%（4 核）。
- `ctest` 24 个用例全部通过。
- 修复“停止晃动后不缩回”：Raw Input 在鼠标完全静止时不再产生样本，能量会冻结在最大值；控制器现在每轮调用 `detector.decay(now)`，无样本时按 200ms 时间常数衰减，hold 到期后正常缩回。`--self-test` 已覆盖该场景。
- 本机 Device Guard 会拦截独立测试 exe；`--self-test` 可在主程序内运行核心逻辑检查，`tools/build.sh` 会自动回退。

**下一步优先级：**

1. M3 收尾：`SetWinEventHook(EVENT_SYSTEM_FOREGROUND)`、`WM_DISPLAYCHANGE`、会话锁屏通知、z 序后台全屏扫描。
2. M4：托盘图标、设置对话框、开机启动、主题变更监听。
3. M5：Inno Setup 按用户安装、GitHub Actions（MSVC + MinGW 双编译）、8 小时浸泡测试。

---

## 15. 审查与打包（2026-11-02）

- 静态分析：clang-tidy（`clang-analyzer-*`、`bugprone-*`、`cert-*`）未发现 analyzer 级问题；剩余为参数可交换等风格提示。
- 全量测试：24 个 doctest + 13 项 `--self-test` 全部通过。
- 审查结论：未发现 critical/high 安全漏洞；已修复停止晃动不缩回、CLI 数值越界、渲染倍率溢出、配置/注册表读取上限、析构异常安全等 6 类问题。完整报告见 `docs/REVIEW.md`。
- 打包产物：`dist/CursorBeacon-0.1.0-win64/`、`dist/CursorBeacon-0.1.0-win64.zip`、`.zip.sha256`。
- 已实测打包脚本：start/status/stop/restart；后台服务 PID 运行、约 10.3MB working set。
