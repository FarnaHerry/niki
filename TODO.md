# 待办

## 待上游处理（本地已有绕过，不阻塞开发）

### 1. PR：`WindowCaptionControls`（框架 API 增补）

**状态**：本地 HuxerUI 已改好（6 文件，+30 行），等有空提 PR。

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

### 2. 移除 `build.mcpp` 里的资源 staging 绕过

**状态**：等 HuxerUI#145 修复。
见 <https://github.com/HuxerUI/HuxerUI/issues/145>。
修复后删掉 `build.mcpp` 中标注 `Workaround for HuxerUI#145` 的整段。

### 3. mcpp 增量缓存漏掉依赖源码改动

改 HuxerUI 的 `.cpp` 或 `build.mcpp` 结构后，`mcpp build` 可能报 `Finished in 0.05s` 复用旧产物，
新的构建动作甚至不会进入 `build.ninja`。遇到行为与源码不一致时先 `rm -rf target`。
这是 mcpp 侧的问题，可另开 issue。

## 待开发

- **画布（第 3 步，暂停）**：完整 `BuildComponent` 会让整页空白。已缩小到「某个组件分支的编译」
  （起始文档只有 Column+Text，运行时不会执行那些分支）。精简到只剩容器 + Text 时布局正常。
  归档实现在 `old/ui/canvas-step3.cppm`。
- **交互**：拖放（palette → 容器、节点移动）、撤销/重做、打开/保存。
- **README** 里列的后续：更多事件、列表虚拟化组件、导航壳、图片资源、多页面工程、C++ → 文档反向解析。
