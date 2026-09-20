// The canvas panel: the document rendered live as real HuxerUI components.
//
// Every node in the document becomes the actual View it would compile to, so
// what the designer shows is what the user gets. Clicking a node selects it;
// the deepest binding wins, so a click selects the innermost node under the
// pointer. Drag-and-drop arrives in a later step.

export module hui.ui.canvas;

import std;
import huxerui;
import hui.core.catalog;
import hui.core.doc;
import hui.ui.editor;

using namespace huxerui;

export namespace hui::canvas {

[[nodiscard]] inline View BuildNode(const editor::Editor& ed, const doc::Node& node);

namespace detail {

/// Parses "#RRGGBB" / "#RRGGBBAA" (Color::Rgba32 is 0xRRGGBBAA). Magenta marks
/// a malformed value so it is obvious on the canvas instead of silently black.
[[nodiscard]] inline Color HexColor(std::string_view hex) {
  std::string digits(hex.substr(1));
  if (digits.size() == 6) digits += "FF";
  std::uint32_t value = 0xFF00FFFFu;
  std::from_chars(digits.data(), digits.data() + digits.size(), value, 16);
  return Color::Rgba32(value);
}

[[nodiscard]] inline Axis AxisOf(const std::string& value) {
  return value == "Horizontal" ? Axis::Horizontal : Axis::Vertical;
}

[[nodiscard]] inline View EmptyContainer(const doc::Node& node) {
  return Text(std::format("empty {}", node.type), TextRole::Label)
      .With(Background(Color::Rgba32(0x00000014)), CornerRadius(6.0F), Padding(16.0F));
}

[[nodiscard]] inline std::vector<View> BuildChildren(const editor::Editor& ed,
                                                     const std::vector<doc::Node>& children) {
  std::vector<View> views;
  views.reserve(children.size());
  for (const auto& child : children) views.push_back(BuildNode(ed, child));
  return views;
}

/// One document node as the HuxerUI component it stands for.
[[nodiscard]] inline View BuildComponent(const editor::Editor& ed, const doc::Node& node) {
  const std::string& type = node.type;

  if (type == "Divider") return Divider(AxisOf(doc::EnumOf(node, "axis")));
  if (type == "Spacer") return Spacer();
  if (type == "Button") return Button(doc::TextOf(node, "label"));

  return Text("unknown component: " + type, TextRole::Label).With(Background(Color::Rgba32(0xCC444430)));
}

}  // namespace detail

/// One node with its container props, user modifiers, and editor chrome.
[[nodiscard]] inline View BuildNode(const editor::Editor& ed, const doc::Node& node) {
  using namespace detail;

  View view = BuildComponent(ed, node);

  // Container configuration props, in the same order the code generator emits.
  if (node.type == "Column" || node.type == "Row" || node.type == "Flow") {
    if (doc::NumberSet(node, "spacing")) {
      view = std::move(view).With(Spacing(static_cast<float>(doc::NumberOf(node, "spacing", 0.0))));
    }
    const std::string main = doc::EnumOf(node, "mainAlign");
    if (main == "Center") view = std::move(view).With(MainAlign(MainAxisAlignment::Center));
    else if (main == "End") view = std::move(view).With(MainAlign(MainAxisAlignment::End));
    else if (main == "SpaceBetween") view = std::move(view).With(MainAlign(MainAxisAlignment::SpaceBetween));
    else if (main == "SpaceAround") view = std::move(view).With(MainAlign(MainAxisAlignment::SpaceAround));
    else if (main == "SpaceEvenly") view = std::move(view).With(MainAlign(MainAxisAlignment::SpaceEvenly));
    const std::string cross = doc::EnumOf(node, "crossAlign");
    if (cross == "Center") view = std::move(view).With(CrossAlign(CrossAxisAlignment::Center));
    else if (cross == "End") view = std::move(view).With(CrossAlign(CrossAxisAlignment::End));
    else if (cross == "Stretch") view = std::move(view).With(CrossAlign(CrossAxisAlignment::Stretch));
  } else if (node.type == "Stack") {
    const auto horizontal = [](const std::string& value) {
      if (value == "Center") return HorizontalAlignment::Center;
      if (value == "End") return HorizontalAlignment::End;
      if (value == "Stretch") return HorizontalAlignment::Stretch;
      return HorizontalAlignment::Start;
    };
    const auto vertical = [](const std::string& value) {
      if (value == "Center") return VerticalAlignment::Center;
      if (value == "End") return VerticalAlignment::End;
      if (value == "Stretch") return VerticalAlignment::Stretch;
      return VerticalAlignment::Start;
    };
    view = std::move(view).With(
        Align(horizontal(doc::EnumOf(node, "alignH")), vertical(doc::EnumOf(node, "alignV"))));
  }

  // User modifiers.
  if (doc::HasModifier(node, "padding")) {
    view = std::move(view).With(Padding(static_cast<float>(doc::ModifierNumber(node, "padding", 0.0))));
  }
  if (doc::HasModifier(node, "width") || doc::HasModifier(node, "height")) {
    Frame frame{};
    if (doc::HasModifier(node, "width")) frame.width = static_cast<float>(doc::ModifierNumber(node, "width", 0.0));
    if (doc::HasModifier(node, "height")) frame.height = static_cast<float>(doc::ModifierNumber(node, "height", 0.0));
    view = std::move(view).With(frame);
  }
  if (doc::HasModifier(node, "cornerRadius")) {
    view = std::move(view).With(CornerRadius(static_cast<float>(doc::ModifierNumber(node, "cornerRadius", 0.0))));
  }
  if (doc::HasModifier(node, "background")) {
    view = std::move(view).With(Background(HexColor(doc::ModifierText(node, "background"))));
  }
  if (doc::HasModifier(node, "foreground")) {
    view = std::move(view).With(Foreground(HexColor(doc::ModifierText(node, "foreground"))));
  }
  if (doc::HasModifier(node, "borderColor")) {
    const float width = static_cast<float>(
        doc::HasModifier(node, "borderWidth") ? doc::ModifierNumber(node, "borderWidth", 1.0) : 1.0);
    view = std::move(view).With(Border{.color = HexColor(doc::ModifierText(node, "borderColor")), .width = width});
  }
  if (doc::HasModifier(node, "fontSize")) {
    view = std::move(view).With(FontSize(static_cast<float>(doc::ModifierNumber(node, "fontSize", 14.0))));
  }
  if (doc::HasModifier(node, "grow")) {
    view = std::move(view).With(Grow(static_cast<float>(doc::ModifierNumber(node, "grow", 1.0))));
  }
  if (doc::HasModifier(node, "enabled") && !doc::ModifierBool(node, "enabled", true)) {
    view = std::move(view).With(Enabled{false});
  }

  // Selection: bound on every node, so the deepest node wins the click.
  const std::string id = node.id;
  view = std::move(view).OnClick([ed, id] {
    ed.selection = id;
    ed.status = "selected " + id;
  });
  if (ed.selection.Get() == node.id) {
    view = std::move(view).With(Border{.color = Color::Rgba32(0x2D6CDFFF), .width = 2.0F}, CornerRadius(4.0F));
  }

  return std::move(view).Key(node.id);
}

/// The canvas panel: the live document in a fixed device frame.
[[nodiscard]] inline View CanvasView(const editor::Editor& ed) {
  using namespace detail;

  const doc::Document& document = ed.doc.Get();
  const std::string selected = ed.selection.Get();
  const doc::Node* node = selected.empty() ? nullptr : doc::FindNode(document, selected);

  View device = BuildNode(ed, document.root);
  device = std::move(device).With(Frame{.width = 460.0F}, Background(Color::Rgba32(0xFFFFFFFF)),
                                 Border{.color = Color::Rgba32(0x00000022), .width = 1.0F}, CornerRadius(12.0F));

  const std::string caption =
      node == nullptr ? std::format("{} · {} child(ren)", document.name, document.root.children.size())
                      : std::format("selected {} [{}] — {}", doc::SummaryOf(*node), node->id, node->type);

  // Step 3a: deliberately conservative — no inner ScrollView and no min_height,
  // to isolate whether the blank page comes from this panel's size constraints.
  return Column{
      Column{std::move(device)}.With(Padding(24.0F), CrossAlign(CrossAxisAlignment::Center)),
      Row{
          Text(caption, TextRole::Label).With(Opacity(0.8F)),
          Spacer(),
          Text("click a node to select it", TextRole::Label).With(Opacity(0.5F)),
      }.With(Padding(8.0F), Spacing(12.0F), CrossAlign(CrossAxisAlignment::Center)),
  }.With(Grow(1.0F));
}

}  // namespace hui::canvas
