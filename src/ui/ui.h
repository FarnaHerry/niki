// ui.h — the designer's UI declarations.
//
// One header for the whole front end, the way apitab keeps its UI: the shell
// and every panel include this, and the implementations live beside it in
// per-panel .cpp files. The engine behind it is a set of C++23 modules, so the
// include block here comes first and the imports follow it — GCC builds every
// translation unit that way.
#pragma once

#include <huxerui/huxerui.h>

#include "ui/theme.h"

#include <string>

import hui.core.catalog;
import hui.core.doc;

namespace hui::ui {

/// Shared designer state, threaded through every panel. Holds the
/// composition-scoped document/selection/status/history handles, the active
/// palette, and the single place a document mutation is published from.
struct Editor final {
  huxerui::State<doc::Document> document;
  huxerui::State<std::string> selection;
  huxerui::State<std::string> status;
  huxerui::State<doc::History> history;
  huxerui::State<std::string> drop_hint;  // container highlighted by a drag
  theme::Palette palette;

  /// Publishes a mutated document: the outgoing version is snapshotted into
  /// history first, so every panel action is undoable.
  void Apply(doc::Document next) const;
  /// Publishes a whole new document (New / Open) and drops the selection.
  void Replace(doc::Document next, std::string message) const;
  void Undo() const;
  void Redo() const;
  void Open(const std::string& file) const;
  void Save(const std::string& file) const;
  void Export(const std::string& file) const;
};

/// Components island: one card per catalog component, grouped by category,
/// over the document structure tree.
[[nodiscard]] huxerui::View PaletteView(const Editor& ed);
[[nodiscard]] huxerui::View StructureView(const Editor& ed);

}  // namespace hui::ui
