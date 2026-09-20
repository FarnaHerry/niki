// Shared designer field vocabulary: the controlled editors the inspector builds
// property, modifier, and event rows out of.
//
// Each field owns its local editing state, so a value being typed is not lost
// between renders, and commits the parsed result through a callback. The
// definitions are composables and live in the .cpp: a field that is edited in
// place needs a recomposition lifetime of its own. Callers key each field by
// node and field name, which is what moves that state when the selection moves.
#pragma once

#include <huxerui/huxerui.h>

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace hui::ui {

/// One labelled row: fixed-width label plus the control, which grows.
[[nodiscard]] inline huxerui::View FieldRow(std::string label, huxerui::View control) {
  return huxerui::Row{
      huxerui::Text(std::move(label), huxerui::TextRole::Label).With(huxerui::Frame{.width = 92.0F}),
      std::move(control).With(huxerui::Grow(1.0F)),
  }
      .With(huxerui::Spacing(8.0F), huxerui::CrossAlign(huxerui::CrossAxisAlignment::Center),
            huxerui::Padding(3.0F));
}

[[nodiscard]] huxerui::View StringField(std::string placeholder, std::string current,
                                        std::function<void(std::string)> commit);

/// Numeric field. Empty text means "unset", which is how an optional value is
/// removed again.
[[nodiscard]] huxerui::View NumberField(std::string placeholder, bool set, double current,
                                        std::function<void(std::optional<double>)> commit);

[[nodiscard]] huxerui::View BoolField(bool current, std::function<void(bool)> commit);

/// Enum picker. The catalog guarantees every enum carries at least one value.
[[nodiscard]] huxerui::View EnumField(std::vector<std::string> values, std::string current,
                                      std::function<void(std::string)> commit);

/// Comma-separated editor for list-valued properties.
[[nodiscard]] huxerui::View StrListField(std::vector<std::string> current,
                                         std::function<void(std::vector<std::string>)> commit);

}  // namespace hui::ui
