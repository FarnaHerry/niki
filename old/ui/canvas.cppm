// The canvas: the document rendered live with real HuxerUI components.
//
// Drofooding is the point — every node the designer shows is the actual View
// it will become. Each rendered node carries click-to-select, a press-then-
// drag DragSource for relocation, and container nodes are typed DropTargets
// that accept palette drops and node moves.

export module hui.ui.canvas;

import std;
import huxerui;
import hui.core.catalog;
import hui.core.doc;
import hui.ui.widgets;

using namespace huxerui;

export namespace hui::canvas {

// Everything a rendered node needs to participate in the editor.
struct Editor final {
  State<doc::Document> doc;
  State<std::string> selection;
  State<std::string> drop_hint;
  std::function<void(doc::Document)> apply;  // commit a mutated document (with history)
};

[[nodiscard]] inline View BuildNode(const Editor& editor, const doc::Node& node);

namespace detail {

[[nodiscard]] inline Color HexColor(std::string_view hex) {
  std::string digits(hex.substr(1));
  if (digits.size() == 6) digits += "FF";
  std::uint32_t value = 0xFF00FF;
  std::from_chars(digits.data(), digits.data() + digits.size(), value, 16);
  return Color::Rgba32(value);
}

[[nodiscard]] inline Axis AxisOf(const std::string& value) {
  return value == "Horizontal" ? Axis::Horizontal : Axis::Vertical;
}

// Builds one node's component with its component-specific configuration,
// before user modifiers and editor chrome are attached.
[[nodiscard]] View BuildComponent(const Editor& editor, const doc::Node& node);

[[nodiscard]] inline View EmptyPlaceholder(const doc::Node& node) {
  return Text(std::format("drop {} children here", node.type), TextRole::Label)
      .With(Background(Color::Rgba32(0x14000000)), CornerRadius(6.0F), Padding(16.0F));
}

[[nodiscard]] inline std::vector<View> BuildChildren(const Editor& editor, const std::vector<doc::Node>& children) {
  std::vector<View> views;
  views.reserve(children.size());
  for (const auto& child : children) {
    views.push_back(BuildNode(editor, child));
  }
  return views;
}

[[nodiscard]] inline View BuildComponent(const Editor& editor, const doc::Node& node) {
  const std::string& type = node.type;

  if (type == "Column" || type == "Row" || type == "Flow" || type == "Stack") {
    std::vector<View> views = BuildChildren(editor, node.children);
    if (views.empty()) views.push_back(EmptyPlaceholder(node));
    if (type == "Column") return Column(std::move(views));
    if (type == "Row") return Row(std::move(views));
    if (type == "Flow") return Flow(std::move(views));
    return Stack(std::move(views));
  }
  if (type == "Text") {
    const std::string role = doc::EnumOf(node, "role");
    return Text(doc::TextOf(node, "text"), role == "Title"     ? TextRole::Title
                                            : role == "Label" ? TextRole::Label
                                                              : TextRole::Body);
  }
  if (type == "Divider") return Divider(AxisOf(doc::EnumOf(node, "axis")));
  if (type == "Spacer") return Spacer();
  if (type == "Button") return Button(doc::TextOf(node, "label"));
  if (type == "Checkbox" || type == "RadioButton" || type == "Switch") {
    const std::string label = doc::TextOf(node, "label");
    const bool state = doc::BoolOf(node, type == "RadioButton" ? "selected" : "checked");
    if (type == "Checkbox") return label.empty() ? Checkbox(state) : Checkbox(label, state);
    if (type == "Switch") return label.empty() ? Switch(state) : Switch(label, state);
    return label.empty() ? RadioButton(state) : RadioButton(label, state);
  }
  if (type == "Chip") {
    return doc::BoolOf(node, "selected") ? Chip(doc::TextOf(node, "label"), true)
                                         : Chip(doc::TextOf(node, "label"));
  }
  if (type == "Slider") {
    const double min = doc::NumberOf(node, "min", 0.0);
    const double max = std::max(doc::NumberOf(node, "max", 1.0), min + 0.0001);
    Slider slider(static_cast<float>(std::clamp(doc::NumberOf(node, "value", 0.5), min, max)));
    slider = std::move(slider).Range(static_cast<float>(min), static_cast<float>(max));
    if (doc::NumberSet(node, "step")) {
      slider = std::move(slider).Step(static_cast<float>(doc::NumberOf(node, "step", 0.0)));
    }
    return slider;
  }
  if (type == "ProgressBar") {
    return doc::NumberSet(node, "value")
               ? ProgressBar(static_cast<float>(std::clamp(doc::NumberOf(node, "value", 0.0), 0.0, 1.0)))
               : ProgressBar();
  }
  if (type == "ProgressCircle") {
    return doc::NumberSet(node, "value")
               ? ProgressCircle(static_cast<float>(std::clamp(doc::NumberOf(node, "value", 0.0), 0.0, 1.0)))
               : ProgressCircle();
  }
  if (type == "Select") {
    const std::vector<std::string> options = doc::ListOf(node, "options");
    if (options.empty()) {
      return Text("Select needs options", TextRole::Label).With(Background(Color::Rgba32(0x30CC4444)));
    }
    const auto selected = static_cast<std::size_t>(
        std::clamp<double>(doc::NumberOf(node, "selected", 0.0), 0.0, static_cast<double>(options.size() - 1)));
    return Select(options, selected, [](const std::string& option) { return Text(option); });
  }
  if (type == "TextField") {
    TextField field(TextEditingValue::FromText(doc::TextOf(node, "value")));
    if (!doc::TextOf(node, "label").empty()) {
      field = std::move(field).Label(doc::TextOf(node, "label"));
    }
    if (!doc::TextOf(node, "placeholder").empty()) {
      field = std::move(field).Placeholder(doc::TextOf(node, "placeholder"));
    }
    const std::string variant = doc::EnumOf(node, "variant");
    if (variant == "Outlined") {
      field = std::move(field).Variant(TextFieldVariant::Outlined);
    } else if (variant == "Standard") {
      field = std::move(field).Variant(TextFieldVariant::Standard);
    }
    return field;
  }
  if (type == "ScrollView") {
    View content = node.children.empty() ? EmptyPlaceholder(node) : BuildNode(editor, node.children.front());
    ScrollView view(std::move(content));
    if (doc::EnumOf(node, "axis") == "Horizontal") {
      view = std::move(view).ScrollAxis(Axis::Horizontal);
    }
    return std::move(view).With(Frame{.height = 240.0F});
  }
  if (type == "MaterialTheme" || type == "MaterialDarkTheme" || type == "FlatTheme" || type == "FlatDarkTheme") {
    View content = node.children.empty() ? EmptyPlaceholder(node) : BuildNode(editor, node.children.front());
    if (type == "MaterialTheme") return MaterialTheme(std::move(content));
    if (type == "MaterialDarkTheme") return MaterialDarkTheme(std::move(content));
    if (type == "FlatTheme") return FlatTheme(std::move(content));
    return FlatDarkTheme(std::move(content));
  }
  return Text("unknown component: " + type, TextRole::Label).With(Background(Color::Rgba32(0x30CC4444)));
}

}  // namespace detail

[[nodiscard]] inline View BuildNode(const Editor& editor, const doc::Node& node) {
  using namespace detail;

  const std::string selected = editor.selection.Get();
  const std::string hint = editor.drop_hint.Get();
  const catalog::ComponentDef* def = catalog::Find(node.type);

  View view = BuildComponent(editor, node);

  // Container configuration props, matching codegen emission order.
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
  {
    if (doc::HasModifier(node, "padding")) {
      view = std::move(view).With(Padding(static_cast<float>(doc::ModifierNumber(node, "padding", 0.0))));
    }
    if (doc::HasModifier(node, "width") || doc::HasModifier(node, "height")) {
      Frame frame{};
      if (doc::HasModifier(node, "width")) {
        frame.width = static_cast<float>(doc::ModifierNumber(node, "width", 0.0));
      }
      if (doc::HasModifier(node, "height")) {
        frame.height = static_cast<float>(doc::ModifierNumber(node, "height", 0.0));
      }
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
      const float width =
          static_cast<float>(doc::HasModifier(node, "borderWidth") ? doc::ModifierNumber(node, "borderWidth", 1.0) : 1.0);
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
  }

  // Relocation drag: press briefly, then drag. The pause keeps ordinary clicks
  // and canvas scrolling responsive.
  const std::string summary = doc::SummaryOf(node);
  view = std::move(view).With(DragSource(
      widgets::CanvasDrop{.move = true, .ref = node.id},
      [type = node.type, glyph = def != nullptr ? def->glyph : "?", summary] {
        return Row{Text(glyph + " " + type, TextRole::Label), Text(summary, TextRole::Label).With(Opacity(0.6F))}
            .With(Spacing(8.0F), Padding(8.0F), Background(Color::Rgba32(0xFFF2F0FA)),
                  Border{.color = Color::Rgba32(0xFF2D6CDF), .width = 1.0F}, CornerRadius(6.0F));
      },
      DragGesture{.minimum_press_duration = std::chrono::milliseconds(120)}));

  // Containers accept drops: palette creations and node moves land as children.
  if (def != nullptr && def->children != catalog::ChildrenPolicy::None) {
    const std::string parent_id = node.id;
    view = std::move(view).With(DropTarget::Accepts<widgets::CanvasDrop>());
    view = std::move(view)
               .On<DropEvents<widgets::CanvasDrop>::Entered>(
                   [drop_hint = editor.drop_hint, parent_id](const widgets::CanvasDrop&, const DropEvent&) {
                     drop_hint = parent_id;
                   })
               .On<DropEvents<widgets::CanvasDrop>::Exited>(
                   [drop_hint = editor.drop_hint, parent_id](const widgets::CanvasDrop&, const DropEvent&) {
                     if (drop_hint.Get() == parent_id) drop_hint = "";
                   })
               .On<DropEvents<widgets::CanvasDrop>::Dropped>(
                   [doc_state = editor.doc, apply = editor.apply, parent_id](const widgets::CanvasDrop& payload,
                                                                             const DropEvent&) {
                     doc::Document next = doc_state.Get();
                     doc::OperationResult result = payload.move
                                                           ? doc::MoveNode(next, payload.ref, parent_id)
                                                           : doc::AddChild(next, parent_id,
                                                                           doc::MakeNode(payload.ref,
                                                                                         doc::NextId(next)));
                     if (result.ok) apply(std::move(next));
                   });
    if (hint == node.id) {
      view = std::move(view).With(Background(Color::Rgba32(0x2E2D6CDF)));
    }
  }

  // Selection: deepest OnClick binding wins, so a click selects the deepest
  // node under the pointer.
  const std::string id = node.id;
  view = std::move(view).OnClick([selection = editor.selection, id] { selection = id; });
  if (selected == node.id) {
    view = std::move(view).With(
        Border{.color = Color::Rgba32(0xFF2D6CDF), .width = 2.0F},
        CornerRadius(4.0F));
  }

  return std::move(view).Key(node.id);
}

/// The canvas panel: the live document in a fixed device frame.
[[nodiscard]] inline View CanvasView(const Editor& editor) {
  const doc::Document& document = editor.doc.Get();
  const std::string selected = editor.selection.Get();
  const catalog::ComponentDef* def = catalog::Find(document.root.type);
  const doc::Node* selected_node = selected.empty() ? nullptr : doc::FindNode(document, selected);

  View device = BuildNode(editor, document.root);
  device = std::move(device).With(
      Frame{.width = 460.0F, .min_height = 640.0F}, Background(Color::Rgba32(0xFFFFFFFF)),
      Border{.color = Color::Rgba32(0x33000000), .width = 1.0F}, CornerRadius(12.0F));

  const std::string status =
      selected_node == nullptr
          ? std::format("{} · {} child(ren)", document.name, document.root.children.size())
          : std::format("selected: {} [{}]", doc::SummaryOf(*selected_node), selected_node->id);
  const bool root_container = def != nullptr && def->children != catalog::ChildrenPolicy::None;

  return Column{
      ScrollView(
          Column{std::move(device)}.With(Padding(24.0F), Spacing(16.0F), CrossAlign(CrossAxisAlignment::Center)))
          .With(Grow(1.0F)),
      Row{
          Text(status, TextRole::Label).With(Opacity(0.8F)),
          Spacer(),
          Text(root_container ? "drag from the palette, or click a palette item to add"
                              : "the root is a leaf; drag components onto a container",
               TextRole::Label)
              .With(Opacity(0.5F)),
      }.With(Padding(8.0F), Spacing(12.0F), CrossAlign(CrossAxisAlignment::Center)),
  }
      .With(Grow(1.0F), Background(Color::Rgba32(0xFFF7F6FB)));
}

}  // namespace hui::canvas
