// The inspector panel: catalogue-driven editors for the selected node's props,
// modifiers, and events, plus document-level editing and the live issue list.

export module hui.ui.inspector;

import std;
import huxerui;
import hui.core.catalog;
import hui.core.doc;
import hui.core.validate;
import hui.ui.theme;
import hui.ui.editor;
import hui.ui.icons;
import hui.ui.widgets;

using namespace huxerui;

export namespace hui::inspector {

namespace detail {

inline void ApplyValue(const editor::Editor& ed, const std::string& id, const std::string& key,
                       doc::PropValue value, bool modifier) {
  doc::Document next = ed.document.Get();
  const doc::OperationResult result =
      modifier ? doc::SetModifier(next, id, key, std::move(value)) : doc::SetProp(next, id, key, std::move(value));
  if (result.ok) {
    ed.Apply(std::move(next));
  } else {
    ed.status = result.error;
  }
}

[[nodiscard]] inline View PropEditor(const editor::Editor& ed, const doc::Node& node,
                                     const catalog::PropDef& prop, bool modifier) {
  const std::string field_key = node.id + "#" + (modifier ? "m:" : "p:") + prop.key;
  const std::string id = node.id;
  const std::string key = prop.key;

  switch (prop.kind) {
    case catalog::PropKind::Text: {
      const std::string current = modifier ? doc::ModifierText(node, key) : doc::TextOf(node, key);
      return widgets::StringField(key, current, [ed, id, key, modifier](std::string next) {
        ApplyValue(ed, id, key, doc::PropValue(std::move(next)), modifier);
      }).Key(field_key);
    }
    case catalog::PropKind::Number: {
      const bool set = modifier ? doc::HasModifier(node, key) : doc::NumberSet(node, key);
      const double current = modifier ? doc::ModifierNumber(node, key, 0.0) : doc::NumberOf(node, key, 0.0);
      return widgets::NumberField(key, set, current, [ed, id, key, modifier](std::optional<double> next) {
        ApplyValue(ed, id, key, next.has_value() ? doc::PropValue(*next) : doc::PropValue(std::monostate{}), modifier);
      }).Key(field_key);
    }
    case catalog::PropKind::Bool: {
      const bool current = modifier ? doc::ModifierBool(node, key, false) : doc::BoolOf(node, key);
      return widgets::BoolField(key, current, [ed, id, key, modifier](bool next) {
        ApplyValue(ed, id, key, doc::PropValue(next), modifier);
      }).Key(field_key);
    }
    case catalog::PropKind::Enum: {
      std::string current = modifier ? doc::ModifierText(node, key) : doc::EnumOf(node, key);
      if (current.empty()) current = prop.default_value.AsString("Start");
      return widgets::EnumField(prop.enum_values, current, [ed, id, key](std::string next) {
        ApplyValue(ed, id, key, doc::PropValue(std::move(next)), false);
      }).Key(field_key);
    }
    case catalog::PropKind::StrList: {
      return widgets::StrListField(doc::ListOf(node, key), [ed, id, key](std::vector<std::string> next) {
        ApplyValue(ed, id, key, doc::PropValue(std::move(next)), false);
      }).Key(field_key);
    }
    case catalog::PropKind::Color: {
      const std::string current = modifier ? doc::ModifierText(node, key) : doc::TextOf(node, key);
      return widgets::StringField("#RRGGBB", current, [ed, id, key, modifier](std::string next) {
        ApplyValue(ed, id, key, doc::PropValue(std::move(next)), modifier);
      }).Key(field_key);
    }
  }
  return Text(prop.key, TextRole::Label);
}

/// One event row: a bind toggle, plus the handler name once bound. An empty
/// handler is legal and generates a TODO placeholder in the C++.
[[nodiscard]] inline View EventEditor(const editor::Editor& ed, const doc::Node& node,
                                      const catalog::EventDef& event) {
  const bool bound = doc::HasEvent(node, event.key);
  const std::string id = node.id;
  const std::string key = event.key;

  const auto set_bound = [ed, id, key](bool next) {
    doc::Document updated = ed.document.Get();
    doc::Node* target = doc::FindNode(updated, id);
    if (target == nullptr) return;
    if (next) {
      target->SetEvent(key, std::string());
    } else {
      target->RemoveEvent(key);
    }
    ed.Apply(std::move(updated));
  };
  const auto set_handler = [ed, id, key](std::string next) {
    doc::Document updated = ed.document.Get();
    doc::Node* target = doc::FindNode(updated, id);
    if (target == nullptr) return;
    target->SetEvent(key, std::move(next));
    ed.Apply(std::move(updated));
  };

  std::vector<View> rows;
  rows.push_back(widgets::FieldRow(key, widgets::BoolField("bound", bound, set_bound).Key(id + "#e:" + key)));
  if (bound) {
    rows.push_back(widgets::FieldRow(
        "handler",
        widgets::StringField("name, or empty for a TODO placeholder", doc::EventText(node, key), set_handler)
            .Key(id + "#eh:" + key)));
    rows.push_back(Text(std::format("{} — {}", event.signature, event.description), TextRole::Label)
                       .With(Opacity(0.5F), Padding(EdgeInsets{.left = 4.0F})));
  }
  return Column(std::move(rows)).With(Spacing(2.0F)).Key(id + "#event:" + key);
}

[[nodiscard]] inline View PageEditor(const editor::Editor& ed) {
  const doc::Document& document = ed.document.Get();
  return Column{
      Text("Document", TextRole::Title),
      widgets::FieldRow("name", widgets::StringField("C++ identifier", document.name,
                                                     [ed](std::string next) {
                                                       doc::Document updated = ed.document.Get();
                                                       updated.name = std::move(next);
                                                       ed.Apply(std::move(updated));
                                                     })
                                   .Key("page#name")),
      Text("select a node in the structure tree to edit it", TextRole::Label).With(Opacity(0.6F)),
  }
      .With(Spacing(8.0F))
      .Key("inspector-page");
}

[[nodiscard]] inline View IssueList(const doc::Document& document, const theme::Palette& palette) {
  const std::vector<validate::Issue> issues = validate::Check(document);
  std::vector<View> rows;
  if (issues.empty()) {
    rows.push_back(Text("no issues", TextRole::Label).With(Opacity(0.6F)));
  }
  for (const auto& issue : issues) {
    rows.push_back(Text(std::format("[{}] {}{}", issue.severity, issue.node_id.empty() ? "" : issue.node_id + ": ",
                                    issue.message),
                        TextRole::Label)
                       .With(issue.IsError() ? Foreground(palette.danger) : Foreground(palette.warning)));
  }
  return Column{
      Text("Issues", TextRole::Title),
      Column(std::move(rows)).With(Spacing(4.0F)),
  }
      .With(Spacing(8.0F))
      .Key("inspector-issues");
}

[[nodiscard]] inline View Section(std::string title, std::vector<View> rows, std::string key) {
  return Column{
      Text(std::move(title), TextRole::Label).With(Opacity(0.6F)),
      Column(std::move(rows)).With(Spacing(2.0F)),
  }
      .With(Spacing(6.0F))
      .Key(std::move(key));
}

}  // namespace detail

[[nodiscard]] inline View InspectorView(const editor::Editor& ed) {
  using namespace detail;

  const doc::Document& document = ed.document.Get();
  const std::string selected = ed.selection.Get();
  const doc::Node* node = selected.empty() ? nullptr : doc::FindNode(document, selected);

  View body;
  if (node == nullptr) {
    body = PageEditor(ed);
  } else {
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
        rows.push_back(widgets::FieldRow(prop.key, PropEditor(ed, *node, prop, false)));
      }
      sections.push_back(Section("Properties", std::move(rows), "inspector-props"));
    }

    std::vector<View> modifier_rows;
    for (const auto& modifier : catalog::Modifiers()) {
      modifier_rows.push_back(widgets::FieldRow(modifier.key, PropEditor(ed, *node, modifier, true)));
    }
    sections.push_back(Section("Modifiers", std::move(modifier_rows), "inspector-modifiers"));

    if (def != nullptr) {
      std::vector<View> event_rows;
      for (const auto* event : catalog::EventsOf(*def)) {
        event_rows.push_back(EventEditor(ed, *node, *event));
      }
      sections.push_back(Section("Events", std::move(event_rows), "inspector-events"));
    }

    std::vector<View> actions;
    if (!is_root) {
      actions.push_back(icons::Action(icons::ArrowUp(), "Move up").OnClick([ed, id = node->id] {
        doc::Document next = ed.document.Get();
        if (doc::ShiftNode(next, id, -1).ok) ed.Apply(std::move(next));
      }));
      actions.push_back(icons::Action(icons::ArrowDown(), "Move down").OnClick([ed, id = node->id] {
        doc::Document next = ed.document.Get();
        if (doc::ShiftNode(next, id, 1).ok) ed.Apply(std::move(next));
      }));
      actions.push_back(icons::Action(icons::Delete(), "Delete node").OnClick([ed, id = node->id] {
        doc::Document next = ed.document.Get();
        if (doc::RemoveNode(next, id).ok) {
          ed.Apply(std::move(next));
          ed.selection = "";
          ed.status = "deleted " + id;
        }
      }));
    }
    sections.push_back(Row(std::move(actions)).With(Spacing(8.0F)).Key("inspector-actions"));

    body = Column(std::move(sections)).With(Spacing(12.0F)).Key("inspector-node");
  }

  return Column{
      std::move(body),
      IssueList(document, ed.palette),
  }
      .With(Spacing(12.0F), Padding(8.0F), Background(ed.palette.panel_bg))
      .Key("inspector");
}

}  // namespace hui::inspector
