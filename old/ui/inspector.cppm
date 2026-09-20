// The inspector: catalog-driven editors for the selected node's props and
// modifiers, plus document-level editing and the live issue list.

export module hui.ui.inspector;

import std;
import huxerui;
import hui.core.catalog;
import hui.core.codegen;
import hui.core.doc;
import hui.core.validate;
import hui.ui.canvas;
import hui.ui.widgets;

using namespace huxerui;

export namespace hui::inspector {

namespace detail {

inline void ApplySetProp(const canvas::Editor& editor, const std::string& id, const std::string& key,
                         doc::PropValue value, bool modifier) {
  doc::Document next = editor.doc.Get();
  const doc::OperationResult result =
      modifier ? doc::SetModifier(next, id, key, std::move(value)) : doc::SetProp(next, id, key, std::move(value));
  if (result.ok) editor.apply(std::move(next));
}

[[nodiscard]] inline View PropEditor(const canvas::Editor& editor, const doc::Node& node, const catalog::PropDef& prop,
                                     bool modifier) {
  const std::string field_key = node.id + "#" + (modifier ? "m:" : "p:") + prop.key;

  switch (prop.kind) {
    case catalog::PropKind::Text: {
      std::string current = modifier ? doc::ModifierText(node, prop.key) : doc::TextOf(node, prop.key);
      if (modifier && current.empty() && !doc::HasModifier(node, prop.key)) current = "";
      return widgets::StringField(prop.key, current,
                                  [editor, id = node.id, key = prop.key](std::string next) {
                                    detail::ApplySetProp(editor, id, key, doc::PropValue(std::move(next)), false);
                                  })
          .Key(field_key);
    }
    case catalog::PropKind::Number: {
      const bool set = modifier ? doc::HasModifier(node, prop.key) : doc::NumberSet(node, prop.key);
      const double current =
          modifier ? doc::ModifierNumber(node, prop.key, 0.0) : doc::NumberOf(node, prop.key, 0.0);
      return widgets::NumberField(prop.key, set, current,
                                  [editor, id = node.id, key = prop.key, modifier](std::optional<double> next) {
                                    detail::ApplySetProp(
                                        editor, id, key,
                                        next.has_value() ? doc::PropValue(*next) : doc::PropValue(std::monostate{}),
                                        modifier);
                                  })
          .Key(field_key);
    }
    case catalog::PropKind::Bool: {
      const bool current = modifier ? doc::ModifierBool(node, prop.key, false) : doc::BoolOf(node, prop.key);
      return widgets::BoolField(prop.key, current,
                                [editor, id = node.id, key = prop.key, modifier](bool next) {
                                  detail::ApplySetProp(editor, id, key, doc::PropValue(next), modifier);
                                })
          .Key(field_key);
    }
    case catalog::PropKind::Enum: {
      std::string current = modifier ? doc::ModifierText(node, prop.key) : doc::EnumOf(node, prop.key);
      if (!modifier && current.empty()) current = prop.default_value.AsString("Start");
      return widgets::EnumField(prop.enum_values, current,
                                [editor, id = node.id, key = prop.key, modifier](std::string next) {
                                  detail::ApplySetProp(editor, id, key, doc::PropValue(std::move(next)), modifier);
                                })
          .Key(field_key);
    }
    case catalog::PropKind::StrList: {
      return widgets::StrListField(doc::ListOf(node, prop.key),
                                   [editor, id = node.id, key = prop.key](std::vector<std::string> next) {
                                     detail::ApplySetProp(editor, id, key, doc::PropValue(std::move(next)), false);
                                   })
          .Key(field_key);
    }
    case catalog::PropKind::Color: {
      const std::string current = modifier ? doc::ModifierText(node, prop.key)
                                           : (node.GetProp(prop.key) != nullptr ? doc::TextOf(node, prop.key) : "");
      return widgets::StringField("#RRGGBB", current,
                                  [editor, id = node.id, key = prop.key, modifier](std::string next) {
                                    detail::ApplySetProp(editor, id, key, doc::PropValue(std::move(next)), modifier);
                                  })
          .Key(field_key);
    }
  }
  return Text(prop.key, TextRole::Label);
}

/// One event row: a bind/unbind switch, plus the handler name once bound. An
/// empty name is legal and generates a placeholder lambda in the C++.
[[nodiscard]] inline View EventEditor(const canvas::Editor& editor, const doc::Node& node,
                                      const catalog::EventDef& event) {
  const bool bound = doc::HasEvent(node, event.key);
  const std::string id = node.id;
  const std::string key = event.key;
  const auto set_bound = [editor, id, key](bool next) {
    doc::Document updated = editor.doc.Get();
    doc::Node* target = doc::FindNode(updated, id);
    if (target == nullptr) return;
    if (next) {
      target->SetEvent(key, std::string());
    } else {
      target->RemoveEvent(key);
    }
    editor.apply(std::move(updated));
  };
  const auto set_handler = [editor, id, key](std::string next) {
    doc::Document updated = editor.doc.Get();
    doc::Node* target = doc::FindNode(updated, id);
    if (target == nullptr) return;
    target->SetEvent(key, std::move(next));
    editor.apply(std::move(updated));
  };

  std::vector<View> rows;
  rows.push_back(widgets::FieldRow(key, widgets::BoolField("bound", bound, set_bound).Key(id + "#e:" + key)));
  if (bound) {
    rows.push_back(widgets::FieldRow(
        "handler",
        widgets::StringField("name, or empty for a TODO placeholder", doc::EventText(node, key), set_handler)
            .Key(id + "#eh:" + key)));
    rows.push_back(
        Text(std::format("{} ({})", event.signature, event.description), TextRole::Label).With(Opacity(0.5F)));
  }
  return Column(std::move(rows)).With(Spacing(2.0F)).Key(id + "#event:" + key);
}

[[nodiscard]] inline View PageEditor(const canvas::Editor& editor) {
  const doc::Document& document = editor.doc.Get();
  return Column{
      Text("Document", TextRole::Title),
      widgets::FieldRow(
          "name",
          widgets::StringField("C++ identifier", document.name,
                               [editor](std::string next) {
                                 doc::Document updated = editor.doc.Get();
                                 updated.name = std::move(next);
                                 editor.apply(std::move(updated));
                               })
              .Key("page#name")),
      Text("select a node on the canvas or in the structure tree to edit it", TextRole::Label).With(Opacity(0.6F)),
  }
      .With(Spacing(8.0F))
      .Key("inspector-page");
}

[[nodiscard]] inline View IssueList(const doc::Document& document) {
  const auto issues = validate::Check(document);
  std::vector<View> rows;
  if (issues.empty()) {
    rows.push_back(Text("no issues", TextRole::Label).With(Opacity(0.6F)));
  }
  for (const auto& issue : issues) {
    rows.push_back(Text(std::format("[{}] {}{}", issue.severity, issue.node_id.empty() ? "" : issue.node_id + ": ",
                                    issue.message),
                        TextRole::Label)
                       .With(issue.IsError() ? Foreground(Color::Rgba32(0xFFB3261E)) : Foreground(Color::Rgba32(0xFF8A6D00))));
  }
  return Column{
      Text("Issues", TextRole::Title),
      Column(std::move(rows)).With(Spacing(4.0F)),
  }
      .With(Spacing(8.0F))
      .Key("inspector-issues");
}

}  // namespace detail

[[nodiscard]] inline View InspectorView(const canvas::Editor& editor) {
  using namespace detail;

  const doc::Document& document = editor.doc.Get();
  const std::string selected = editor.selection.Get();
  const doc::Node* node = selected.empty() ? nullptr : doc::FindNode(document, selected);

  View body;
  if (node == nullptr) {
    body = PageEditor(editor);
  } else {
    const catalog::ComponentDef* def = catalog::Find(node->type);
    const bool is_root = document.root.id == node->id;

    std::vector<View> prop_fields;
    if (def != nullptr) {
      for (const auto& prop : def->props) {
        prop_fields.push_back(PropEditor(editor, *node, prop, false));
      }
    }

    std::vector<View> modifier_fields;
    for (const auto& modifier : catalog::Modifiers()) {
      modifier_fields.push_back(PropEditor(editor, *node, modifier, true));
    }

    std::vector<View> event_fields;
    if (def != nullptr) {
      for (const auto* event : catalog::EventsOf(*def)) {
        event_fields.push_back(EventEditor(editor, *node, *event));
      }
    }

    std::vector<View> sections;
    sections.push_back(Row{
        Text(def != nullptr ? def->glyph : "?", TextRole::Title),
        Text(node->type, TextRole::Title),
        Text(node->id, TextRole::Label).With(Opacity(0.5F)),
    }
                           .With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center))
                           .Key("inspector-header"));

    if (!prop_fields.empty()) {
      std::vector<View> rows;
      for (std::size_t i = 0; i < prop_fields.size(); ++i) {
        rows.push_back(widgets::FieldRow(def->props[i].key, std::move(prop_fields[i])));
      }
      sections.push_back(Column{
          Text("Properties", TextRole::Label).With(Opacity(0.6F)),
          Column(std::move(rows)).With(Spacing(2.0F)),
      }
                             .With(Spacing(6.0F))
                             .Key("inspector-props"));
    }

    sections.push_back(Column{
        Text("Modifiers", TextRole::Label).With(Opacity(0.6F)),
        Column(std::move(modifier_fields)).With(Spacing(2.0F)),
    }
                           .With(Spacing(6.0F))
                           .Key("inspector-modifiers"));

    if (!event_fields.empty()) {
      sections.push_back(Column{
          Text("Events", TextRole::Label).With(Opacity(0.6F)),
          Column(std::move(event_fields)).With(Spacing(6.0F)),
      }
                             .With(Spacing(6.0F))
                             .Key("inspector-events"));
    }

    std::vector<View> actions;
    if (!is_root) {
      actions.push_back(Button("Up").OnClick([editor, id = node->id] {
        doc::Document next = editor.doc.Get();
        if (doc::ShiftNode(next, id, -1).ok) editor.apply(std::move(next));
      }));
      actions.push_back(Button("Down").OnClick([editor, id = node->id] {
        doc::Document next = editor.doc.Get();
        if (doc::ShiftNode(next, id, 1).ok) editor.apply(std::move(next));
      }));
      actions.push_back(Button("Delete").OnClick([editor, id = node->id, selection = editor.selection] {
        doc::Document next = editor.doc.Get();
        if (doc::RemoveNode(next, id).ok) {
          editor.apply(std::move(next));
          selection = "";
        }
      }));
    }
    sections.push_back(Row(std::move(actions)).With(Spacing(8.0F)).Key("inspector-actions"));

    body = Column(std::move(sections)).With(Spacing(12.0F)).Key("inspector-node");
  }

  return Column{
      std::move(body),
      Spacer(),
      IssueList(document),
  }
      .With(Spacing(8.0F), Padding(8.0F))
      .Key("inspector");
}

}  // namespace hui::inspector
