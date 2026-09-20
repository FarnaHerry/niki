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

四个岛屿都已接上真实面板，接下来是细化。

- **检查器**：颜色用取色器而不是手写 `#RRGGBB`；枚举用下拉以外更省空间的形式；字段的本地缓冲目前靠
  `.Key(node#field)` 跟随选中切换，页面切换时要再确认一次（同一个 key 在不同页面里可能撞上）；
  属性行的 label 宽度固定 92px，长 key 会挤。
- **代码岛**：语法高亮、行号、导出前预览哪些 handler 需要自己实现（`codegen` 已经产出清单，
  只是没显示）；页签切换目前每次重算 `GenerateCpp`，文档大以后要加缓存。
- **画布细化**：拖到容器**内部某个位置**插入（目前只追加到末尾）、拖动时的落点插入线、
  多选与键盘删除；拉伸把手只改 `width`/`height`，还没有等比（Shift）、从中心拉、按相邻元素对齐吸附；
  面板「按下即拖」后要盯一下两处 ScrollView（组件岛、画布）还能不能拖动滚动。
- **标签页细化**：拖动重排（`StateList::Move` 已经就位）、双击重命名（`RenameActive` 已经就位，
  改名后要连带决定是否重命名文件）、`Save As` 走 `FilePicker::SaveFileAsync`、
  关闭未保存页面时确认、把整组页面存成一个工程文件。
- **README 里列的后续**：更多事件（指针/键盘/生命周期）、列表虚拟化组件、导航壳、图片资源、
  页面之间的跳转模型、C++ → 文档反向解析。
