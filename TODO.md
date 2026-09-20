# 待办

## 待上游处理（本地已有绕过，不阻塞开发）

### 1. PR：`WindowCaptionControls`（框架 API 增补）

**状态**：本地 HuxerUI 检出已改好（6 文件，+30 行），等有空提 PR。

**动机**：`WindowChromeMode::Custom` 下框架**无条件**画一组 min/max/close，且 Linux 适配器恒定
预留右侧 138px（`3 × kLinuxCaptionButtonWidth`），应用层无法关闭、也无法覆盖（该节点是
RuntimeRoot 的最后一个子节点，画在应用内容之上）。

**改动**：

| 文件 | 内容 |
|---|---|
| `include/huxerui/window.h` | `enum class WindowCaptionControls { Framework, Application }` + `WindowOptions::caption_controls`（默认 `Framework`，保持原行为） |
| `src/application/window_internal.h` | `WindowState` 携带该选项 |
| `src/runtime/runtime.cpp` | 仅 `Framework` 时挂载 / 校验 / Reconcile 该节点 |
| `platform/linux/linux_internal.h` | `ResolveLinuxTitleBarMetrics(..., bool reserve_caption_controls = true)`，`Application` 时 `right_inset = 0` |
| `platform/linux/linux_adapter.cpp` | 传递该选项 |
| `modules/huxerui.cppm` | 补 `using huxerui::WindowCaptionControls;`（生成文件，漏了会导致 `import huxerui;` 看不到该名字） |

**待补**：`platform/windows/win32_adapter.cpp` 的 `QueryTitleBarMetrics()` 仍按系统按钮预留；
Windows 的 custom chrome 由系统画按钮，需要时再同步。

## 待开发

岛屿逐个补齐，顺序：检查器 → 代码面板。

- **检查器岛（右侧 320）**：属性（枚举下拉、布尔开关、数值、选项列表）、修饰符
  （padding/width/height/grow/圆角/前景背景边框/字号/enabled）与事件（开关绑定 + 填 handler 名），
  外加问题列表。归档实现在 `old/ui-modules/inspector.cppm` + `widgets.cppm`，
  受控字段控件按 `src/ui/*.h/.cpp` 的约定重写。
- **代码岛（底部 200）**：C++ / JSON 双页签 + Copy，直接调 `hui.core.codegen` 与
  `hui.core.docio`。归档实现在 `old/ui-modules/codepanel.cppm`。
- **画布细化**：拖到容器**内部某个位置**插入（目前只追加到末尾）、拖动时的落点插入线、
  多选与键盘删除。
- **README 里列的后续**：更多事件（指针/键盘/生命周期）、列表虚拟化组件、导航壳、图片资源、
  多页面工程、C++ → 文档反向解析。
