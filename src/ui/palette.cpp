#include "ui/ui.h"

#include <format>
#include <string>
#include <utility>
#include <vector>

using namespace huxerui;

namespace hui::ui {
namespace {

/// Appends one component: into the selected container when it accepts children,
/// otherwise under the document root.
void AddComponent(const Editor& ed, const std::string& type) {
  doc::Document next = ed.document.Get();
  std::string parent = next.root.id;
  const std::string selected = ed.selection.Get();
  if (const doc::Node* node = selected.empty() ? nullptr : doc::FindNode(next, selected)) {
    const catalog::ComponentDef* def = catalog::Find(node->type);
    if (def != nullptr &&
        (def->children == catalog::ChildrenPolicy::Any ||
         (def->children == catalog::ChildrenPolicy::Single && node->children.empty()))) {
      parent = node->id;
    }
  }
  const std::string id = doc::NextId(next);
  const doc::OperationResult result = doc::AddChild(next, parent, doc::MakeNode(type, id));
  if (!result.ok) {
    ed.status = result.error;
    return;
  }
  ed.Apply(std::move(next));
  ed.selection = id;
  ed.status = std::format("added {} as {} under {}", type, id, parent);
}

[[nodiscard]] View Card(const Editor& ed, const catalog::ComponentDef& component) {
  const std::string type = component.type;
  return Row{
      Text(component.glyph, TextRole::Label).With(Frame{.width = 22.0F}),
      Text(component.type, TextRole::Label).With(Grow(1.0F)),
  }
      .With(Spacing(8.0F), Padding(6.0F), CrossAlign(CrossAxisAlignment::Center),
            Background(ed.palette.card_bg), Border{.color = ed.palette.card_border, .width = 1.0F},
            CornerRadius(6.0F))
      .OnClick([ed, type] { AddComponent(ed, type); });
}

}  // namespace

View PaletteView(const Editor& ed) {
  std::vector<View> rows;
  std::string category;
  for (const auto& component : catalog::All()) {
    if (component.category != category) {
      category = component.category;
      rows.push_back(Text(category, TextRole::Label).With(Padding(4.0F), Opacity(0.6F)));
    }
    rows.push_back(Card(ed, component));
  }

  return Column{
      Text("Components", TextRole::Title),
      Text("click a card to append it to the document", TextRole::Label).With(Opacity(0.6F)),
      Column(std::move(rows)).With(Spacing(4.0F)),
  }
      .With(Spacing(8.0F))
      .Key("palette");
}

}  // namespace hui::ui
