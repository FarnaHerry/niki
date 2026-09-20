// The inspector island: catalog-driven editors for the selected node's
// properties, modifiers, and events, the document itself when nothing is
// selected, and the live issue list underneath both.

#include "ui/ui.h"

#include "ui/icons.h"
#include "ui/widgets.h"

#include <format>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

import hui.core.validate;

using namespace huxerui;

namespace hui::ui {
namespace {

/// Commits one property or modifier change, or reports why the engine refused.
void ApplyValue(const Editor& ed, const std::string& id, const std::string& key, doc::PropValue value,
                bool modifier) {
  doc::Document next = ed.Document();
  const doc::OperationResult result = modifier ? doc::SetModifier(next, id, key, std::move(value))
                                               : doc::SetProp(next, id, key, std::move(value));
  if (result.ok) {
    ed.Apply(std::move(next));
  } else {
    ed.SetStatus(result.error);
  }
}

/// The control for one catalog property, chosen by its declared kind. `modifier`
/// picks between a node's properties and its modifiers, which differ in where
/// the value lives rather than in how it is edited.
[[nodiscard]] View PropEditor(const Editor& ed, const doc::Node& node, const catalog::PropDef& prop,
                              bool modifier) {
  const std::string id = node.id;
  const std::string key = prop.key;
  const std::string field_key = id + "#" + (modifier ? "m:" : "p:") + key;

  switch (prop.kind) {
    case catalog::PropKind::Text: {
      const std::string current = modifier ? doc::ModifierText(node, key) : doc::TextOf(node, key);
      return StringField(key, current, [ed, id, key, modifier](std::string next) {
        ApplyValue(ed, id, key, doc::PropValue(std::move(next)), modifier);
      }).Key(field_key);
    }
    case catalog::PropKind::Number: {
      const bool set = modifier ? doc::HasModifier(node, key) : doc::NumberSet(node, key);
      const double current =
          modifier ? doc::ModifierNumber(node, key, 0.0) : doc::NumberOf(node, key, 0.0);
      return NumberField(key, set, current, [ed, id, key, modifier](std::optional<double> next) {
        ApplyValue(ed, id, key,
                   next.has_value() ? doc::PropValue(*next) : doc::PropValue(std::monostate{}), modifier);
      }).Key(field_key);
    }
    case catalog::PropKind::Bool: {
      const bool current = modifier ? doc::ModifierBool(node, key, false) : doc::BoolOf(node, key);
      return BoolField(current, [ed, id, key, modifier](bool next) {
        ApplyValue(ed, id, key, doc::PropValue(next), modifier);
      }).Key(field_key);
    }
    case catalog::PropKind::Enum: {
      std::string current = modifier ? doc::ModifierText(node, key) : doc::EnumOf(node, key);
      if (current.empty()) {
        current = prop.default_value.AsString("Start");
      }
      return EnumField(prop.enum_values, current, [ed, id, key, modifier](std::string next) {
        ApplyValue(ed, id, key, doc::PropValue(std::move(next)), modifier);
      }).Key(field_key);
    }
    case catalog::PropKind::StrList: {
      return StrListField(doc::ListOf(node, key), [ed, id, key, modifier](std::vector<std::string> next) {
        ApplyValue(ed, id, key, doc::PropValue(std::move(next)), modifier);
      }).Key(field_key);
    }
    case catalog::PropKind::Color: {
      const std::string current = modifier ? doc::ModifierText(node, key) : doc::TextOf(node, key);
      return StringField("#RRGGBB", current, [ed, id, key, modifier](std::string next) {
        ApplyValue(ed, id, key, doc::PropValue(std::move(next)), modifier);
      }).Key(field_key);
    }
  }
  return Text(prop.key, TextRole::Label);
}

/// One event row: whether the event is bound, and the handler name once it is.
/// An empty handler is legal and generates a TODO placeholder in the C++.
[[nodiscard]] View EventEditor(const Editor& ed, const doc::Node& node, const catalog::EventDef& event) {
  const bool bound = doc::HasEvent(node, event.key);
  const std::string id = node.id;
  const std::string key = event.key;

  const auto edit = [ed, id, key](const std::function<void(doc::Node&)>& mutate) {
    doc::Document next = ed.Document();
    doc::Node* target = doc::FindNode(next, id);
    if (target == nullptr) {
      return;
    }
    mutate(*target);
    ed.Apply(std::move(next));
  };

  std::vector<View> rows;
  rows.push_back(FieldRow(key, BoolField(bound, [edit, key](bool next) {
                            edit([&key, next](doc::Node& target) {
                              if (next) {
                                target.SetEvent(key, std::string());
                              } else {
                                target.RemoveEvent(key);
                              }
                            });
                          }).Key(id + "#e:" + key)));
  if (bound) {
    rows.push_back(FieldRow("handler", StringField("name, or empty for a TODO placeholder",
                                                   doc::EventText(node, key),
                                                   [edit, key](std::string next) {
                                                     edit([&key, &next](doc::Node& target) {
                                                       target.SetEvent(key, std::move(next));
                                                     });
                                                   })
                                      .Key(id + "#eh:" + key)));
    rows.push_back(Text(std::format("{} — {}", event.signature, event.description), TextRole::Label)
                       .With(Opacity(0.5F), Padding(EdgeInsets{.left = 4.0F})));
  }
  return Column(std::move(rows)).With(Spacing(2.0F)).Key(id + "#event:" + key);
}

/// What the inspector shows when nothing is selected: the document itself.
[[nodiscard]] View PageEditor(const Editor& ed) {
  return Column{
      Text("Document", TextRole::Title),
      FieldRow("name", StringField("C++ identifier", ed.Document().name, [ed](std::string next) {
                 doc::Document updated = ed.Document();
                 updated.name = std::move(next);
                 ed.Apply(std::move(updated));
               }).Key("page#name")),
      Text("select a node in the structure tree to edit it", TextRole::Label).With(Opacity(0.6F)),
  }
      .With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Stretch))
      .Key("inspector-page");
}

[[nodiscard]] View IssueList(const doc::Document& document, const theme::Palette& palette) {
  const std::vector<validate::Issue> issues = validate::Check(document);
  std::vector<View> rows;
  if (issues.empty()) {
    rows.push_back(Text("no issues", TextRole::Label).With(Opacity(0.6F)));
  }
  for (const auto& issue : issues) {
    rows.push_back(
        Text(std::format("[{}] {}{}", issue.severity, issue.node_id.empty() ? "" : issue.node_id + ": ",
                         issue.message),
             TextRole::Label)
            .With(issue.IsError() ? Foreground(palette.danger) : Foreground(palette.warning)));
  }
  return Column{
      Text("Issues", TextRole::Title),
      Column(std::move(rows)).With(Spacing(4.0F), CrossAlign(CrossAxisAlignment::Stretch)),
  }
      .With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Stretch))
      .Key("inspector-issues");
}

[[nodiscard]] View Section(std::string title, std::vector<View> rows, std::string key) {
  return Column{
      Text(std::move(title), TextRole::Label).With(Opacity(0.6F)),
      Column(std::move(rows)).With(Spacing(2.0F), CrossAlign(CrossAxisAlignment::Stretch)),
  }
      .With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Stretch))
      .Key(std::move(key));
}

/// The node's own actions. The root has none: the engine refuses to remove it
/// or move it among siblings, so offering the buttons would only offer errors.
[[nodiscard]] View NodeActions(const Editor& ed, const doc::Node& node) {
  const std::string id = node.id;
  return Row{
      icons::Action(icons::ArrowUp(), "Move up").OnClick([ed, id] {
        doc::Document next = ed.Document();
        if (doc::ShiftNode(next, id, -1).ok) {
          ed.Apply(std::move(next));
        }
      }),
      icons::Action(icons::ArrowDown(), "Move down").OnClick([ed, id] {
        doc::Document next = ed.Document();
        if (doc::ShiftNode(next, id, 1).ok) {
          ed.Apply(std::move(next));
        }
      }),
      icons::Action(icons::Delete(), "Delete node").OnClick([ed, id] {
        doc::Document next = ed.Document();
        const doc::OperationResult result = doc::RemoveNode(next, id);
        if (result.ok) {
          ed.Apply(std::move(next));
          ed.Select(std::string());
          ed.SetStatus("deleted " + id);
        } else {
          ed.SetStatus(result.error);
        }
      }),
  }
      .With(Spacing(8.0F))
      .Key("inspector-actions");
}

}  // namespace

View InspectorView(const Editor& ed) {
  const doc::Document& document = ed.Document();
  const std::string selected = ed.Selection();
  const doc::Node* node = selected.empty() ? nullptr : doc::FindNode(document, selected);

  if (node == nullptr) {
    return Column{PageEditor(ed), IssueList(document, ed.palette)}
        .With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Stretch))
        .Key("inspector");
  }

  const catalog::ComponentDef* def = catalog::Find(node->type);
  const bool is_root = document.root.id == node->id;

  std::vector<View> sections;
  sections.push_back(Row{
      Text(def != nullptr ? def->glyph : "?", TextRole::Title),
      Text(node->type, TextRole::Title),
      Text(node->id, TextRole::Label).With(Opacity(0.5F)),
  }
                         .With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center))
                         .Key("inspector-header"));

  if (def != nullptr && !def->props.empty()) {
    std::vector<View> rows;
    for (const auto& prop : def->props) {
      rows.push_back(FieldRow(prop.key, PropEditor(ed, *node, prop, false)));
    }
    sections.push_back(Section("Properties", std::move(rows), "inspector-props"));
  }

  std::vector<View> modifier_rows;
  for (const auto& modifier : catalog::Modifiers()) {
    modifier_rows.push_back(FieldRow(modifier.key, PropEditor(ed, *node, modifier, true)));
  }
  sections.push_back(Section("Modifiers", std::move(modifier_rows), "inspector-modifiers"));

  if (def != nullptr) {
    std::vector<View> event_rows;
    for (const auto* event : catalog::EventsOf(*def)) {
      event_rows.push_back(EventEditor(ed, *node, *event));
    }
    sections.push_back(Section("Events", std::move(event_rows), "inspector-events"));
  }

  if (!is_root) {
    sections.push_back(NodeActions(ed, *node));
  }

  return Column{Column(std::move(sections)).With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Stretch))
                    .Key("inspector-node"),
                IssueList(document, ed.palette)}
      .With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Stretch))
      .Key("inspector");
}

}  // namespace hui::ui
