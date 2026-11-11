# CursorBeacon 代码审查与安全检查

> 第二轮全量复查（2026-11-11，含 QPC 溢出、分屏光标形状、SPI 阻塞、Raw Input 健康重注册）见 [REVIEW-2026-11-11.md](REVIEW-2026-11-11.md)。

日期：2026-11-02
版本：0.1.0
范围：`src/`、`tests/`、`CMakeLists.txt`、`resources/app.manifest`、`tools/` 脚本

## 结论

**未发现 critical / high 级别的安全漏洞或必现崩溃。** 发现并修复了 6 类问题，其中 1 个是用户实际遇到的功能 bug（停止晃动后不缩回）。剩余问题均为低风险或已知功能缺口。

## 已修复

| # | 严重度 | 问题 | 修复 |
|---|---|---|---|
| 1 | 高（功能） | Raw Input 在鼠标完全静止后不再产生样本，检测器能量冻结在最大值，`hold_until` 每轮被续期，导致停止晃动后不缩回；点击产生样本才恢复 | 控制器每轮调用 `ShakeDetector::decay(now)`，无样本时按 200ms 时间常数衰减；新增自测项 |
| 2 | 中 | CLI 数值未限制：`--scale`/`--sensitivity`/`--hold-ms`/`--duration`/`--test-scale` 接受 NaN、Inf、1e18，可能触发 `lround`/整数转换未定义行为 | 所有数值先做 `isfinite` 检查并 clamp 到安全范围 |
| 3 | 中 | `CursorRenderer` 对 NaN/超大 scale 直接 `lround`，可能溢出 | 渲染前 clamp scale 到 1..8，target 到 8..512 |
| 4 | 中 | 配置文件和注册表光标路径读取无上限，异常大值可能造成内存压力 | 配置文件限制 1MB；注册表字符串限制 64KB；`ERROR_MORE_DATA` 安全失败 |
| 5 | 低 | `CursorEffector` 析构函数调用可能分配内存的 `restore()`，极端情况下 `noexcept` 析构中抛异常导致 terminate | 析构中 try/catch，失败回退到无分配的 `SystemParametersInfoW` |
| 6 | 低 | 有符号位运算、忽略 `swprintf`/`fprintf` 返回值等静态分析告警 | 改为显式无符号类型并检查/显式忽略返回值 |
| 7 | 高（功能） | 普通鼠标移动/锯齿导航被误判为晃动，导致光标频繁 grow/shrink；3Hz 持续晃动在反转阈值边界会中途掉档 | 提高速度/幅度阈值；加入 path 与净位移过滤（net/path ≤0.50、net/amplitude ≤0.35）；250ms 持续激活 + 600ms 冷却；Raw Input 与轮询单生产者；默认 3 次反转 |

## 已确认的安全属性

- **无网络**：无遥测、无自动更新、无外部请求。
- **无提权**：manifest 为 `asInvoker`；实测普通用户可调用 `SetSystemCursor`，不需要管理员权限。
- **无注入**：不注入进程、不挂全局输入 Hook、不读写其他进程内存；只用 Raw Input / 轮询和系统光标 API。
- **数据面最小**：只读 `HKCU\Control Panel\Cursors` 和用户当前 `.cur` 主题；只写 `%APPDATA%\CursorBeacon`、`%LOCALAPPDATA%\CursorBeacon`。
- **异常路径可恢复**：脏标记 + `atexit` + 未处理异常回调 + `--restore` + `--stop`；实测退出/停止后无残留大光标。
- **单实例**：`Local\CursorBeacon.Singleton`，避免多实例同时替换光标。
- **配置健壮性**：所有字段做范围/有限性校验；未知字段忽略；损坏配置回退默认值。

## 剩余风险（低）

1. **同会话停止事件**：同一登录会话中的任意进程可以打开并 SetEvent `Local\CursorBeacon.StopRequest`，让服务退出。这是同用户级 DoS，不涉及提权或数据泄露；可接受。
2. **未签名 exe**：可能触发 SmartScreen / 杀软误报。发布时建议代码签名，或在文档中说明 SHA256 和源码构建方式。
3. **全屏检测仅前台窗口 + 几何**：后台/非前台的全屏窗口、z 序扫描尚未实现；独占全屏游戏需要实机逐项验证。纯几何规则已有单测。
4. **App 自绘光标**：`SetSystemCursor` 只影响标准系统光标；游戏/设计软件自己绘制光标时不受影响。
5. **`.ani` 动画光标**：放大期间可能只显示首帧；缩回后恢复系统动画。
6. **主题变更需重启**：尚未监听 `HKCU\Control Panel\Cursors` 变化。
7. **混合 DPI**：当前 3 屏均为 144 DPI；100%/150%/200% 混合 DPI 仍需单独验证。
8. **Hand 光标热点**：测试中原始热点 10、4x 后 38，按几何比例预期 40；差异来自 `.cur` 内高分辨率帧本身的热点，2px 级别，不影响可用性。
9. **Device Guard**：本机策略会间歇性拦截独立 `cursorbeacon_tests.exe`；已增加主程序内 `--self-test`，构建脚本检测到拦截会自动回退。正常机器/CI 仍运行完整 doctest。

## 并发与资源

- 线程模型：控制器主线程、PointerSource 消息线程、按需启动的 polling 线程。
- `SampleQueue` 使用 mutex + auto-reset event；采样生产者与控制器消费者之间无无界队列。
- 停止顺序：`--stop`/Ctrl+C → 控制器退出循环 → `PointerSource::stop()` → 停止 polling → 等待消息线程 → `CursorEffector::restore()`。
- 9 秒移动压力测试：句柄 158 → 162（波动，无持续增长），GDI 0，User objects 5–7。
- 光标帧只在当前 DPI/倍率下缓存；切换倍率时销毁旧帧；effector 使用 `CopyIcon` 后由系统接管。
- 脏标记在成功恢复后删除；恢复失败时保留，供下次启动重试。

## 测试证据

- `ctest`：26 个 doctest 用例全部通过（Release）。
- `--self-test`：13 项核心逻辑检查全部 PASS，包含“无新样本时能量衰减”。
- **误触发隔离测试**：慢速大圈（0.8Hz、300×200）0 次 scale；带净位移的快速锯齿导航 0 次 scale；真正 4Hz 来回晃动正常放大到 4x。
- **稳定性测试**：持续 3Hz 晃动不再中途掉档；停止后保持 → 逐级 3x→2.5x→2x→1.75x→1.5x→1.25x → `cursor restored`。
- `--test-scale 4`：144 DPI 下 target 192px，4 个光标帧；热点检查 Arrow/IBeam/Crosshair 符合预期。
- 空闲：working set ≈10.8MB，CPU ≈0.05%（4 核）。
- 畸形配置（`max_scale=nan`、`sensitivity=999`、`hold_ms=-5`）：正常启动，参数被净化，无崩溃。
- 退出/停止后：无 `restore.needed` 脏标记。

## 建议的后续优先级

1. M4：托盘 + 设置 + 开机启动，替代当前隐藏控制台服务。
2. M3 收尾：`SetWinEventHook(EVENT_SYSTEM_FOREGROUND)`、`WM_DISPLAYCHANGE`、会话锁屏通知、z 序全屏扫描。
3. M5：代码签名、Inno Setup 按用户安装、GitHub Actions MSVC + MinGW 双编译、8 小时浸泡。
