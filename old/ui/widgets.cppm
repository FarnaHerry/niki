// Shared designer UI vocabulary: the drag-and-drop payload types and the
// property editor fields the inspector builds documents out of.

export module hui.ui.widgets;

import std;
import huxerui;

using namespace huxerui;

export namespace hui::widgets {

// The one drag payload the canvas speaks: palette items create (move=false,
// ref = component type), existing nodes relocate (move=true, ref = node id).
// One payload type keeps one DropTarget per container node.
struct CanvasDrop final {
  bool move = false;
  std::string ref;
};

[[nodiscard]] inline View SectionTitle(std::string text) {
  return Text(std::move(text), TextRole::Label).With(Padding(8.0F), FontSize(12.0F), Opacity(0.7F));
}

[[nodiscard]] inline View FieldRow(std::string label, View control) {
  return Row{Text(std::move(label), TextRole::Label).With(Frame{.width = 96.0F}),
             std::move(control).With(Grow(1.0F))}
      .With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center), Padding(4.0F));
}

/// One-line controlled text field with its own editing state.
/// Key it by the owning node and field so switching selection resets it.
[[huxerui::composable]]
View StringField(std::string placeholder, std::string current, std::function<void(std::string)> commit) {
  auto value = UseState(TextEditingValue::FromText(current));
  return TextField(value.Get())
      .Placeholder(std::move(placeholder))
      .OnChanged([value, commit](const TextEditingValue& next) mutable {
        value = next;
        commit(next.text);
      });
}

/// Numeric field: empty means "unset" for optional values.
[[huxerui::composable]]
View NumberField(std::string placeholder, bool set, double current, std::function<void(std::optional<double>)> commit) {
  auto value = UseState(TextEditingValue::FromText(set ? std::format("{}", current) : ""));
  return TextField(value.Get())
      .Placeholder(std::move(placeholder))
      .OnChanged([value, commit](const TextEditingValue& next) mutable {
        value = next;
        const std::string& text = next.text;
        if (text.empty()) {
          commit(std::nullopt);
          return;
        }
        double parsed = 0.0;
        const auto [pointer, error] = std::from_chars(text.data(), text.data() + text.size(), parsed);
        if (error == std::errc{} && pointer == text.data() + text.size()) {
          commit(parsed);
        }
      });
}

[[huxerui::composable]]
View BoolField(std::string label, bool current, std::function<void(bool)> commit) {
  return Switch(std::move(label), current).OnChanged([commit](bool next) mutable { commit(next); });
}

/// Controlled enum picker; values never empty (catalog guarantees enum props carry values).
[[huxerui::composable]]
View EnumField(std::vector<std::string> values, std::string current, std::function<void(std::string)> commit) {
  const auto found = std::ranges::find(values, current);
  const std::size_t selected = found == values.end() ? 0 : static_cast<std::size_t>(found - values.begin());
  auto shared_values = std::make_shared<std::vector<std::string>>(values);
  return Select(std::move(values), selected, [](const std::string& option) { return Text(option); })
      .OnChanged([shared_values = std::move(shared_values), commit = std::move(commit)](std::size_t index) mutable {
        if (index < shared_values->size()) commit((*shared_values)[index]);
      });
}

/// Comma-separated list editor for StrList props.
[[huxerui::composable]]
View StrListField(std::vector<std::string> current, std::function<void(std::vector<std::string>)> commit) {
  auto value = UseState(TextEditingValue::FromText(std::ranges::fold_left(
      current, std::string(), [](const std::string& acc, const std::string& item) {
        return acc.empty() ? item : acc + ", " + item;
      })));
  return TextField(value.Get())
      .Placeholder("Option A, Option B")
      .OnChanged([value, commit](const TextEditingValue& next) mutable {
        value = next;
        std::vector<std::string> items;
        std::string item;
        std::istringstream stream(next.text);
        while (std::getline(stream, item, ',')) {
          const std::size_t first = item.find_first_not_of(' ');
          const std::size_t last = item.find_last_not_of(' ');
          if (first != std::string::npos) items.push_back(item.substr(first, last - first + 1));
        }
        commit(std::move(items));
      });
}

}  // namespace hui::widgets
