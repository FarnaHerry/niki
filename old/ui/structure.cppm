// The structure tree: the document as an indented outline. Click selects; the
// selected row highlights, and every row is a relocation DragSource.

export module hui.ui.structure;

import std;
import huxerui;
import hui.core.catalog;
import hui.core.doc;
import hui.ui.canvas;
import hui.ui.widgets;

using namespace huxerui;

export namespace hui::structure {

namespace detail {

[[nodiscard]] inline View RowFor(const canvas::Editor& editor, const doc::Node& node, std::size_t depth) {
  const std::string selected = editor.selection.Get();
  const catalog::ComponentDef* def = catalog::Find(node.type);

  View row = Row{
      Text(def != nullptr ? def->glyph : "?", TextRole::Label).With(Frame{.width = 20.0F}),
      Text(node.type, TextRole::Label),
      Text(doc::SummaryOf(node), TextRole::Label).With(Opacity(0.55F), Grow(1.0F)),
      Text(node.events.empty() ? std::string() : std::format("⚡ {}", node.events.size()), TextRole::Label)
          .With(Opacity(0.6F)),
      Text(node.id, TextRole::Label).With(Opacity(0.4F)),
  }
                 .With(Spacing(6.0F),
                       Padding(EdgeInsets{.left = 4.0F + static_cast<float>(depth) * 14.0F, .top = 3.0F, .bottom = 3.0F}),
                       CrossAlign(CrossAxisAlignment::Center), CornerRadius(4.0F))
                 .With(DragSource(widgets::CanvasDrop{.move = true, .ref = node.id},
                                  [type = node.type, summary = doc::SummaryOf(node)] {
                                    return Text(type + " — " + summary, TextRole::Label)
                                        .With(Padding(8.0F), Background(Color::Rgba32(0xFFF2F0FA)),
                                              Border{.color = Color::Rgba32(0xFF2D6CDF), .width = 1.0F},
                                              CornerRadius(6.0F));
                                  }))
                 .OnClick([selection = editor.selection, id = node.id] { selection = id; });
  if (selected == node.id) {
    row = std::move(row).With(Background(Color::Rgba32(0x2E2D6CDF)));
  }
  return std::move(row).Key(node.id);
}

inline void RowsFor(const canvas::Editor& editor, const doc::Node& node, std::size_t depth, std::vector<View>& rows) {
  rows.push_back(RowFor(editor, node, depth));
  for (const auto& child : node.children) {
    RowsFor(editor, child, depth + 1, rows);
  }
}

}  // namespace detail

[[nodiscard]] inline View StructureView(const canvas::Editor& editor) {
  const doc::Document& document = editor.doc.Get();

  std::vector<View> rows;
  detail::RowsFor(editor, document.root, 0, rows);

  return Column{
      Text("Structure", TextRole::Title),
      Column(std::move(rows)).With(Spacing(1.0F)),
      Spacer(),
  }
      .With(Spacing(8.0F), Padding(8.0F))
      .Key("structure");
}

}  // namespace hui::structure
