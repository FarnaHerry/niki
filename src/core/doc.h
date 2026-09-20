// The hui document model: a tree of typed nodes plus the pure operations the
// designer, the CLI, and the MCP server all mutate documents through.
//
// A Node is a value. Operations return the mutated document by reference so
// callers can keep one Document in a State and assign the result; nothing here
// touches the filesystem or the UI.

#pragma once

#include "core/std.h"
#include "core/catalog.h"
#include "core/json.h"

namespace hui::doc {

using PropValue = std::variant<std::monostate, bool, double, std::string, std::vector<std::string>>;
// monostate means "not set": the catalog default applies for props that have
// one, and "absent" for optional props and modifiers.

struct Node final {
  std::string id;
  std::string type;
  std::vector<std::pair<std::string, PropValue>> props;      // insertion-ordered
  std::vector<std::pair<std::string, PropValue>> modifiers;  // insertion-ordered
  // Event key -> handler name, insertion-ordered. An empty handler means "bind
  // the event but emit a placeholder lambda"; an absent key means "unbound".
  std::vector<std::pair<std::string, std::string>> events;
  std::vector<Node> children;

  [[nodiscard]] const PropValue* GetProp(std::string_view key) const {
    for (const auto& [name, value] : props) {
      if (name == key) return &value;
    }
    return nullptr;
  }
  void SetProp(std::string key, PropValue value) {
    for (auto& [name, existing] : props) {
      if (name == key) {
        existing = std::move(value);
        return;
      }
    }
    props.emplace_back(std::move(key), std::move(value));
  }
  void RemoveProp(std::string_view key) {
    std::erase_if(props, [key](const auto& entry) { return entry.first == key; });
  }

  [[nodiscard]] const PropValue* GetModifier(std::string_view key) const {
    for (const auto& [name, value] : modifiers) {
      if (name == key) return &value;
    }
    return nullptr;
  }
  void SetModifier(std::string key, PropValue value) {
    for (auto& [name, existing] : modifiers) {
      if (name == key) {
        existing = std::move(value);
        return;
      }
    }
    modifiers.emplace_back(std::move(key), std::move(value));
  }
  void RemoveModifier(std::string_view key) {
    std::erase_if(modifiers, [key](const auto& entry) { return entry.first == key; });
  }

  [[nodiscard]] const std::string* GetEvent(std::string_view key) const {
    for (const auto& [name, handler] : events) {
      if (name == key) return &handler;
    }
    return nullptr;
  }
  void SetEvent(std::string key, std::string handler) {
    for (auto& [name, existing] : events) {
      if (name == key) {
        existing = std::move(handler);
        return;
      }
    }
    events.emplace_back(std::move(key), std::move(handler));
  }
  void RemoveEvent(std::string_view key) {
    std::erase_if(events, [key](const auto& entry) { return entry.first == key; });
  }
};

struct Document final {
  std::string format = "hui/1";
  std::string name = "NewPage";
  Node root;
};

// ---------------------------------------------------------------- accessors --

/// Reads one prop as a string through the catalog default; "" when unset.
[[nodiscard]] inline std::string TextOf(const Node& node, std::string_view key) {
  const auto* value = node.GetProp(key);
  if (value == nullptr) {
    const auto* component = catalog::Find(node.type);
    if (component != nullptr) {
      const auto* prop = catalog::FindProp(*component, key);
      if (prop != nullptr && prop->default_value.IsString()) return prop->default_value.AsString();
    }
    return "";
  }
  if (const auto* text = std::get_if<std::string>(value)) return *text;
  return "";
}

/// Reads one prop as a number; fallback when unset or not numeric.
[[nodiscard]] inline double NumberOf(const Node& node, std::string_view key, double fallback) {
  const auto* value = node.GetProp(key);
  if (value == nullptr) {
    const auto* component = catalog::Find(node.type);
    if (component != nullptr) {
      const auto* prop = catalog::FindProp(*component, key);
      if (prop != nullptr && prop->default_value.IsNumber()) return prop->default_value.AsNumber();
    }
    return fallback;
  }
  if (const auto* number = std::get_if<double>(value)) return *number;
  return fallback;
}

[[nodiscard]] inline bool NumberSet(const Node& node, std::string_view key) {
  const auto* value = node.GetProp(key);
  return value != nullptr && std::holds_alternative<double>(*value);
}

[[nodiscard]] inline bool BoolOf(const Node& node, std::string_view key) {
  const auto* value = node.GetProp(key);
  if (value == nullptr) {
    const auto* component = catalog::Find(node.type);
    if (component != nullptr) {
      const auto* prop = catalog::FindProp(*component, key);
      if (prop != nullptr && prop->default_value.IsBool()) return prop->default_value.AsBool();
    }
    return false;
  }
  if (const auto* flag = std::get_if<bool>(value)) return *flag;
  return false;
}

[[nodiscard]] inline std::string EnumOf(const Node& node, std::string_view key) {
  return TextOf(node, key);
}

[[nodiscard]] inline std::vector<std::string> ListOf(const Node& node, std::string_view key) {
  const auto* component = catalog::Find(node.type);
  const auto* value = node.GetProp(key);
  if (value != nullptr) {
    if (const auto* list = std::get_if<std::vector<std::string>>(value)) return *list;
    return {};
  }
  if (component != nullptr) {
    const auto* prop = catalog::FindProp(*component, key);
    if (prop != nullptr && prop->default_value.IsArray()) {
      std::vector<std::string> items;
      for (const auto& item : prop->default_value.AsArray()) {
        items.push_back(item.AsString());
      }
      return items;
    }
  }
  return {};
}

[[nodiscard]] inline double ModifierNumber(const Node& node, std::string_view key, double fallback) {
  const auto* value = node.GetModifier(key);
  if (value == nullptr) return fallback;
  if (const auto* number = std::get_if<double>(value)) return *number;
  return fallback;
}

[[nodiscard]] inline bool HasModifier(const Node& node, std::string_view key) {
  return node.GetModifier(key) != nullptr;
}

[[nodiscard]] inline std::string ModifierText(const Node& node, std::string_view key) {
  const auto* value = node.GetModifier(key);
  if (value == nullptr) return "";
  if (const auto* text = std::get_if<std::string>(value)) return *text;
  return "";
}

[[nodiscard]] inline bool ModifierBool(const Node& node, std::string_view key, bool fallback = false) {
  const auto* value = node.GetModifier(key);
  if (value == nullptr) return fallback;
  if (const auto* flag = std::get_if<bool>(value)) return *flag;
  return fallback;
}

/// True when the node binds this event (even with an empty placeholder handler).
[[nodiscard]] inline bool HasEvent(const Node& node, std::string_view key) {
  return node.GetEvent(key) != nullptr;
}

/// The bound handler name; "" for a placeholder binding or an unbound event.
[[nodiscard]] inline std::string EventText(const Node& node, std::string_view key) {
  const auto* handler = node.GetEvent(key);
  return handler != nullptr ? *handler : "";
}

// ------------------------------------------------------------------- lookup --

[[nodiscard]] inline Node* FindNode(Node& node, std::string_view id) {
  if (node.id == id) return &node;
  for (auto& child : node.children) {
    if (Node* found = FindNode(child, id)) return found;
  }
  return nullptr;
}

[[nodiscard]] inline const Node* FindNode(const Node& node, std::string_view id) {
  if (node.id == id) return &node;
  for (const auto& child : node.children) {
    if (const Node* found = FindNode(child, id)) return found;
  }
  return nullptr;
}

[[nodiscard]] inline Node* FindNode(Document& document, std::string_view id) {
  return FindNode(document.root, id);
}

[[nodiscard]] inline const Node* FindNode(const Document& document, std::string_view id) {
  return FindNode(document.root, id);
}

[[nodiscard]] inline Node* FindParent(Node& node, std::string_view id) {
  for (auto& child : node.children) {
    if (child.id == id) return &node;
    if (Node* found = FindParent(child, id)) return found;
  }
  return nullptr;
}

[[nodiscard]] inline Node* FindParent(Document& document, std::string_view id) {
  if (document.root.id == id) return nullptr;
  return FindParent(document.root, id);
}

/// True when candidate is id itself or one of its descendants.
[[nodiscard]] inline bool Contains(const Node& node, std::string_view id) {
  if (node.id == id) return true;
  for (const auto& child : node.children) {
    if (Contains(child, id)) return true;
  }
  return false;
}

/// Collects every id in declaration order.
[[nodiscard]] inline std::vector<std::string> AllIds(const Node& node) {
  std::vector<std::string> ids{node.id};
  for (const auto& child : node.children) {
    for (auto& id : AllIds(child)) ids.push_back(std::move(id));
  }
  return ids;
}

/// A short human-readable summary for tree rows and outlines.
[[nodiscard]] inline std::string SummaryOf(const Node& node) {
  if (node.type == "Text") return TextOf(node, "text");
  if (node.type == "Button" || node.type == "Chip") return TextOf(node, "label");
  if (node.type == "Checkbox" || node.type == "RadioButton" || node.type == "Switch") {
    const std::string label = TextOf(node, "label");
    return label.empty() ? node.type : label;
  }
  if (node.type == "TextField") return TextOf(node, "label");
  return node.type;
}

// ---------------------------------------------------------------- creation --

[[nodiscard]] inline std::string NextId(const Document& document) {
  long long max = 0;
  for (const auto& id : AllIds(document.root)) {
    if (id.starts_with('n')) {
      const long long number = std::strtoll(id.c_str() + 1, nullptr, 10);
      if (number > max) max = number;
    }
  }
  return std::format("n{}", max + 1);
}

/// Creates a node of a catalog type with an explicit id and no children.
[[nodiscard]] inline Node MakeNode(std::string type, std::string id) {
  Node node;
  node.id = std::move(id);
  node.type = std::move(type);
  return node;
}

/// A starter document: one titled Column.
[[nodiscard]] inline Document StarterDocument(std::string name) {
  Document document;
  document.name = std::move(name);
  document.root = MakeNode("Column", "n1");
  document.root.SetProp("spacing", 12.0);
  document.root.SetModifier("padding", 24.0);
  Node title = MakeNode("Text", "n2");
  title.SetProp("text", document.name);
  title.SetProp("role", std::string("Title"));
  document.root.children.push_back(std::move(title));
  return document;
}

// --------------------------------------------------------------- operations --

struct OperationResult final {
  bool ok = false;
  std::string error;
};

/// Appends (or inserts at index) a new child under parent.
inline OperationResult AddChild(Document& document, std::string parent_id, Node child, long long index = -1) {
  Node* parent = FindNode(document, parent_id);
  if (parent == nullptr) return {.ok = false, .error = "hui: parent node not found: " + parent_id};
  const auto* def = catalog::Find(parent->type);
  if (def == nullptr) return {.ok = false, .error = "hui: unknown parent type: " + parent->type};
  if (def->children == catalog::ChildrenPolicy::None) {
    return {.ok = false, .error = parent->type + " takes no children"};
  }
  if (def->children == catalog::ChildrenPolicy::Single && !parent->children.empty()) {
    return {.ok = false, .error = parent->type + " takes a single child"};
  }
  if (FindNode(document, child.id) != nullptr) {
    return {.ok = false, .error = "hui: duplicate node id: " + child.id};
  }
  const std::size_t at = index >= 0 ? std::min(static_cast<std::size_t>(index), parent->children.size())
                               : parent->children.size();
  parent->children.insert(parent->children.begin() + static_cast<long>(at), std::move(child));
  return {.ok = true};
}

/// Removes one node; the root cannot be removed.
inline OperationResult RemoveNode(Document& document, std::string_view id) {
  if (document.root.id == id) return {.ok = false, .error = "hui: the root node cannot be removed"};
  Node* parent = FindParent(document, id);
  if (parent == nullptr) return {.ok = false, .error = "hui: node not found: " + std::string(id)};
  std::erase_if(parent->children, [id](const Node& node) { return node.id == id; });
  return {.ok = true};
}

/// Moves one node under a new parent (or within it) at an optional index.
inline OperationResult MoveNode(Document& document, std::string_view id, std::string new_parent, long long index = -1) {
  if (document.root.id == id) return {.ok = false, .error = "hui: the root node cannot be moved"};
  Node* node = FindNode(document, id);
  if (node == nullptr) return {.ok = false, .error = "hui: node not found: " + std::string(id)};
  if (Contains(*node, new_parent)) {
    return {.ok = false, .error = "hui: cannot move a node into its own subtree"};
  }
  Node copy = *node;
  if (const OperationResult removed = RemoveNode(document, id); !removed.ok) return removed;
  if (const OperationResult added = AddChild(document, std::move(new_parent), std::move(copy), index); !added.ok) {
    return added;
  }
  return {.ok = true};
}

/// Reorders one node among its siblings; offset -1 is up, +1 is down.
inline OperationResult ShiftNode(Document& document, std::string_view id, int offset) {
  Node* parent = FindParent(document, id);
  if (parent == nullptr) return {.ok = false, .error = "hui: the root node cannot be reordered"};
  const auto position = std::ranges::find_if(
      parent->children, [id](const Node& node) { return node.id == id; });
  if (position == parent->children.end()) return {.ok = false, .error = "hui: node not found"};
  const long from = static_cast<long>(position - parent->children.begin());
  const long to = std::clamp<long>(from + offset, 0, static_cast<long>(parent->children.size()) - 1);
  if (from == to) return {.ok = true};
  Node node = std::move(*position);
  parent->children.erase(position);
  parent->children.insert(parent->children.begin() + to, std::move(node));
  return {.ok = true};
}

/// Sets or removes one prop on one node.
inline OperationResult SetProp(Document& document, std::string_view id, std::string key, PropValue value,
                               bool remove = false) {
  Node* node = FindNode(document, id);
  if (node == nullptr) return {.ok = false, .error = "hui: node not found: " + std::string(id)};
  if (remove || std::holds_alternative<std::monostate>(value)) {
    node->RemoveProp(key);
    return {.ok = true};
  }
  node->SetProp(std::move(key), std::move(value));
  return {.ok = true};
}

/// Sets or removes one modifier on one node.
inline OperationResult SetModifier(Document& document, std::string_view id, std::string key, PropValue value,
                                   bool remove = false) {
  Node* node = FindNode(document, id);
  if (node == nullptr) return {.ok = false, .error = "hui: node not found: " + std::string(id)};
  if (remove || std::holds_alternative<std::monostate>(value)) {
    node->RemoveModifier(key);
    return {.ok = true};
  }
  node->SetModifier(std::move(key), std::move(value));
  return {.ok = true};
}

// ------------------------------------------------------------------ history --

/// Undo/redo stacks of document snapshots. The caller owns the current
/// document; Commit snapshots it before the next version is assigned.
struct History final {
  std::vector<Document> undo;
  std::vector<Document> redo;
};

inline void Commit(History& history, const Document& current) {
  history.undo.push_back(current);
  if (history.undo.size() > 200) history.undo.erase(history.undo.begin());
  history.redo.clear();
}

[[nodiscard]] inline bool CanUndo(const History& history) { return !history.undo.empty(); }
[[nodiscard]] inline bool CanRedo(const History& history) { return !history.redo.empty(); }

inline std::optional<Document> Undo(History& history, const Document& current) {
  if (history.undo.empty()) return std::nullopt;
  history.redo.push_back(current);
  Document previous = std::move(history.undo.back());
  history.undo.pop_back();
  return previous;
}

inline std::optional<Document> Redo(History& history, const Document& current) {
  if (history.redo.empty()) return std::nullopt;
  history.undo.push_back(current);
  Document next = std::move(history.redo.back());
  history.redo.pop_back();
  return next;
}

}  // namespace hui::doc
