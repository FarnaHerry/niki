// The structure panel: the document as an indented outline. Clicking a row
// selects that node; selection is shared with every other panel.

export module hui.ui.structure;

import std;
import huxerui;
import hui.core.catalog;
import hui.core.doc;
import hui.ui.editor;

using namespace huxerui;

export namespace hui::structure {

namespace detail {

[[nodiscard]] inline View RowFor(const editor::Editor& ed, const doc::Node& node, std::size_t depth) {
  const std::string id = node.id;
  const catalog::ComponentDef* def = catalog::Find(node.type);
  const std::string summary = doc::SummaryOf(node);

  View row = Row{
      Text(def != nullptr ? def->glyph : "?", TextRole::Label).With(Frame{.width = 18.0F}),
      Text(node.type, TextRole::Label),
      Text(summary == node.type ? std::string() : summary, TextRole::Label).With(Opacity(0.55F), Grow(1.0F)),
      Text(id, TextRole::Label).With(Opacity(0.4F)),
  }
      .With(Spacing(6.0F),
            Padding(EdgeInsets{.top = 3.0F, .bottom = 3.0F, .left = 4.0F + static_cast<float>(depth) * 14.0F}),
            CrossAlign(CrossAxisAlignment::Center), CornerRadius(4.0F))
      .OnClick([ed, id] {
        ed.selection = id;
        ed.status = "selected " + id;
      });
  if (ed.selection.Get() == id) {
    row = std::move(row).With(Background(ed.palette.accent_soft));
  }
  return std::move(row).Key(id);
}

inline void RowsFor(const editor::Editor& ed, const doc::Node& node, std::size_t depth, std::vector<View>& rows) {
  rows.push_back(RowFor(ed, node, depth));
  for (const auto& child : node.children) {
    RowsFor(ed, child, depth + 1, rows);
  }
}

}  // namespace detail

[[nodiscard]] inline View StructureView(const editor::Editor& ed) {
  const doc::Document& document = ed.document.Get();

  std::vector<View> rows;
  detail::RowsFor(ed, document.root, 0, rows);

  return Column{
      Text("Structure", TextRole::Title),
      Column(std::move(rows)).With(Spacing(1.0F)),
  }
      .With(Spacing(6.0F), Padding(8.0F))
      .Key("structure");
}

}  // namespace hui::structure
