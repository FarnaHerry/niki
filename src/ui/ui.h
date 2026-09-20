// ui.h — the designer's UI declarations.
//
// One header for the whole front end, the way apitab keeps its UI: the shell
// and every panel include this, and the implementations live beside it in
// per-panel .cpp files. The engine behind it is a set of C++23 modules, so the
// include block here comes first and the imports follow it — GCC builds every
// translation unit that way.
#pragma once

#include <huxerui/huxerui.h>

#include "ui/resize.h"
#include "ui/theme.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

import hui.core.catalog;
import hui.core.doc;

namespace hui::ui {

/// The one drag payload the designer speaks. One payload type means a container
/// needs a single DropTarget: a palette card creates a node (move = false,
/// ref = component type), a node already on the canvas relocates (move = true,
/// ref = node id).
struct DropPayload final {
  bool move = false;
  std::string ref;
};

/// One page: a document plus everything that belongs to it. Tabs hold one of
/// these each, so switching tabs is a change of index and never a migration of
/// state between slots.
///
/// A page and its file are the same thing seen twice: `path` is the file, and
/// `title` is the name its tab shows — the file's own name once it has one.
struct Page final {
  std::string title;      // tab label: the file name, or a placeholder name
  std::string path;       // the file itself; empty until the first Save
  doc::Document document;
  doc::History history;
  std::string selection;  // selected node id, empty when nothing is selected
  bool dirty = false;     // edited since the last Save
};

/// A fresh page holding a starter document.
[[nodiscard]] Page StarterPage(std::string title, std::string path);

/// A resize gesture in progress: which node, the size it had when the gesture
/// began, and the document to put back into history when it ends. The document
/// is published live while the pointer moves, so the whole gesture costs one
/// undo step rather than one per frame.
struct ResizeGesture final {
  std::string node;  // empty when nothing is being resized
  float width = 0.0F;
  float height = 0.0F;
  doc::Document before;
};

/// Smallest a node may be resized to, in logical pixels.
inline constexpr float kMinimumNodeSize = 8.0F;

/// Shared designer state, threaded through every panel.
///
/// The open pages live in one list and `active` picks among them. Panels read
/// and write the active page through the accessors below and never see the tab
/// list, so a panel written for one page works unchanged for every tab.
struct Editor final {
  huxerui::StateList<Page> pages;
  huxerui::State<std::size_t> active;
  huxerui::State<std::string> status;
  huxerui::State<std::string> drop_hint;  // container highlighted by a drag
  huxerui::State<ResizeGesture> resize;
  std::shared_ptr<NodeMetrics> metrics;  // where the canvas reports node sizes
  theme::Palette palette;

  [[nodiscard]] std::size_t ActiveIndex() const;
  [[nodiscard]] const Page& Current() const;
  [[nodiscard]] const doc::Document& Document() const;
  [[nodiscard]] const std::string& Selection() const;
  [[nodiscard]] const std::string& Status() const;
  [[nodiscard]] const std::string& Hint() const;

  /// Applies `mutate` to a copy of the active page and publishes it. Every
  /// change to a page goes through here, so an edit is always one whole page
  /// and a panel can never leave the document half updated.
  void Edit(const std::function<void(Page&)>& mutate) const;

  void Select(std::string id) const;
  void SetStatus(std::string message) const;
  void SetHint(std::string id) const;

  /// Publishes a mutated document: the outgoing version is snapshotted into the
  /// active page's history first, so every panel action is undoable.
  void Apply(doc::Document next) const;

  /// Starts resizing `id` from the size it currently has on screen. The size
  /// comes from the canvas's measurement table, so a node with no explicit
  /// width still resizes from where it actually is.
  void BeginResize(std::string id) const;
  /// Applies a resize delta to the node the gesture started on. Ignored when
  /// `id` is not that node, so a stale handler cannot move someone else.
  void ResizeBy(const std::string& id, float delta_width, float delta_height) const;
  /// Ends the gesture and puts its starting document into history.
  void EndResize() const;
  [[nodiscard]] bool Resizing() const;

  /// Appends a page and activates it. An empty title falls back to the document
  /// name; an empty path means the page has never been saved.
  void AddPage(std::string title, std::string path, doc::Document document) const;
  /// Appends an empty starter page named after how many pages are already open.
  void NewPage() const;
  void Activate(std::size_t index) const;
  /// Removes a page. The last one is not removed: closing it starts a new
  /// document in place, so there is always exactly one page to edit.
  void ClosePage(std::size_t index) const;
  void RenameActive(std::string title) const;

  void Undo() const;
  void Redo() const;
  /// Opens a file in its own tab, or switches to it when it is already open.
  void Open(const std::string& file) const;
  /// Writes the active page to its own file, giving it one derived from its
  /// title when it has never been saved.
  void Save() const;
  /// Writes the active page to `file` and adopts that file as the page's own.
  void SaveAs(const std::string& file) const;
  /// Generates the active page's C++ module next to its own file.
  void Export() const;
};

/// Tab strip: one tab per page, each showing the file it is, with a close
/// button, plus the add button.
[[nodiscard]] huxerui::View TabStripView(const Editor& ed);

/// Components island: one card per catalog component, grouped by category,
/// over the document structure tree.
[[nodiscard]] huxerui::View PaletteView(const Editor& ed);
[[nodiscard]] huxerui::View StructureView(const Editor& ed);

/// Canvas island: the document rendered with the real components it stands
/// for, with click-to-select, drag-to-relocate, resize handles, and container
/// drop targets.
[[nodiscard]] huxerui::View CanvasView(const Editor& ed);

/// Inspector island: the selected node's properties, modifiers, and events —
/// or the document itself when nothing is selected — over the live issue list.
[[nodiscard]] huxerui::View InspectorView(const Editor& ed);

/// Code island: the C++ the document generates and its own JSON behind two
/// tabs. A composable: it owns which tab is showing.
[[nodiscard]] huxerui::View CodeView(const Editor& ed);

}  // namespace hui::ui
