#include "ui/widgets.h"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <format>
#include <memory>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using namespace huxerui;

namespace hui::ui {

[[huxerui::composable]]
View StringField(std::string placeholder, std::string current, std::function<void(std::string)> commit) {
  auto value = UseState(TextEditingValue::FromText(current));
  return TextField(value.Get())
      .Placeholder(std::move(placeholder))
      .OnChanged([value, commit = std::move(commit)](const TextEditingValue& next) mutable {
        value = next;
        commit(next.text);
      });
}

[[huxerui::composable]]
View NumberField(std::string placeholder, bool set, double current,
                 std::function<void(std::optional<double>)> commit) {
  auto value = UseState(TextEditingValue::FromText(set ? std::format("{}", current) : std::string()));
  return TextField(value.Get())
      .Placeholder(std::move(placeholder))
      .OnChanged([value, commit = std::move(commit)](const TextEditingValue& next) mutable {
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
View BoolField(bool current, std::function<void(bool)> commit) {
  return Switch(current).OnChanged([commit = std::move(commit)](bool next) mutable { commit(next); });
}

[[huxerui::composable]]
View EnumField(std::vector<std::string> values, std::string current, std::function<void(std::string)> commit) {
  const auto found = std::ranges::find(values, current);
  const std::size_t selected =
      found == values.end() ? 0 : static_cast<std::size_t>(found - values.begin());
  auto choices = std::make_shared<const std::vector<std::string>>(values);
  return Select(std::move(values), selected, [](const std::string& option) { return Text(option); })
      .OnChanged([choices = std::move(choices), commit = std::move(commit)](std::size_t index) mutable {
        if (index < choices->size()) {
          commit((*choices)[index]);
        }
      });
}

[[huxerui::composable]]
View StrListField(std::vector<std::string> current, std::function<void(std::vector<std::string>)> commit) {
  auto value = UseState(TextEditingValue::FromText(std::ranges::fold_left(
      current, std::string(), [](const std::string& accumulated, const std::string& item) {
        return accumulated.empty() ? item : accumulated + ", " + item;
      })));
  return TextField(value.Get())
      .Placeholder("Option A, Option B")
      .OnChanged([value, commit = std::move(commit)](const TextEditingValue& next) mutable {
        value = next;
        std::vector<std::string> items;
        std::string item;
        std::istringstream stream(next.text);
        while (std::getline(stream, item, ',')) {
          const std::size_t first = item.find_first_not_of(' ');
          const std::size_t last = item.find_last_not_of(' ');
          if (first != std::string::npos) {
            items.push_back(item.substr(first, last - first + 1));
          }
        }
        commit(std::move(items));
      });
}

}  // namespace hui::ui
