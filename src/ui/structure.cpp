#include "ui/ui.h"

#include <cstddef>
#include <format>
#include <string>
#include <utility>
#include <vector>

using namespace huxerui;

namespace hui::ui {
namespace {

[[nodiscard]] View RowFor(const Editor& ed, const doc::Node& node, std::size_t depth) {
  const std::string id = node.id;
  const catalog::ComponentDef* def = catalog::Find(node.type);
  const std::string summary = doc::SummaryOf(node);

  View row = Row{
      Text(def != nullptr ? def->glyph : "?", TextRole::Label).With(Frame{.width = 18.0F}),
      Text(node.type, TextRole::Label),
      Text(summary == node.type ? std::string() : summary, TextRole::Label).With(Opacity(0.55F), Grow(1.0F)),
      // Bound events are worth seeing here: they are the reason a node behaves
      // differently from its siblings in the running application.
      Text(node.events.empty() ? std::string() : std::format("⚡ {}", node.events.size()), TextRole::Label)
          .With(Opacity(0.6F)),
      Text(id, TextRole::Label).With(Opacity(0.4F)),
  }
      .With(Spacing(6.0F),
            Padding(EdgeInsets{.top = 3.0F, .bottom = 3.0F,
                               .left = 4.0F + static_cast<float>(depth) * 14.0F}),
            CrossAlign(CrossAxisAlignment::Center), CornerRadius(4.0F))
      // A row is a way to pick a node up as well as to select it: dragging one
      // onto a canvas container moves the node there, and it travels as itself.
      .With(DragSource(DropPayload{.move = true, .ref = id},
                       [ed, id]() -> View { return NodePreview(ed, id); }))
      .OnClick([ed, id] {
        ed.Select(id);
        ed.SetStatus("selected " + id);
      });
  if (ed.Selection() == id) {
    row = std::move(row).With(Background(ed.palette.accent_soft));
  }
  return std::move(row).Key(id);
}

void RowsFor(const Editor& ed, const doc::Node& node, std::size_t depth, std::vector<View>& rows) {
  rows.push_back(RowFor(ed, node, depth));
  for (const auto& child : node.children) {
    RowsFor(ed, child, depth + 1, rows);
  }
}

}  // namespace

View StructureView(const Editor& ed) {
  const doc::Document& document = ed.Document();

  std::vector<View> rows;
  RowsFor(ed, document.root, 0, rows);

  return Column{
      Text("Structure", TextRole::Title),
      Column(std::move(rows)).With(Spacing(1.0F)),
  }
      .With(Spacing(6.0F))
      .Key("structure");
}

}  // namespace hui::ui
