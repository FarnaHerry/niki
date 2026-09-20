// Document validation: one pass that both humans (studio status line, CLI) and
// agents (MCP hui_validate / hui_apply responses) read.
//
// Errors mean the document cannot faithfully become HuxerUI C++; warnings mean
// it can, but the result will be surprising or degraded.

#pragma once

#include "core/std.h"
#include "core/catalog.h"
#include "core/doc.h"
#include "core/json.h"

namespace hui::validate {

struct Issue final {
  std::string severity;  // "error" | "warning"
  std::string node_id;   // "" when the issue is document-level
  std::string message;

  [[nodiscard]] bool IsError() const { return severity == "error"; }
};

namespace detail {

[[nodiscard]] inline std::string PropKindName(catalog::PropKind kind) {
  switch (kind) {
    case catalog::PropKind::Text: return "text";
    case catalog::PropKind::Number: return "number";
    case catalog::PropKind::Bool: return "bool";
    case catalog::PropKind::Enum: return "enum";
    case catalog::PropKind::StrList: return "string list";
    case catalog::PropKind::Color: return "color";
  }
  return "text";
}

[[nodiscard]] inline std::string ValueKindName(const doc::PropValue& value) {
  if (std::holds_alternative<bool>(value)) return "bool";
  if (std::holds_alternative<double>(value)) return "number";
  if (std::holds_alternative<std::string>(value)) return "text";
  if (std::holds_alternative<std::vector<std::string>>(value)) return "string list";
  return "unset";
}

[[nodiscard]] inline bool IsValidColor(std::string_view text) {
  if (text.size() != 7 && text.size() != 9) return false;
  if (text[0] != '#') return false;
  for (std::size_t i = 1; i < text.size(); ++i) {
    const char ch = text[i];
    const bool hex = (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
    if (!hex) return false;
  }
  return true;
}

[[nodiscard]] inline bool IsIdentifier(std::string_view text) {
  if (text.empty()) return false;
  const char first = text[0];
  if (!(std::isalpha(static_cast<unsigned char>(first)) != 0 || first == '_')) return false;
  for (const char ch : text) {
    if (!(std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_')) return false;
  }
  return true;
}

inline void ValidateValue(std::vector<Issue>& issues, const std::string& node_id, const std::string& key,
                          const catalog::PropDef& def, const doc::PropValue& value) {
  const auto mismatch = [&](const std::string& found) {
    issues.push_back({.severity = "error",
                      .node_id = node_id,
                      .message = std::format("property '{}' expects {}, found {}", key, PropKindName(def.kind),
                                             found)});
  };
  switch (def.kind) {
    case catalog::PropKind::Text:
      if (!std::holds_alternative<std::string>(value)) mismatch(ValueKindName(value));
      break;
    case catalog::PropKind::Number:
      if (const auto* number = std::get_if<double>(&value)) {
        if (!std::isfinite(*number)) {
          issues.push_back(
              {.severity = "error", .node_id = node_id, .message = std::format("property '{}' must be finite", key)});
        }
      } else {
        mismatch(ValueKindName(value));
      }
      break;
    case catalog::PropKind::Bool:
      if (!std::holds_alternative<bool>(value)) mismatch(ValueKindName(value));
      break;
    case catalog::PropKind::Enum:
      if (const auto* text = std::get_if<std::string>(&value)) {
        if (std::ranges::find(def.enum_values, *text) == def.enum_values.end()) {
          const std::string joined = std::ranges::fold_left(
              def.enum_values, std::string(), [](const std::string& acc, const std::string& item) {
                return acc.empty() ? item : acc + ", " + item;
              });
          issues.push_back({.severity = "error",
                            .node_id = node_id,
                            .message =
                                std::format("property '{}' has unknown value '{}'; expected one of {}", key, *text, joined)});
        }
      } else {
        mismatch(ValueKindName(value));
      }
      break;
    case catalog::PropKind::StrList:
      if (!std::holds_alternative<std::vector<std::string>>(value)) mismatch(ValueKindName(value));
      break;
    case catalog::PropKind::Color:
      if (const auto* text = std::get_if<std::string>(&value)) {
        if (!IsValidColor(*text)) {
          issues.push_back({.severity = "error",
                            .node_id = node_id,
                            .message = std::format("property '{}' must be #RRGGBB or #RRGGBBAA, found '{}'", key,
                                                   *text)});
        }
      } else {
        mismatch(ValueKindName(value));
      }
      break;
  }
}

inline void ValidateEntries(std::vector<Issue>& issues, const std::string& node_id,
                            const std::vector<std::pair<std::string, doc::PropValue>>& modifiers) {
  for (const auto& [key, value] : modifiers) {
    if (std::holds_alternative<std::monostate>(value)) continue;
    const catalog::PropDef* def = catalog::FindModifier(key);
    if (def == nullptr) {
      issues.push_back({.severity = "error", .node_id = node_id, .message = std::format("unknown modifier '{}'", key)});
      continue;
    }
    ValidateValue(issues, node_id, key, *def, value);
  }
}

inline void ValidateNode(std::vector<Issue>& issues, const doc::Node& node, std::set<std::string>& seen_ids) {
  if (!seen_ids.insert(node.id).second) {
    issues.push_back({.severity = "error", .node_id = node.id, .message = "duplicate node id"});
  }
  const catalog::ComponentDef* def = catalog::Find(node.type);
  if (def == nullptr) {
    issues.push_back({.severity = "error", .node_id = node.id, .message = "unknown component type: " + node.type});
    return;
  }

  for (const auto& [key, value] : node.props) {
    if (std::holds_alternative<std::monostate>(value)) continue;
    const catalog::PropDef* prop = catalog::FindProp(*def, key);
    if (prop == nullptr) {
      issues.push_back(
          {.severity = "error", .node_id = node.id, .message = std::format("{} has no property '{}'", node.type, key)});
      continue;
    }
    ValidateValue(issues, node.id, key, *prop, value);
  }

  ValidateEntries(issues, node.id, node.modifiers);

  for (const auto& [key, handler] : node.events) {
    if (catalog::FindEvent(*def, key) == nullptr) {
      issues.push_back(
          {.severity = "error", .node_id = node.id, .message = std::format("{} has no event '{}'", node.type, key)});
      continue;
    }
    if (!handler.empty() && !IsIdentifier(handler)) {
      issues.push_back({.severity = "error",
                        .node_id = node.id,
                        .message = std::format("event '{}' handler '{}' is not a C++ identifier (leave it empty for a "
                                               "placeholder)",
                                               key, handler)});
    }
  }

  if (def->children == catalog::ChildrenPolicy::None && !node.children.empty()) {
    issues.push_back(
        {.severity = "error", .node_id = node.id, .message = std::format("{} takes no children", node.type)});
  }
  if (def->children == catalog::ChildrenPolicy::Single && node.children.size() > 1) {
    issues.push_back({.severity = "error",
                      .node_id = node.id,
                      .message = std::format("{} takes a single child, found {}", node.type, node.children.size())});
  }

  // Component-specific contracts.
  if (node.type == "Select") {
    const std::vector<std::string> options = doc::ListOf(node, "options");
    if (options.empty()) {
      issues.push_back({.severity = "error", .node_id = node.id, .message = "Select requires at least one option"});
    } else {
      const double selected = doc::NumberOf(node, "selected", 0.0);
      if (selected < 0 || selected >= static_cast<double>(options.size())) {
        issues.push_back({.severity = "warning",
                          .node_id = node.id,
                          .message = std::format("Select selected index {} is outside the option range and is clamped "
                                                 "when rendered",
                                                 selected)});
      }
    }
  }
  if (node.type == "Slider") {
    const double min = doc::NumberOf(node, "min", 0.0);
    const double max = doc::NumberOf(node, "max", 1.0);
    if (min >= max) {
      issues.push_back({.severity = "error", .node_id = node.id, .message = "Slider min must be below max"});
    } else {
      const double value = doc::NumberOf(node, "value", 0.5);
      if (value < min || value > max) {
        issues.push_back({.severity = "warning",
                          .node_id = node.id,
                          .message = "Slider value is outside its range and is clamped when rendered"});
      }
    }
    if (doc::NumberSet(node, "step")) {
      const double step = doc::NumberOf(node, "step", 0.0);
      if (step <= 0.0) {
        issues.push_back({.severity = "error", .node_id = node.id, .message = "Slider step must be positive"});
      }
    }
  }
  if (node.type == "TextField") {
    if (doc::TextOf(node, "label").empty()) {
      issues.push_back(
          {.severity = "warning", .node_id = node.id, .message = "TextField without a label is hard to discover"});
    }
  }

  // Modifiers with value constraints.
  if (doc::HasModifier(node, "padding") && doc::ModifierNumber(node, "padding", 0.0) < 0.0) {
    issues.push_back({.severity = "error", .node_id = node.id, .message = "padding cannot be negative"});
  }
  if (doc::HasModifier(node, "grow") && doc::ModifierNumber(node, "grow", 0.0) < 0.0) {
    issues.push_back({.severity = "error", .node_id = node.id, .message = "grow cannot be negative"});
  }
  if (doc::HasModifier(node, "fontSize") && doc::ModifierNumber(node, "fontSize", 0.0) <= 0.0) {
    issues.push_back({.severity = "error", .node_id = node.id, .message = "fontSize must be positive"});
  }
  if (doc::HasModifier(node, "cornerRadius") && doc::ModifierNumber(node, "cornerRadius", 0.0) < 0.0) {
    issues.push_back({.severity = "error", .node_id = node.id, .message = "cornerRadius cannot be negative"});
  }
  if (doc::HasModifier(node, "width") && doc::ModifierNumber(node, "width", 0.0) <= 0.0) {
    issues.push_back({.severity = "error", .node_id = node.id, .message = "width must be positive"});
  }
  if (doc::HasModifier(node, "height") && doc::ModifierNumber(node, "height", 0.0) <= 0.0) {
    issues.push_back({.severity = "error", .node_id = node.id, .message = "height must be positive"});
  }
  if (doc::HasModifier(node, "borderWidth") && doc::ModifierNumber(node, "borderWidth", 0.0) < 0.0) {
    issues.push_back({.severity = "error", .node_id = node.id, .message = "borderWidth cannot be negative"});
  }

  for (const auto& child : node.children) {
    ValidateNode(issues, child, seen_ids);
  }
}

}  // namespace detail

[[nodiscard]] inline std::vector<Issue> Check(const doc::Document& document) {
  std::vector<Issue> issues;
  if (document.format != "hui/1") {
    issues.push_back({.severity = "error", .node_id = "", .message = "format must be \"hui/1\""});
  }
  if (!detail::IsIdentifier(document.name)) {
    issues.push_back({.severity = "error",
                      .node_id = "",
                      .message = std::format("name '{}' is not a valid C++ identifier (letters, digits, _)", document.name)});
  }
  std::set<std::string> seen_ids;
  detail::ValidateNode(issues, document.root, seen_ids);

  const catalog::ComponentDef* root_def = catalog::Find(document.root.type);
  if (root_def != nullptr && root_def->children == catalog::ChildrenPolicy::None) {
    issues.push_back({.severity = "warning",
                      .node_id = document.root.id,
                      .message = "the root is a leaf component; there is nowhere to drop new children"});
  }
  return issues;
}

[[nodiscard]] inline bool HasErrors(const std::vector<Issue>& issues) {
  return std::ranges::any_of(issues, [](const Issue& issue) { return issue.IsError(); });
}

[[nodiscard]] inline json::Value IssuesJson(const std::vector<Issue>& issues) {
  std::vector<json::Value> items;
  for (const auto& issue : issues) {
    items.push_back(json::Value::ObjectWith({
        {"severity", json::Value(issue.severity)},
        {"nodeId", json::Value(issue.node_id)},
        {"message", json::Value(issue.message)},
    }));
  }
  json::Value out = json::Value::ObjectWith({{"valid", json::Value(!HasErrors(issues))},
                                             {"errorCount",
                                              json::Value(static_cast<double>(
                                                  std::ranges::count_if(issues, [](const Issue& i) { return i.IsError(); })))},
                                             {"warningCount",
                                              json::Value(static_cast<double>(
                                                  std::ranges::count_if(issues, [](const Issue& i) { return !i.IsError(); })))},
                                             {"issues", json::Value(std::move(items))}});
  return out;
}

}  // namespace hui::validate
