// Document <-> JSON conversion and .hui.json file IO.
//
// The file is the contract between the designer, the CLI, the MCP server, and
// AI agents editing it by hand: insertion-ordered, minimal (absent props mean
// catalog defaults), and pretty-printed so diffs stay readable.

#pragma once

#include "core/std.h"
#include "core/catalog.h"
#include "core/doc.h"
#include "core/json.h"

namespace hui::io {

[[nodiscard]] inline json::Value PropValueToJson(const doc::PropValue& value) {
  if (const auto* flag = std::get_if<bool>(&value)) return json::Value(*flag);
  if (const auto* number = std::get_if<double>(&value)) return json::Value(*number);
  if (const auto* text = std::get_if<std::string>(&value)) return json::Value(*text);
  if (const auto* list = std::get_if<std::vector<std::string>>(&value)) {
    std::vector<json::Value> items;
    for (const auto& item : *list) items.emplace_back(item);
    return json::Value(std::move(items));
  }
  return json::Value(nullptr);
}

[[nodiscard]] inline doc::PropValue JsonToPropValue(const json::Value& value) {
  switch (value.type()) {
    case json::Type::Bool: return doc::PropValue(value.AsBool());
    case json::Type::Number: return doc::PropValue(value.AsNumber());
    case json::Type::String: return doc::PropValue(value.AsString());
    case json::Type::Array: {
      std::vector<std::string> items;
      for (const auto& item : value.AsArray()) items.push_back(item.AsString());
      return doc::PropValue(std::move(items));
    }
    default: return doc::PropValue(std::monostate{});
  }
}

[[nodiscard]] inline json::Value NodeToJson(const doc::Node& node) {
  json::Value out = json::Value::ObjectWith({{"id", json::Value(node.id)}, {"type", json::Value(node.type)}});

  if (!node.props.empty()) {
    std::vector<json::Member> props;
    for (const auto& [key, value] : node.props) {
      if (std::holds_alternative<std::monostate>(value)) continue;
      props.emplace_back(key, PropValueToJson(value));
    }
    if (!props.empty()) out.Set("props", json::Value(std::move(props)));
  }

  if (!node.modifiers.empty()) {
    std::vector<json::Member> modifiers;
    for (const auto& [key, value] : node.modifiers) {
      if (std::holds_alternative<std::monostate>(value)) continue;
      modifiers.emplace_back(key, PropValueToJson(value));
    }
    if (!modifiers.empty()) out.Set("modifiers", json::Value(std::move(modifiers)));
  }

  if (!node.events.empty()) {
    std::vector<json::Member> events;
    for (const auto& [key, handler] : node.events) {
      events.emplace_back(key, json::Value(handler));
    }
    out.Set("events", json::Value(std::move(events)));
  }

  if (!node.children.empty()) {
    std::vector<json::Value> children;
    for (const auto& child : node.children) children.push_back(NodeToJson(child));
    out.Set("children", json::Value(std::move(children)));
  }
  return out;
}

[[nodiscard]] inline json::Value DocumentToJson(const doc::Document& document) {
  return json::Value::ObjectWith({
      {"format", json::Value(document.format)},
      {"name", json::Value(document.name)},
      {"root", NodeToJson(document.root)},
  });
}

[[nodiscard]] inline std::string Serialize(const doc::Document& document) {
  return DocumentToJson(document).DumpPretty() + "\n";
}

[[nodiscard]] inline std::expected<doc::Node, std::string> ParseNode(const json::Value& value) {
  if (!value.IsObject()) return std::unexpected("hui: node must be a JSON object");
  doc::Node node;
  node.id = value.GetString("id");
  node.type = value.GetString("type");
  if (node.id.empty()) return std::unexpected("hui: node is missing its id");
  if (node.type.empty()) return std::unexpected("hui: node " + node.id + " is missing its type");
  if (const json::Value* props = value.Find("props"); props != nullptr && props->IsObject()) {
    for (const auto& [key, prop] : props->AsObject()) {
      node.SetProp(key, JsonToPropValue(prop));
    }
  }
  if (const json::Value* modifiers = value.Find("modifiers"); modifiers != nullptr && modifiers->IsObject()) {
    for (const auto& [key, modifier] : modifiers->AsObject()) {
      node.SetModifier(key, JsonToPropValue(modifier));
    }
  }
  if (const json::Value* events = value.Find("events"); events != nullptr && events->IsObject()) {
    for (const auto& [key, handler] : events->AsObject()) {
      node.SetEvent(key, handler.IsString() ? handler.AsString() : std::string());
    }
  }
  if (const json::Value* children = value.Find("children"); children != nullptr && children->IsArray()) {
    for (const auto& child : children->AsArray()) {
      auto parsed = ParseNode(child);
      if (!parsed) return std::unexpected(parsed.error());
      node.children.push_back(std::move(*parsed));
    }
  }
  return node;
}

[[nodiscard]] inline std::expected<doc::Document, std::string> ParseDocument(const json::Value& value) {
  if (!value.IsObject()) return std::unexpected("hui: document must be a JSON object");
  doc::Document document;
  document.format = value.GetString("format", "hui/1");
  document.name = value.GetString("name", "NewPage");
  const json::Value* root = value.Find("root");
  if (root == nullptr) return std::unexpected("hui: document is missing its root node");
  auto parsed_root = ParseNode(*root);
  if (!parsed_root) return std::unexpected(parsed_root.error());
  document.root = std::move(*parsed_root);
  return document;
}

[[nodiscard]] inline std::expected<doc::Document, std::string> ParseDocumentText(std::string_view text) {
  auto parsed = json::Value::Parse(text);
  if (!parsed) return std::unexpected(parsed.error());
  return ParseDocument(*parsed);
}

// ------------------------------------------------------------------- files --

[[nodiscard]] inline std::expected<doc::Document, std::string> LoadDocument(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) return std::unexpected("hui: cannot open " + path.string());
  std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  auto document = ParseDocumentText(text);
  if (!document) return std::unexpected(path.string() + ": " + document.error());
  return document;
}

[[nodiscard]] inline std::expected<void, std::string>
SaveDocument(const doc::Document& document, const std::filesystem::path& path) {
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  if (!file) return std::unexpected("hui: cannot write " + path.string());
  file << Serialize(document);
  file.close();
  if (!file) return std::unexpected("hui: failed while writing " + path.string());
  return {};
}

[[nodiscard]] inline std::expected<std::string, std::string> ReadTextFile(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) return std::unexpected("hui: cannot open " + path.string());
  std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  return text;
}

[[nodiscard]] inline std::expected<void, std::string> WriteTextFile(const std::string& text,
                                                                    const std::filesystem::path& path) {
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  if (!file) return std::unexpected("hui: cannot write " + path.string());
  file << text;
  file.close();
  if (!file) return std::unexpected("hui: failed while writing " + path.string());
  return {};
}

}  // namespace hui::io
