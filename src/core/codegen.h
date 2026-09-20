// The code generator: a hui document becomes HuxerUI C++ in the framework's
// own DSL style (see HuxerUI AGENTS.md "UI DSL and examples").
//
//   return Column {
//     Text("Count: 0", TextRole::Title),
//     Row {
//       Button("Increment"),
//     }.With(Spacing(8.0F)),
//   }.With(Padding(24.0F));
//
// Two output styles: a complete module unit, or a bare return snippet to paste
// into an existing composable.

#pragma once

#include "core/std.h"
#include "core/catalog.h"
#include "core/doc.h"
#include "core/json.h"

namespace hui::codegen {

enum class Style {
  Module,
  Snippet,
};

struct Options final {
  Style style = Style::Module;
  std::string source_name;  // file the document came from, for the header comment
};

namespace detail {

[[nodiscard]] inline std::string FormatFloat(double value) {
  if (!std::isfinite(value)) value = 0.0;
  if (value == std::floor(value) && std::abs(value) < 1e15) {
    return std::format("{:.1f}F", value);
  }
  return std::format("{}F", value);
}

[[nodiscard]] inline std::string Escape(std::string_view text) {
  std::string out;
  out.push_back('"');
  for (const char raw : text) {
    const auto ch = static_cast<unsigned char>(raw);
    switch (ch) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (ch < 0x20) {
          static constexpr char digits[] = "0123456789abcdef";
          out += "\\x";
          out.push_back(digits[(ch >> 4) & 0xF]);
          out.push_back(digits[ch & 0xF]);
        } else {
          out.push_back(raw);
        }
    }
  }
  out.push_back('"');
  return out;
}

[[nodiscard]] inline std::string ColorLiteral(std::string_view hex) {
  std::string digits(hex.substr(1));
  if (digits.size() == 6) digits += "FF";
  std::uint32_t value = 0;
  std::from_chars(digits.data(), digits.data() + digits.size(), value, 16);
  return std::format("Color::Rgba32(0x{:08X}u)", value);
}

[[nodiscard]] inline std::string SnakeCase(std::string_view text) {
  std::string out;
  for (std::size_t i = 0; i < text.size(); ++i) {
    const char ch = text[i];
    if (ch >= 'A' && ch <= 'Z') {
      if (i > 0) out.push_back('_');
      out.push_back(static_cast<char>(ch - 'A' + 'a'));
    } else {
      out.push_back(ch);
    }
  }
  return out;
}

/// Whether the prop currently differs from its catalog default. An absent
/// prop never differs: absent means "use the default".
[[nodiscard]] inline bool PropDiffers(const doc::Node& node, std::string_view key) {
  const auto* value = node.GetProp(key);
  if (value == nullptr) return false;
  const auto* component = catalog::Find(node.type);
  if (component == nullptr) return true;
  const auto* prop = catalog::FindProp(*component, key);
  if (prop == nullptr) return false;
  const json::Value current = [value]() {
    switch (value->index()) {
      case 1: return json::Value(std::get<bool>(*value));
      case 2: return json::Value(std::get<double>(*value));
      case 3: return json::Value(std::get<std::string>(*value));
      case 4: {
        std::vector<json::Value> items;
        for (const auto& item : std::get<std::vector<std::string>>(*value)) items.emplace_back(item);
        return json::Value(std::move(items));
      }
      default: return json::Value(nullptr);
    }
  }();
  return current.Dump() != prop->default_value.Dump();
}

/// Everything this node emits into .With(...), in one fixed order: space
/// first (Padding, Frame), then container policy, then paint and text.
[[nodiscard]] inline std::vector<std::string> WithList(const doc::Node& node) {
  std::vector<std::string> parts;
  const auto has = [&](std::string_view key) { return doc::HasModifier(node, key); };
  const auto number = [&](std::string_view key) { return doc::ModifierNumber(node, key, 0.0); };
  const bool linear = node.type == "Column" || node.type == "Row" || node.type == "Flow";
  const bool stack = node.type == "Stack";

  if (has("padding")) parts.push_back("Padding(" + FormatFloat(number("padding")) + ")");
  if (has("width") || has("height")) {
    std::string frame = "Frame{";
    if (has("width") && has("height")) {
      frame += ".width = " + FormatFloat(number("width")) + ", .height = " + FormatFloat(number("height"));
    } else if (has("width")) {
      frame += ".width = " + FormatFloat(number("width"));
    } else {
      frame += ".height = " + FormatFloat(number("height"));
    }
    frame += "}";
    parts.push_back(std::move(frame));
  }
  if (linear && PropDiffers(node, "spacing")) {
    parts.push_back("Spacing(" + FormatFloat(doc::NumberOf(node, "spacing", 0.0)) + ")");
  }
  if (linear && PropDiffers(node, "mainAlign")) {
    parts.push_back("MainAlign(MainAxisAlignment::" + doc::EnumOf(node, "mainAlign") + ")");
  }
  if (linear && PropDiffers(node, "crossAlign")) {
    parts.push_back("CrossAlign(CrossAxisAlignment::" + doc::EnumOf(node, "crossAlign") + ")");
  }
  if (stack && (PropDiffers(node, "alignH") || PropDiffers(node, "alignV"))) {
    parts.push_back("Align(HorizontalAlignment::" + doc::EnumOf(node, "alignH") + ", VerticalAlignment::" +
                    doc::EnumOf(node, "alignV") + ")");
  }
  if (has("cornerRadius")) parts.push_back("CornerRadius(" + FormatFloat(number("cornerRadius")) + ")");
  if (has("background")) parts.push_back("Background(" + ColorLiteral(doc::ModifierText(node, "background")) + ")");
  if (has("foreground")) parts.push_back("Foreground(" + ColorLiteral(doc::ModifierText(node, "foreground")) + ")");
  if (has("borderColor")) {
    const double width = has("borderWidth") ? number("borderWidth") : 1.0;
    parts.push_back("Border{.color = " + ColorLiteral(doc::ModifierText(node, "borderColor")) +
                    ", .width = " + FormatFloat(width) + "}");
  }
  if (has("fontSize")) parts.push_back("FontSize(" + FormatFloat(number("fontSize")) + ")");
  if (has("grow")) parts.push_back("Grow(" + FormatFloat(number("grow")) + ")");
  if (has("enabled") && !doc::ModifierBool(node, "enabled", true)) parts.push_back("Enabled{false}");
  return parts;
}

[[nodiscard]] inline std::string JoinWith(const std::vector<std::string>& parts, std::string_view indent) {
  if (parts.size() <= 2) {
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i) {
      if (i > 0) out += ", ";
      out += parts[i];
    }
    return out;
  }
  std::string out = "\n";
  for (std::size_t i = 0; i < parts.size(); ++i) {
    if (i > 0) out += ",\n";
    out += std::string(indent) + "    " + parts[i];
  }
  out += "\n" + std::string(indent);
  return out;
}

/// The parenthesised parameter list of a handler signature: "void(bool)" -> "(bool)".
[[nodiscard]] inline std::string ParamList(std::string_view signature) {
  const std::size_t open = signature.find('(');
  return open == std::string_view::npos ? std::string() : std::string(signature.substr(open));
}

/// The `.OnX(...)` suffixes for one node's bindings, in declaration order. An
/// empty handler name emits a compiling placeholder lambda; unknown keys are
/// skipped because validation already reports them.
[[nodiscard]] inline std::vector<std::string> EventBindings(const doc::Node& node) {
  std::vector<std::string> parts;
  const catalog::ComponentDef* component = catalog::Find(node.type);
  if (component == nullptr) return parts;
  for (const auto& [key, handler] : node.events) {
    const catalog::EventDef* event = catalog::FindEvent(*component, key);
    if (event == nullptr) continue;
    if (handler.empty()) {
      parts.push_back(event->method + "([]" + ParamList(event->signature) + " { /* hui: TODO " + key +
                       " handler for " + node.type + " " + node.id + " */ })");
    } else {
      parts.push_back(event->method + "(" + handler + ")");
    }
  }
  return parts;
}

/// One application-provided handler the generated code expects to exist.
struct HandlerRef final {
  std::string handler;
  std::string signature;
  std::string context;  // e.g. "Button n4 onClick"
};

inline void CollectHandlers(const doc::Node& node, std::vector<HandlerRef>& out) {
  const catalog::ComponentDef* component = catalog::Find(node.type);
  if (component != nullptr) {
    for (const auto& [key, handler] : node.events) {
      if (handler.empty()) continue;
      const catalog::EventDef* event = catalog::FindEvent(*component, key);
      out.push_back({.handler = handler,
                     .signature = event != nullptr ? event->signature : "void()",
                     .context = node.type + " " + node.id + " " + key});
    }
  }
  for (const auto& child : node.children) CollectHandlers(child, out);
}

/// Renders one handler as a copy-pasteable declaration, e.g. "void OnClick();".
[[nodiscard]] inline std::string HandlerDeclaration(const HandlerRef& handler) {
  const std::size_t open = handler.signature.find('(');
  if (open == std::string::npos) return handler.signature + " " + handler.handler + "();";
  return handler.signature.substr(0, open) + " " + handler.handler + handler.signature.substr(open) + ";";
}

/// The trailing comment listing every named handler the document binds, so the
/// generated module says exactly what the application must provide. Empty when
/// the document binds nothing (or only placeholders), keeping simple output clean.
[[nodiscard]] inline std::string HandlerManifest(const doc::Document& document) {
  std::vector<HandlerRef> handlers;
  CollectHandlers(document.root, handlers);
  if (handlers.empty()) return "";

  std::set<std::string> seen;
  std::string out =
      "\n// hui: this document binds handlers the enclosing scope must provide (the generated module\n"
      "// does not define them). Declare them in this module, or import them, then implement them.\n";
  for (const auto& handler : handlers) {
    if (!seen.insert(handler.handler).second) continue;
    out += std::format("//   {:<44} // {}\n", HandlerDeclaration(handler), handler.context);
  }
  return out;
}

[[nodiscard]] std::string GenerateNode(const doc::Node& node, std::string_view indent);

[[nodiscard]] inline std::vector<std::string> GenerateChildren(const std::vector<doc::Node>& children,
                                                               std::string_view indent) {
  std::vector<std::string> out;
  out.reserve(children.size());
  for (const auto& child : children) {
    out.push_back(GenerateNode(child, std::string(indent) + "  "));
  }
  return out;
}

[[nodiscard]] inline std::string GenerateBracedContainer(const doc::Node& node, std::string_view indent,
                                                         const std::vector<std::string>& child_lines) {
  std::string out = node.type + " {";
  for (const auto& line : child_lines) {
    out += "\n" + std::string(indent) + "  " + line + ",";
  }
  if (!child_lines.empty()) out += "\n" + std::string(indent);
  out += "}";
  return out;
}

[[nodiscard]] inline std::string GenerateNode(const doc::Node& node, std::string_view indent) {
  const std::string type = node.type;
  std::string out;

  if (type == "Column" || type == "Row" || type == "Flow" || type == "Stack") {
    out = GenerateBracedContainer(node, indent, GenerateChildren(node.children, indent));
  } else if (type == "Text") {
    out = "Text(" + Escape(doc::TextOf(node, "text"));
    if (doc::EnumOf(node, "role") != "Body") {
      out += ", TextRole::" + doc::EnumOf(node, "role");
    }
    out += ")";
  } else if (type == "Divider") {
    out = doc::EnumOf(node, "axis") == "Vertical" ? "Divider(Axis::Vertical)" : "Divider()";
  } else if (type == "Spacer") {
    out = "Spacer()";
  } else if (type == "Button") {
    out = "Button(" + Escape(doc::TextOf(node, "label")) + ")";
  } else if (type == "Checkbox" || type == "RadioButton" || type == "Switch") {
    const std::string label = doc::TextOf(node, "label");
    const std::string state_name = type == "RadioButton" ? "selected" : "checked";
    const bool state = doc::BoolOf(node, state_name);
    if (label.empty()) {
      out = std::format("{}({})", type, state ? "true" : "false");
    } else {
      out = std::format("{}({}, {})", type, Escape(label), state ? "true" : "false");
    }
  } else if (type == "Chip") {
    const bool selected = doc::BoolOf(node, "selected");
    out = selected ? "Chip(" + Escape(doc::TextOf(node, "label")) + ", true)"
                   : "Chip(" + Escape(doc::TextOf(node, "label")) + ")";
  } else if (type == "Slider") {
    const double min = doc::NumberOf(node, "min", 0.0);
    const double max = doc::NumberOf(node, "max", 1.0);
    out = "Slider(" + FormatFloat(std::clamp(doc::NumberOf(node, "value", 0.5), std::min(min, max), max)) + ")";
    if (min != 0.0 || max != 1.0) {
      out += ".Range(" + FormatFloat(min) + ", " + FormatFloat(max) + ")";
    }
    if (doc::NumberSet(node, "step")) {
      out += ".Step(" + FormatFloat(doc::NumberOf(node, "step", 0.0)) + ")";
    }
  } else if (type == "ProgressBar" || type == "ProgressCircle") {
    if (doc::NumberSet(node, "value")) {
      out = type + "(" + FormatFloat(std::clamp(doc::NumberOf(node, "value", 0.0), 0.0, 1.0)) + ")";
    } else {
      out = type + "()";
    }
  } else if (type == "Select") {
    const std::vector<std::string> options = doc::ListOf(node, "options");
    if (options.empty()) {
      // Validation reports the error; emitted code still compiles.
      out = "Text(\"Select\")  // hui: Select requires at least one option";
    } else {
      std::string items;
      for (std::size_t i = 0; i < options.size(); ++i) {
        if (i > 0) items += ", ";
        items += Escape(options[i]);
      }
      const auto selected = static_cast<std::size_t>(
          std::clamp<double>(doc::NumberOf(node, "selected", 0.0), 0.0, static_cast<double>(options.size() - 1)));
      out = "Select(std::vector<std::string>{" + items + "}, " + std::format("{}", selected) +
            ", [](const std::string& option) { return Text(option); })";
    }
  } else if (type == "TextField") {
    out = "TextField(TextEditingValue::FromText(" + Escape(doc::TextOf(node, "value")) + "))";
    if (!doc::TextOf(node, "label").empty()) {
      out += ".Label(" + Escape(doc::TextOf(node, "label")) + ")";
    }
    if (!doc::TextOf(node, "placeholder").empty()) {
      out += ".Placeholder(" + Escape(doc::TextOf(node, "placeholder")) + ")";
    }
    if (doc::EnumOf(node, "variant") != "Filled") {
      out += ".Variant(TextFieldVariant::" + doc::EnumOf(node, "variant") + ")";
    }
  } else if (type == "ScrollView") {
    std::string child = node.children.empty() ? "Column {}"
                                              : GenerateNode(node.children.front(), std::string(indent) + "  ");
    out = "ScrollView(" + child + ")";
    if (doc::EnumOf(node, "axis") == "Horizontal") {
      out += ".ScrollAxis(Axis::Horizontal)";
    }
  } else if (type == "MaterialTheme" || type == "MaterialDarkTheme" || type == "FlatTheme" || type == "FlatDarkTheme") {
    std::string child = node.children.empty() ? "Column {}"
                                              : GenerateNode(node.children.front(), std::string(indent) + "  ");
    out = type + "(" + child + ")";
  } else {
    out = "Text(" + Escape(type) + ")  // hui: unknown component";
  }

  const std::vector<std::string> with_parts = WithList(node);
  if (!with_parts.empty()) {
    out += ".With(" + JoinWith(with_parts, indent) + ")";
  }
  for (const auto& binding : EventBindings(node)) {
    out += "." + binding;
  }
  return out;
}

}  // namespace detail

[[nodiscard]] inline std::string GenerateCpp(const doc::Document& document, const Options& options = {}) {
  using namespace detail;
  const std::string expression =
      GenerateNode(document.root, options.style == Style::Snippet ? "" : "  ");
  if (options.style == Style::Snippet) {
    return "return " + expression + ";\n";
  }

  std::string header = "// Generated by hui 0.1.0";
  if (!options.source_name.empty()) {
    header += " from " + options.source_name;
  }
  header += ". Edit the document, not this file.";
  std::string code = std::format(
      "{}\nexport module {};\n\nimport std;\nimport huxerui;\n\nusing namespace huxerui;\n\nexport View {}() {{\n  "
      "return {};\n}}\n",
      header, SnakeCase(document.name), document.name, expression);
  code += HandlerManifest(document);
  return code;
}

[[nodiscard]] inline std::string ModuleNameFor(const doc::Document& document) {
  return detail::SnakeCase(document.name);
}

}  // namespace hui::codegen
