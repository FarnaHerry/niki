// The component catalog: one authoritative description of every HuxerUI
// component hui can place, drive, validate, and emit.
//
// The palette renders it, the inspector edits through it, the validator checks
// against it, the code generator emits from it, and the MCP server publishes
// it — so AI agents and humans read the same contract.

export module hui.core.catalog;

import std;
import hui.core.json;

export namespace hui::catalog {

enum class PropKind {
  Text,      // UTF-8 string
  Number,    // double, rendered and emitted as a C++ float literal
  Bool,      // controlled boolean
  Enum,      // one of enum_values
  StrList,   // list of strings (e.g. Select options), comma-separated in the inspector
  Color,     // "#RRGGBB" or "#RRGGBBAA"
};

struct PropDef final {
  std::string key;
  PropKind kind{};
  std::string description;
  std::vector<std::string> enum_values;  // PropKind::Enum only
  json::Value default_value{};           // json::Null means "no default; absent is meaningful"
};

/// One bindable callback. `key` is the document key ("onClick"), `method` the
/// HuxerUI DSL method that binds it ("OnClick"), and `signature` the handler
/// shape codegen documents in the generated module.
struct EventDef final {
  std::string key;
  std::string method;
  std::string signature;
  std::string description;
};

enum class ChildrenPolicy {
  None,    // leaf component
  Any,     // multi-child container
  Single,  // wraps exactly one child
};

struct ComponentDef final {
  std::string type;
  std::string category;
  std::string glyph;
  std::string description;
  ChildrenPolicy children{};
  std::vector<PropDef> props;
  std::vector<EventDef> events;
};

[[nodiscard]] inline const std::vector<ComponentDef>& All() {
  using PK = PropKind;
  using CP = ChildrenPolicy;
  static const std::vector<ComponentDef> components{
      ComponentDef{
          .type = "Column",
          .category = "Layout",
          .glyph = "▤",
          .description = "Measures and places children vertically in declaration order.",
          .children = CP::Any,
          .props =
              {
                  {.key = "spacing", .kind = PK::Number, .default_value = 0.0},
                  {.key = "mainAlign",
                   .kind = PK::Enum,
                   .enum_values = {"Start", "Center", "End", "SpaceBetween", "SpaceAround", "SpaceEvenly"},
                   .default_value = "Start"},
                  {.key = "crossAlign",
                   .kind = PK::Enum,
                   .enum_values = {"Start", "Center", "End", "Stretch"},
                   .default_value = "Start"},
              },
      },
      ComponentDef{
          .type = "Row",
          .category = "Layout",
          .glyph = "▥",
          .description = "Measures and places children horizontally in declaration order.",
          .children = CP::Any,
          .props =
              {
                  {.key = "spacing", .kind = PK::Number, .default_value = 0.0},
                  {.key = "mainAlign",
                   .kind = PK::Enum,
                   .enum_values = {"Start", "Center", "End", "SpaceBetween", "SpaceAround", "SpaceEvenly"},
                   .default_value = "Start"},
                  {.key = "crossAlign",
                   .kind = PK::Enum,
                   .enum_values = {"Start", "Center", "End", "Stretch"},
                   .default_value = "Start"},
              },
      },
      ComponentDef{
          .type = "Flow",
          .category = "Layout",
          .glyph = "≋",
          .description = "Places children in wrapping horizontal lines when width is bounded.",
          .children = CP::Any,
          .props =
              {
                  {.key = "spacing", .kind = PK::Number, .default_value = 0.0},
                  {.key = "mainAlign",
                   .kind = PK::Enum,
                   .enum_values = {"Start", "Center", "End", "SpaceBetween", "SpaceAround", "SpaceEvenly"},
                   .default_value = "Start"},
                  {.key = "crossAlign",
                   .kind = PK::Enum,
                   .enum_values = {"Start", "Center", "End", "Stretch"},
                   .default_value = "Start"},
              },
      },
      ComponentDef{
          .type = "Stack",
          .category = "Layout",
          .glyph = "◫",
          .description = "Overlays children in declaration order within shared bounds.",
          .children = CP::Any,
          .props =
              {
                  {.key = "alignH",
                   .kind = PK::Enum,
                   .enum_values = {"Start", "Center", "End", "Stretch"},
                   .default_value = "Start"},
                  {.key = "alignV",
                   .kind = PK::Enum,
                   .enum_values = {"Start", "Center", "End", "Stretch"},
                   .default_value = "Start"},
              },
      },
      ComponentDef{
          .type = "Text",
          .category = "Content",
          .glyph = "¶",
          .description = "Presents one paragraph of text with an optional typography role.",
          .children = CP::None,
          .props =
              {
                  {.key = "text", .kind = PK::Text, .default_value = "Text"},
                  {.key = "role", .kind = PK::Enum, .enum_values = {"Body", "Label", "Title"}, .default_value = "Body"},
              },
      },
      ComponentDef{
          .type = "Divider",
          .category = "Content",
          .glyph = "―",
          .description = "Draws a themed separator along the requested axis.",
          .children = CP::None,
          .props =
              {
                  {.key = "axis",
                   .kind = PK::Enum,
                   .enum_values = {"Horizontal", "Vertical"},
                   .default_value = "Horizontal"},
              },
      },
      ComponentDef{
          .type = "Spacer",
          .category = "Content",
          .glyph = "⇕",
          .description = "Consumes remaining main-axis space inside a Row or Column.",
          .children = CP::None,
          .props = {},
      },
      ComponentDef{
          .type = "Button",
          .category = "Input",
          .glyph = "▭",
          .description = "Presents a labeled action; bind activation with OnClick.",
          .children = CP::None,
          .props =
              {
                  {.key = "label", .kind = PK::Text, .default_value = "Button"},
              },
      },
      ComponentDef{
          .type = "Checkbox",
          .category = "Input",
          .glyph = "☑",
          .description = "Controlled checkbox, optionally with a leading label.",
          .children = CP::None,
          .props =
              {
                  {.key = "label", .kind = PK::Text, .default_value = ""},
                  {.key = "checked", .kind = PK::Bool, .default_value = false},
              },
          .events =
              {
                  {.key = "onChanged",
                   .method = "OnChanged",
                   .signature = "void(bool)",
                   .description = "Proposes the next checked value; the application owns the state."},
              },
      },
      ComponentDef{
          .type = "RadioButton",
          .category = "Input",
          .glyph = "⦿",
          .description = "Controlled radio button; group exclusivity stays application-owned.",
          .children = CP::None,
          .props =
              {
                  {.key = "label", .kind = PK::Text, .default_value = ""},
                  {.key = "selected", .kind = PK::Bool, .default_value = false},
              },
          .events =
              {
                  {.key = "onChanged",
                   .method = "OnChanged",
                   .signature = "void(bool)",
                   .description = "Proposes the next selected value; the group stays application-owned."},
              },
      },
      ComponentDef{
          .type = "Switch",
          .category = "Input",
          .glyph = "⇋",
          .description = "Controlled on/off switch, optionally with a label.",
          .children = CP::None,
          .props =
              {
                  {.key = "label", .kind = PK::Text, .default_value = ""},
                  {.key = "checked", .kind = PK::Bool, .default_value = false},
              },
          .events =
              {
                  {.key = "onChanged",
                   .method = "OnChanged",
                   .signature = "void(bool)",
                   .description = "Proposes the next checked value; the application owns the state."},
              },
      },
      ComponentDef{
          .type = "Chip",
          .category = "Input",
          .glyph = "◍",
          .description = "Compact action or selectable value chip.",
          .children = CP::None,
          .props =
              {
                  {.key = "label", .kind = PK::Text, .default_value = "Chip"},
                  {.key = "selected", .kind = PK::Bool, .default_value = false},
              },
          .events =
              {
                  {.key = "onChanged",
                   .method = "OnChanged",
                   .signature = "void(bool)",
                   .description = "Proposes the next selected value for a toggle chip."},
              },
      },
      ComponentDef{
          .type = "Slider",
          .category = "Input",
          .glyph = "⎯",
          .description = "Controlled continuous value with range and optional step.",
          .children = CP::None,
          .props =
              {
                  {.key = "value", .kind = PK::Number, .default_value = 0.5},
                  {.key = "min", .kind = PK::Number, .default_value = 0.0},
                  {.key = "max", .kind = PK::Number, .default_value = 1.0},
                  {.key = "step", .kind = PK::Number, .default_value = json::Value(nullptr)},
              },
          .events =
              {
                  {.key = "onChanged",
                   .method = "OnChanged",
                   .signature = "void(float)",
                   .description = "Proposes a new constrained value while the handle moves."},
                  {.key = "onCommitted",
                   .method = "OnCommitted",
                   .signature = "void(float)",
                   .description = "Fires once when an adjustment finishes with its final value."},
              },
      },
      ComponentDef{
          .type = "ProgressBar",
          .category = "Input",
          .glyph = "▬",
          .description = "Linear progress; a set value is determinate, absent is indeterminate.",
          .children = CP::None,
          .props =
              {
                  {.key = "value", .kind = PK::Number, .default_value = json::Value(nullptr)},
              },
      },
      ComponentDef{
          .type = "ProgressCircle",
          .category = "Input",
          .glyph = "◌",
          .description = "Circular progress; a set value is determinate, absent is indeterminate.",
          .children = CP::None,
          .props =
              {
                  {.key = "value", .kind = PK::Number, .default_value = json::Value(nullptr)},
              },
      },
      ComponentDef{
          .type = "Select",
          .category = "Input",
          .glyph = "▾",
          .description = "Controlled finite-choice field with an anchored popup.",
          .children = CP::None,
          .props =
              {
                  {.key = "options",
                   .kind = PK::StrList,
                   .default_value = std::vector<json::Value>{"Option A", "Option B"}},
                  {.key = "selected", .kind = PK::Number, .default_value = 0.0},
              },
          .events =
              {
                  {.key = "onChanged",
                   .method = "OnChanged",
                   .signature = "void(std::size_t)",
                   .description = "Proposes the selected option index; the application owns it."},
              },
      },
      ComponentDef{
          .type = "TextField",
          .category = "Input",
          .glyph = "✎",
          .description = "Controlled single-line text editor with label and placeholder.",
          .children = CP::None,
          .props =
              {
                  {.key = "label", .kind = PK::Text, .default_value = ""},
                  {.key = "placeholder", .kind = PK::Text, .default_value = ""},
                  {.key = "value", .kind = PK::Text, .default_value = ""},
                  {.key = "variant",
                   .kind = PK::Enum,
                   .enum_values = {"Filled", "Outlined", "Standard"},
                   .default_value = "Filled"},
              },
          .events =
              {
                  {.key = "onChanged",
                   .method = "OnChanged",
                   .signature = "void(const TextEditingValue&)",
                   .description = "Proposes the complete edited value; the application owns it."},
                  {.key = "onSubmitted",
                   .method = "OnSubmitted",
                   .signature = "void()",
                   .description = "Reports that the configured submit action was performed."},
              },
      },
      ComponentDef{
          .type = "ScrollView",
          .category = "Container",
          .glyph = "⛶",
          .description = "Makes one content subtree scrollable along the configured axis.",
          .children = CP::Single,
          .props =
              {
                  {.key = "axis",
                   .kind = PK::Enum,
                   .enum_values = {"Vertical", "Horizontal"},
                   .default_value = "Vertical"},
              },
      },
      ComponentDef{
          .type = "MaterialTheme",
          .category = "Theme",
          .glyph = "◆",
          .description = "Applies the Material light theme to one content subtree.",
          .children = CP::Single,
          .props = {},
      },
      ComponentDef{
          .type = "MaterialDarkTheme",
          .category = "Theme",
          .glyph = "◈",
          .description = "Applies the Material dark theme to one content subtree.",
          .children = CP::Single,
          .props = {},
      },
      ComponentDef{
          .type = "FlatTheme",
          .category = "Theme",
          .glyph = "◇",
          .description = "Applies the flat light theme to one content subtree.",
          .children = CP::Single,
          .props = {},
      },
      ComponentDef{
          .type = "FlatDarkTheme",
          .category = "Theme",
          .glyph = "◐",
          .description = "Applies the flat dark theme to one content subtree.",
          .children = CP::Single,
          .props = {},
      },
  };
  return components;
}

[[nodiscard]] inline const ComponentDef* Find(std::string_view type) {
  for (const auto& component : All()) {
    if (component.type == type) return &component;
  }
  return nullptr;
}

[[nodiscard]] inline const PropDef* FindProp(const ComponentDef& component, std::string_view key) {
  for (const auto& prop : component.props) {
    if (prop.key == key) return &prop;
  }
  return nullptr;
}

// ---------------------------------------------------------------- events ----

/// Events every node accepts, because HuxerUI's View base carries them.
[[nodiscard]] inline const std::vector<EventDef>& UniversalEvents() {
  static const std::vector<EventDef> events{
      {.key = "onClick",
       .method = "OnClick",
       .signature = "void()",
       .description = "Semantic activation: tap, keyboard action, or accessibility invoke."},
  };
  return events;
}

/// The events one component accepts: the universal list first, then its own.
[[nodiscard]] inline std::vector<const EventDef*> EventsOf(const ComponentDef& component) {
  std::vector<const EventDef*> events;
  for (const auto& event : UniversalEvents()) events.push_back(&event);
  for (const auto& event : component.events) events.push_back(&event);
  return events;
}

[[nodiscard]] inline const EventDef* FindEvent(const ComponentDef& component, std::string_view key) {
  for (const auto* event : EventsOf(component)) {
    if (event->key == key) return event;
  }
  return nullptr;
}

/// Modifier keys every node accepts, in emission order. These map onto View
/// modifiers applied through .With(...); absent means "not set".
[[nodiscard]] inline const std::vector<PropDef>& Modifiers() {
  using PK = PropKind;
  static const std::vector<PropDef> modifiers{
      {.key = "padding", .kind = PK::Number, .description = "Uniform Padding inset in logical pixels."},
      {.key = "grow", .kind = PK::Number, .description = "Share of remaining main-axis space (Grow)."},
      {.key = "width", .kind = PK::Number, .description = "Preferred outer width (Frame)."},
      {.key = "height", .kind = PK::Number, .description = "Preferred outer height (Frame)."},
      {.key = "cornerRadius", .kind = PK::Number, .description = "Corner radius in logical pixels."},
      {.key = "fontSize", .kind = PK::Number, .description = "Inherited text font size."},
      {.key = "background", .kind = PK::Color, .description = "Solid Background color."},
      {.key = "foreground", .kind = PK::Color, .description = "Text Foreground color."},
      {.key = "borderColor", .kind = PK::Color, .description = "Border color (with borderWidth)."},
      {.key = "borderWidth", .kind = PK::Number, .description = "Border width in logical pixels."},
      {.key = "enabled", .kind = PK::Bool, .description = "Input eligibility (Enabled); false disables."},
  };
  return modifiers;
}

[[nodiscard]] inline const PropDef* FindModifier(std::string_view key) {
  for (const auto& modifier : Modifiers()) {
    if (modifier.key == key) return &modifier;
  }
  return nullptr;
}

[[nodiscard]] inline std::string_view ChildrenPolicyName(ChildrenPolicy policy) {
  switch (policy) {
    case ChildrenPolicy::None: return "none";
    case ChildrenPolicy::Any: return "any";
    case ChildrenPolicy::Single: return "single";
  }
  return "any";
}

[[nodiscard]] inline json::Value PropDefJson(const PropDef& prop) {
  json::Value out = json::Value::ObjectWith({
      {"key", json::Value(prop.key)},
      {"kind",
       json::Value(std::string([](PropKind kind) {
         switch (kind) {
           case PropKind::Text: return "text";
           case PropKind::Number: return "number";
           case PropKind::Bool: return "bool";
           case PropKind::Enum: return "enum";
           case PropKind::StrList: return "strlist";
           case PropKind::Color: return "color";
         }
         return "text";
       }(prop.kind)))},
      {"description", json::Value(prop.description)},
      {"default", prop.default_value},
  });
  if (prop.kind == PropKind::Enum) {
    std::vector<json::Value> values;
    for (const auto& value : prop.enum_values) values.emplace_back(value);
    out.Set("values", json::Value(std::move(values)));
  }
  return out;
}

[[nodiscard]] inline json::Value EventDefJson(const EventDef& event) {
  return json::Value::ObjectWith({
      {"key", json::Value(event.key)},
      {"method", json::Value(event.method)},
      {"signature", json::Value(event.signature)},
      {"description", json::Value(event.description)},
  });
}

[[nodiscard]] inline json::Value ComponentJson(const ComponentDef& component) {
  std::vector<json::Value> props;
  for (const auto& prop : component.props) props.push_back(PropDefJson(prop));
  std::vector<json::Value> events;
  for (const auto* event : EventsOf(component)) events.push_back(EventDefJson(*event));
  return json::Value::ObjectWith({
      {"type", json::Value(component.type)},
      {"category", json::Value(component.category)},
      {"glyph", json::Value(component.glyph)},
      {"description", json::Value(component.description)},
      {"children", json::Value(std::string(ChildrenPolicyName(component.children)))},
      {"props", json::Value(std::move(props))},
      {"events", json::Value(std::move(events))},
  });
}

/// The whole catalog as JSON — what `hui catalog --json` prints and the MCP
/// hui_catalog tool returns.
[[nodiscard]] inline json::Value CatalogJson() {
  std::vector<json::Value> components;
  std::vector<std::string> categories;
  for (const auto& component : All()) {
    components.push_back(ComponentJson(component));
    if (std::ranges::find(categories, component.category) == categories.end()) {
      categories.push_back(component.category);
    }
  }
  std::vector<json::Value> category_values;
  for (const auto& category : categories) category_values.emplace_back(category);

  std::vector<json::Value> modifiers;
  for (const auto& modifier : Modifiers()) modifiers.push_back(PropDefJson(modifier));

  std::vector<json::Value> universal_events;
  for (const auto& event : UniversalEvents()) universal_events.push_back(EventDefJson(event));

  return json::Value::ObjectWith({
      {"format", json::Value("hui/1")},
      {"components", json::Value(std::move(components))},
      {"categories", json::Value(std::move(category_values))},
      {"modifiers", json::Value(std::move(modifiers))},
      {"universalEvents", json::Value(std::move(universal_events))},
  });
}

}  // namespace hui::catalog
