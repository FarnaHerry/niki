// The palette: one card per catalog component, grouped by category. Cards are
// drag sources onto canvas containers, and clicking one adds it to the current
// selection's container (or the root) — the keyboard- and touch-friendly path.

export module hui.ui.palette;

import std;
import huxerui;
import hui.core.catalog;
import hui.core.doc;
import hui.ui.canvas;
import hui.ui.widgets;

using namespace huxerui;

export namespace hui::palette {

namespace detail {

/// Adds one component under the selected container, or the root.
inline void AddComponent(const canvas::Editor& editor, const std::string& type) {
  doc::Document next = editor.doc.Get();
  const std::string selected = editor.selection.Get();
  std::string parent = next.root.id;
  if (const doc::Node* node = selected.empty() ? nullptr : doc::FindNode(next, selected)) {
    const catalog::ComponentDef* def = catalog::Find(node->type);
    if (def != nullptr && def->children != catalog::ChildrenPolicy::None) {
      parent = node->id;
    }
  }
  const std::string id = doc::NextId(next);
  if (doc::AddChild(next, parent, doc::MakeNode(type, id)).ok) {
    editor.apply(std::move(next));
    editor.selection = id;
  }
}

[[nodiscard]] inline View ComponentCard(const canvas::Editor& editor, const catalog::ComponentDef& component) {
  const std::string type = component.type;
  return Row{
      Text(component.glyph, TextRole::Label).With(Frame{.width = 24.0F}),
      Text(component.type, TextRole::Label).With(Grow(1.0F)),
  }
      .With(Spacing(8.0F), Padding(8.0F), CrossAlign(CrossAxisAlignment::Center),
            Background(Color::Rgba32(0xFFFFFFFF)), Border{.color = Color::Rgba32(0x22000000), .width = 1.0F},
            CornerRadius(6.0F))
      .With(DragSource(widgets::CanvasDrop{.move = false, .ref = type}, [type, glyph = component.glyph] {
        return Text(glyph + " " + type, TextRole::Label)
            .With(Padding(8.0F), Background(Color::Rgba32(0xFFF2F0FA)),
                  Border{.color = Color::Rgba32(0xFF2D6CDF), .width = 1.0F}, CornerRadius(6.0F));
      }))
      .OnClick([editor, type] { detail::AddComponent(editor, type); });
}

}  // namespace detail

[[nodiscard]] inline View PaletteView(const canvas::Editor& editor) {
  using namespace detail;

  std::vector<View> sections;
  std::string category;
  std::vector<View> cards;
  const auto flush = [&] {
    if (cards.empty()) return;
    sections.push_back(Flow(std::move(cards)).With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Stretch)));
    cards.clear();
  };
  for (const auto& component : catalog::All()) {
    if (component.category != category) {
      flush();
      category = component.category;
      sections.push_back(Text(category, TextRole::Label).With(Padding(4.0F), Opacity(0.6F)));
    }
    cards.push_back(ComponentCard(editor, component));
  }
  flush();

  return Column{
      Text("Components", TextRole::Title),
      Text("drag onto a container — or click to add", TextRole::Label).With(Opacity(0.6F)),
      Column(std::move(sections)).With(Spacing(6.0F)),
  }
      .With(Spacing(8.0F), Padding(8.0F))
      .Key("palette");
}

}  // namespace hui::palette
