// The canvas island: the document rendered with the components it stands for.
//
// Every node becomes the actual View it will compile to, so what the designer
// shows is what the generated code produces. A node selects on click — the
// innermost binding wins, so the deepest node under the pointer is the one
// selected — drags to relocate, and any node the catalog calls a container
// accepts both a relocation and a card dragged out of the palette.

#include "ui/ui.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <utility>
#include <vector>

using namespace huxerui;

namespace hui::ui {
namespace {

[[nodiscard]] View BuildNode(const Editor& ed, const doc::Node& node);

/// Parses "#RRGGBB" / "#RRGGBBAA". The engine already validated the value, so a
/// malformed one here means the catalog and the document disagree: magenta makes
/// that visible on the canvas instead of silently painting black.
[[nodiscard]] Color HexColor(std::string_view hex) {
  std::string digits(hex);
  if (!digits.empty() && digits.front() == '#') {
    digits.erase(0, 1);
  }
  if (digits.size() == 6) {
    digits += "FF";
  }
  std::uint32_t value = 0xFF00FFFFu;
  std::from_chars(digits.data(), digits.data() + digits.size(), value, 16);
  return Color::Rgba32(value);
}

[[nodiscard]] Axis AxisOf(const std::string& value) {
  return value == "Horizontal" ? Axis::Horizontal : Axis::Vertical;
}

[[nodiscard]] View EmptyContainer(const Editor& ed, const doc::Node& node) {
  return Text(std::format("drop {} children here", node.type), TextRole::Label)
      .With(Background(ed.palette.accent_soft), CornerRadius(6.0F), Padding(16.0F));
}

[[nodiscard]] std::vector<View> BuildChildren(const Editor& ed, const std::vector<doc::Node>& children) {
  std::vector<View> views;
  views.reserve(children.size());
  for (const auto& child : children) {
    views.push_back(BuildNode(ed, child));
  }
  return views;
}

/// One node's component, configured from its props but before user modifiers
/// and before the editor's own selection and drop chrome is attached.
[[nodiscard]] View BuildComponent(const Editor& ed, const doc::Node& node) {
  const std::string& type = node.type;

  if (type == "Column" || type == "Row" || type == "Flow" || type == "Stack") {
    std::vector<View> views = BuildChildren(ed, node.children);
    if (views.empty()) {
      views.push_back(EmptyContainer(ed, node));
    }
    if (type == "Column") {
      return Column(std::move(views));
    }
    if (type == "Row") {
      return Row(std::move(views));
    }
    if (type == "Flow") {
      return Flow(std::move(views));
    }
    return Stack(std::move(views));
  }
  if (type == "Text") {
    const std::string role = doc::EnumOf(node, "role");
    return Text(doc::TextOf(node, "text"), role == "Title"     ? TextRole::Title
                                            : role == "Label" ? TextRole::Label
                                                              : TextRole::Body);
  }
  if (type == "Divider") {
    return Divider(AxisOf(doc::EnumOf(node, "axis")));
  }
  if (type == "Spacer") {
    return Spacer();
  }
  if (type == "Button") {
    return Button(doc::TextOf(node, "label"));
  }
  if (type == "Checkbox" || type == "RadioButton" || type == "Switch") {
    const std::string label = doc::TextOf(node, "label");
    const bool state = doc::BoolOf(node, type == "RadioButton" ? "selected" : "checked");
    if (type == "Checkbox") {
      return label.empty() ? Checkbox(state) : Checkbox(label, state);
    }
    if (type == "Switch") {
      return label.empty() ? Switch(state) : Switch(label, state);
    }
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
      return Text("Select needs options", TextRole::Label)
          .With(Background(ed.palette.danger), Foreground(Color::Rgba32(0xFFFFFFFF)));
    }
    const auto selected = static_cast<std::size_t>(
        std::clamp<double>(doc::NumberOf(node, "selected", 0.0), 0.0, static_cast<double>(options.size() - 1)));
    return Select(std::move(options), selected, [](const std::string& option) { return Text(option); });
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
    View content = node.children.empty() ? EmptyContainer(ed, node) : BuildNode(ed, node.children.front());
    ScrollView view(std::move(content));
    if (doc::EnumOf(node, "axis") == "Horizontal") {
      view = std::move(view).ScrollAxis(Axis::Horizontal);
    }
    return std::move(view).With(Frame{.height = 240.0F});
  }
  if (type == "MaterialTheme" || type == "MaterialDarkTheme" || type == "FlatTheme" || type == "FlatDarkTheme") {
    View content = node.children.empty() ? EmptyContainer(ed, node) : BuildNode(ed, node.children.front());
    if (type == "MaterialTheme") {
      return MaterialTheme(std::move(content));
    }
    if (type == "MaterialDarkTheme") {
      return MaterialDarkTheme(std::move(content));
    }
    if (type == "FlatTheme") {
      return FlatTheme(std::move(content));
    }
    return FlatDarkTheme(std::move(content));
  }
  return Text("unknown component: " + type, TextRole::Label)
      .With(Background(ed.palette.danger), Foreground(Color::Rgba32(0xFFFFFFFF)));
}

/// Container configuration props, in the order the code generator emits them.
[[nodiscard]] View ApplyContainerProps(View view, const doc::Node& node) {
  if (node.type == "Column" || node.type == "Row" || node.type == "Flow") {
    if (doc::NumberSet(node, "spacing")) {
      view = std::move(view).With(Spacing(static_cast<float>(doc::NumberOf(node, "spacing", 0.0))));
    }
    const std::string main = doc::EnumOf(node, "mainAlign");
    if (main == "Center") {
      view = std::move(view).With(MainAlign(MainAxisAlignment::Center));
    } else if (main == "End") {
      view = std::move(view).With(MainAlign(MainAxisAlignment::End));
    } else if (main == "SpaceBetween") {
      view = std::move(view).With(MainAlign(MainAxisAlignment::SpaceBetween));
    } else if (main == "SpaceAround") {
      view = std::move(view).With(MainAlign(MainAxisAlignment::SpaceAround));
    } else if (main == "SpaceEvenly") {
      view = std::move(view).With(MainAlign(MainAxisAlignment::SpaceEvenly));
    }
    const std::string cross = doc::EnumOf(node, "crossAlign");
    if (cross == "Center") {
      view = std::move(view).With(CrossAlign(CrossAxisAlignment::Center));
    } else if (cross == "End") {
      view = std::move(view).With(CrossAlign(CrossAxisAlignment::End));
    } else if (cross == "Stretch") {
      view = std::move(view).With(CrossAlign(CrossAxisAlignment::Stretch));
    }
    return view;
  }
  if (node.type == "Stack") {
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
    return std::move(view).With(
        Align(horizontal(doc::EnumOf(node, "alignH")), vertical(doc::EnumOf(node, "alignV"))));
  }
  return view;
}

/// The node's own modifiers, applied in the order the generator emits them.
[[nodiscard]] View ApplyModifiers(View view, const doc::Node& node) {
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
  return view;
}

[[nodiscard]] View BuildNode(const Editor& ed, const doc::Node& node) {
  const catalog::ComponentDef* def = catalog::Find(node.type);
  const bool container = def != nullptr && def->children != catalog::ChildrenPolicy::None;

  View view = ApplyContainerProps(ApplyModifiers(BuildComponent(ed, node), node), node);

  // Relocation drag: press briefly, then drag. The pause keeps an ordinary click
  // — which selects — from being read as the start of a move.
  const std::string summary = doc::SummaryOf(node);
  const std::string glyph = def != nullptr ? def->glyph : "?";
  view = std::move(view).With(DragSource(
      DropPayload{.move = true, .ref = node.id},
      [type = node.type, glyph, card_bg = ed.palette.card_bg, accent = ed.palette.accent, summary] {
        return Row{
            Text(glyph + " " + type, TextRole::Label),
            Text(summary, TextRole::Label).With(Opacity(0.6F)),
        }
            .With(Spacing(8.0F), Padding(8.0F), CornerRadius(6.0F), Background(card_bg),
                  Border{.color = accent, .width = 1.0F});
      },
      DragGesture{.minimum_press_duration = std::chrono::milliseconds(120)}));

  // A container accepts a card from the palette (create) and a node already on
  // the canvas (relocate) through the same payload type.
  if (container) {
    const std::string parent_id = node.id;
    view = std::move(view)
               .With(DropTarget::Accepts<DropPayload>())
               .On<DropEvents<DropPayload>::Entered>(
                   [ed, parent_id](const DropPayload&, const DropEvent&) { ed.SetHint(parent_id); })
               .On<DropEvents<DropPayload>::Exited>(
                   [ed, parent_id](const DropPayload&, const DropEvent&) {
                     if (ed.Hint() == parent_id) {
                       ed.SetHint(std::string());
                     }
                   })
               .On<DropEvents<DropPayload>::Dropped>(
                   [ed, parent_id](const DropPayload& payload, const DropEvent&) {
                     doc::Document next = ed.Document();
                     const std::string new_id = doc::NextId(next);
                     const doc::OperationResult result =
                         payload.move
                             ? doc::MoveNode(next, payload.ref, parent_id)
                             : doc::AddChild(next, parent_id, doc::MakeNode(payload.ref, new_id));
                     if (!result.ok) {
                       ed.SetStatus(result.error);
                       return;
                     }
                     ed.Apply(std::move(next));
                     ed.SetStatus(payload.move ? std::format("moved {} into {}", payload.ref, parent_id)
                                               : std::format("added {} as {} under {}", payload.ref, new_id,
                                                             parent_id));
                   });
    if (ed.Hint() == node.id) {
      view = std::move(view).With(Background(ed.palette.accent_soft));
    }
  }

  // Selection chrome last, so it is not covered by a container's drop tint.
  const std::string id = node.id;
  view = std::move(view).OnClick([ed, id] { ed.Select(id); });
  if (ed.Selection() == node.id) {
    view = std::move(view).With(Border{.color = ed.palette.accent, .width = 2.0F}, CornerRadius(4.0F));
  }

  return std::move(view).Key(node.id);
}

}  // namespace

View CanvasView(const Editor& ed) {
  const doc::Document& document = ed.Document();
  const std::string selected = ed.Selection();
  const doc::Node* selected_node = selected.empty() ? nullptr : doc::FindNode(document, selected);

  View device = BuildNode(ed, document.root);
  device = std::move(device).With(Frame{.width = 460.0F, .min_height = 640.0F}, Background(ed.palette.device_bg),
                                  Border{.color = ed.palette.card_border, .width = 1.0F}, CornerRadius(12.0F));

  const std::string status =
      selected_node == nullptr
          ? std::format("{} · {} child(ren)", document.name, document.root.children.size())
          : std::format("selected: {} [{}]", doc::SummaryOf(*selected_node), selected_node->id);

  return Column{
      ScrollView(Column{std::move(device)}
                     .With(Padding(24.0F), Spacing(16.0F), CrossAlign(CrossAxisAlignment::Center)))
          .With(Grow(1.0F)),
      Row{
          Text(status, TextRole::Label).With(Opacity(0.8F)),
          Spacer(),
          Text("drag a palette card onto a container, or drag a node to move it", TextRole::Label)
              .With(Opacity(0.5F)),
      }.With(Padding(EdgeInsets{.top = 6.0F}), Spacing(12.0F), CrossAlign(CrossAxisAlignment::Center)),
  }
      .With(Grow(1.0F))
      .Key("canvas");
}

}  // namespace hui::ui
