// The hui MCP server: the same document engine the GUI and CLI use, exposed
// over the Model Context Protocol on stdio so an AI agent can read the
// component catalog, build and mutate layout documents, validate them, and get
// back compilable HuxerUI C++.
//
// Transport: newline-delimited JSON-RPC 2.0 (one message per line), which is
// what the MCP stdio transport specifies. stdout carries only protocol frames;
// diagnostics go to stderr.

export module hui.mcp;

import std;
import hui.core.catalog;
import hui.core.codegen;
import hui.core.doc;
import hui.core.docio;
import hui.core.json;
import hui.core.validate;

export namespace hui::mcp {

namespace detail {

// ------------------------------------------------------------------ framing --

json::Value MakeResult(const json::Value& id, json::Value result) {
  return json::Value::ObjectWith({{"jsonrpc", json::Value("2.0")}, {"id", id}, {"result", std::move(result)}});
}

json::Value MakeError(const json::Value& id, int code, std::string message) {
  return json::Value::ObjectWith(
      {{"jsonrpc", json::Value("2.0")},
       {"id", id},
       {"error", json::Value::ObjectWith({{"code", json::Value(code)}, {"message", json::Value(std::move(message))}})}});
}

json::Value TextContent(std::string text) {
  return json::Value::ObjectWith(
      {{"type", json::Value("text")}, {"text", json::Value(std::move(text))}});
}

// ------------------------------------------------------------------- tools ---

const char* const kFormatDoc = R"(hui layout document (format "hui/1")
=========================================
A document is a JSON tree of HuxerUI component nodes:

{
  "format": "hui/1",
  "name": "CounterPage",          // C++ identifier; becomes View CounterPage()
  "root": {
    "id": "n1", "type": "Column", // type must exist in the hui catalog
    "props": {...},               // component props; absent = catalog default
    "modifiers": {...},           // shared layout/paint modifiers, all optional
    "events": {"onClick": "OnReset"}, // event key -> handler name; "" = TODO placeholder
    "children": [ ... ]
  }
}

Nodes need stable unique ids ("n1", "n2", ...). Containers (Column, Row, Flow,
Stack) take any children; ScrollView and the theme components take one; the
rest are leaves. Props, events, and modifiers are listed per component by
hui_catalog; every node also accepts the shared modifiers and the "onClick"
event. Event handler names must be C++ identifiers and are emitted verbatim as
X.OnClick(Handler) in the generated code, with a trailing comment listing the
signatures the enclosing scope must provide. An empty handler name emits a
compiling placeholder lambda instead. Colors are "#RRGGBB" or "#RRGGBBAA".)";

[[nodiscard]] json::Value ToolsList() {
  const auto object_schema = [](std::string_view description) {
    return json::Value::ObjectWith(
        {{"type", json::Value("object")},
         {"description", json::Value(std::string(description))},
         {"properties", json::Value::ObjectWith({})},
         {"additionalProperties", json::Value(true)}});
  };
  const auto document_prop = [] {
    return json::Value::ObjectWith({
        {"type", json::Value("object")},
        {"description",
         json::Value("A complete hui layout document (format hui/1), as returned by hui_new_document or a "
                     "previous hui_apply")},
    });
  };

  std::vector<json::Value> tools;
  tools.push_back(json::Value::ObjectWith({
      {"name", json::Value("hui_catalog")},
      {"description",
       json::Value("Every HuxerUI component hui supports: props with types, enum values and defaults, child policy "
                   "(none/any/single), bindable events with their handler signatures, the shared modifier list, and "
                   "the document format. Call this before building a layout.")},
      {"inputSchema", object_schema("No inputs.")},
  }));
  tools.push_back(json::Value::ObjectWith({
      {"name", json::Value("hui_new_document")},
      {"description", json::Value("Creates a starter document with one titled Column.")},
      {"inputSchema",
       json::Value::ObjectWith({
           {"type", json::Value("object")},
           {"properties",
            json::Value::ObjectWith(
                {{"name", json::Value::ObjectWith({{"type", json::Value("string")},
                                                   {"description", json::Value("C++ identifier, e.g. LoginPage")}})}})},
           {"required", json::Value(std::vector<json::Value>{"name"})},
       })},
  }));
  tools.push_back(json::Value::ObjectWith({
      {"name", json::Value("hui_validate")},
      {"description", json::Value("Checks a document: unknown types, wrong prop kinds or enum values, children under "
                                  "leaves, duplicate ids, out-of-range values. Returns issues with node ids.")},
      {"inputSchema",
       json::Value::ObjectWith({{"type", json::Value("object")},
                                {"properties", json::Value::ObjectWith({{"document", document_prop()}})},
                                {"required", json::Value(std::vector<json::Value>{"document"})}})},
  }));
  tools.push_back(json::Value::ObjectWith({
      {"name", json::Value("hui_generate_code")},
      {"description",
       json::Value("Generates HuxerUI C++ from a document. Style \"module\" (default) returns a complete import "
                   "huxerui module unit; \"snippet\" returns a return statement to paste into an existing "
                   "composable.")},
      {"inputSchema",
       json::Value::ObjectWith({{"type", json::Value("object")},
                                {"properties",
                                 json::Value::ObjectWith(
                                     {{"document", document_prop()},
                                      {"style",
                                       json::Value::ObjectWith(
                                           {{"type", json::Value("string")},
                                            {"enum", json::Value(std::vector<json::Value>{"module", "snippet"})}})}})},
                                {"required", json::Value(std::vector<json::Value>{"document"})}})},
  }));
  tools.push_back(json::Value::ObjectWith({
      {"name", json::Value("hui_apply")},
      {"description",
       json::Value("Applies a batch of operations to a document and returns the updated document plus validation. "
                   "Operations: {\"op\":\"add\",\"type\":...,\"parent\":<id>,\"index\"?:n,\"props\"?:{},\"modifiers\"?:"
                   "{},\"events\"?:{},\"id\"?:new-id}; {\"op\":\"update\",\"id\":...,\"props\"?:{},\"removeProps\"?:[],"
                   "\"modifiers\"?:{},\"removeModifiers\"?:[],\"events\"?:{\"onClick\":\"Handler\"},"
                   "\"removeEvents\"?:[]}; {\"op\":\"remove\",\"id\":...}; {\"op\":\"move\",\"id\":...,"
                   "\"parent\":<id>,\"index\"?:n}; {\"op\":\"shift\",\"id\":...,\"offset\":-1|1}; "
                   "{\"op\":\"rename\",\"name\":...}. Events map an event key to a handler name; an empty handler "
                   "emits a placeholder, and null removes the binding. Batch order is sequential; the first failure "
                   "stops the batch.")},
      {"inputSchema",
       json::Value::ObjectWith({{"type", json::Value("object")},
                                {"properties",
                                 json::Value::ObjectWith({{"document", document_prop()},
                                                          {"operations",
                                                           json::Value::ObjectWith(
                                                               {{"type", json::Value("array")},
                                                                {"description",
                                                                 json::Value("Operations applied in order.")}})}})},
                                {"required", json::Value(std::vector<json::Value>{"document", "operations"})}})},
  }));
  tools.push_back(json::Value::ObjectWith({
      {"name", json::Value("hui_outline")},
      {"description", json::Value("A compact indented outline of one document: node type, id, and summary per line. "
                                  "The cheap way to re-read a document you are iterating on.")},
      {"inputSchema",
       json::Value::ObjectWith({{"type", json::Value("object")},
                                {"properties", json::Value::ObjectWith({{"document", document_prop()}})},
                                {"required", json::Value(std::vector<json::Value>{"document"})}})},
  }));
  return json::Value(std::move(tools));
}

// prop/modifier JSON -> PropValue, checked against the catalog kind.
[[nodiscard]] std::expected<doc::PropValue, std::string> JsonToTypedValue(const json::Value& value,
                                                                          const catalog::PropDef& def) {
  switch (def.kind) {
    case catalog::PropKind::Text:
      if (value.IsString()) return doc::PropValue(value.AsString());
      break;
    case catalog::PropKind::Number:
      if (value.IsNumber()) return doc::PropValue(value.AsNumber());
      break;
    case catalog::PropKind::Bool:
      if (value.IsBool()) return doc::PropValue(value.AsBool());
      break;
    case catalog::PropKind::Enum:
      if (value.IsString()) return doc::PropValue(value.AsString());
      break;
    case catalog::PropKind::StrList: {
      if (!value.IsArray()) break;
      std::vector<std::string> items;
      for (const auto& item : value.AsArray()) items.push_back(item.AsString());
      return doc::PropValue(std::move(items));
    }
    case catalog::PropKind::Color:
      if (value.IsString()) return doc::PropValue(value.AsString());
      break;
  }
  return std::unexpected(std::format("value for '{}' must be JSON {}", def.key, [&] {
    switch (def.kind) {
      case catalog::PropKind::Text: return "string";
      case catalog::PropKind::Number: return "number";
      case catalog::PropKind::Bool: return "bool";
      case catalog::PropKind::Enum: return "string (enum)";
      case catalog::PropKind::StrList: return "array of strings";
      case catalog::PropKind::Color: return "string (#RRGGBB or #RRGGBBAA)";
    }
    return "string";
  }()));
}

[[nodiscard]] std::expected<doc::PropValue, std::string> PropFromJson(const doc::Node& node, bool modifier,
                                                                      const std::string& key,
                                                                      const json::Value& value) {
  if (value.IsNull()) return doc::PropValue(std::monostate{});
  const catalog::PropDef* def = nullptr;
  if (modifier) {
    def = catalog::FindModifier(key);
  } else {
    const auto* component = catalog::Find(node.type);
    if (component != nullptr) def = catalog::FindProp(*component, key);
  }
  if (def == nullptr) {
    return std::unexpected(
        std::format("unknown {} '{}' on {}", modifier ? "modifier" : "property", key, node.type));
  }
  return JsonToTypedValue(value, *def);
}

struct ToolError final {
  std::string message;
};

// Applies one operation; returns an error message or nullopt.
[[nodiscard]] std::optional<std::string> ApplyOperation(doc::Document& document, const json::Value& operation) {
  if (!operation.IsObject()) return "each operation must be an object";
  const std::string op = operation.GetString("op");
  if (op == "add") {
    const std::string type = operation.GetString("type");
    if (catalog::Find(type) == nullptr) return "unknown component type: " + type;
    std::string id = operation.GetString("id");
    if (id.empty()) id = doc::NextId(document);
    doc::Node node = doc::MakeNode(type, id);
    if (const json::Value* props = operation.Find("props"); props != nullptr && props->IsObject()) {
      for (const auto& [key, value] : props->AsObject()) {
        auto typed = PropFromJson(node, false, key, value);
        if (!typed) return typed.error();
        node.SetProp(key, std::move(*typed));
      }
    }
    if (const json::Value* modifiers = operation.Find("modifiers"); modifiers != nullptr && modifiers->IsObject()) {
      for (const auto& [key, value] : modifiers->AsObject()) {
        auto typed = PropFromJson(node, true, key, value);
        if (!typed) return typed.error();
        node.SetModifier(key, std::move(*typed));
      }
    }
    if (const json::Value* events = operation.Find("events"); events != nullptr && events->IsObject()) {
      const catalog::ComponentDef* component = catalog::Find(type);
      for (const auto& [key, value] : events->AsObject()) {
        if (catalog::FindEvent(*component, key) == nullptr) return "unknown event '" + key + "' on " + type;
        node.SetEvent(key, value.IsString() ? value.AsString() : std::string());
      }
    }
    long long index = -1;
    if (const json::Value* raw = operation.Find("index"); raw != nullptr && raw->IsNumber()) {
      index = raw->AsInteger();
    }
    const std::string parent = operation.GetString("parent", document.root.id);
    const doc::OperationResult result = doc::AddChild(document, parent, std::move(node), index);
    return result.ok ? std::nullopt : std::optional(result.error);
  }
  if (op == "update") {
    const std::string id = operation.GetString("id");
    doc::Node* node = doc::FindNode(document, id);
    if (node == nullptr) return "node not found: " + id;
    if (const json::Value* props = operation.Find("props"); props != nullptr && props->IsObject()) {
      for (const auto& [key, value] : props->AsObject()) {
        auto typed = PropFromJson(*node, false, key, value);
        if (!typed) return typed.error();
        if (std::holds_alternative<std::monostate>(*typed)) {
          node->RemoveProp(key);
        } else {
          node->SetProp(key, std::move(*typed));
        }
      }
    }
    if (const json::Value* remove_props = operation.Find("removeProps"); remove_props != nullptr) {
      for (const auto& key : remove_props->AsArray()) {
        node->RemoveProp(key.AsString());
      }
    }
    if (const json::Value* modifiers = operation.Find("modifiers"); modifiers != nullptr && modifiers->IsObject()) {
      for (const auto& [key, value] : modifiers->AsObject()) {
        auto typed = PropFromJson(*node, true, key, value);
        if (!typed) return typed.error();
        if (std::holds_alternative<std::monostate>(*typed)) {
          node->RemoveModifier(key);
        } else {
          node->SetModifier(key, std::move(*typed));
        }
      }
    }
    if (const json::Value* remove_modifiers = operation.Find("removeModifiers"); remove_modifiers != nullptr) {
      for (const auto& key : remove_modifiers->AsArray()) {
        node->RemoveModifier(key.AsString());
      }
    }
    if (const json::Value* events = operation.Find("events"); events != nullptr && events->IsObject()) {
      const catalog::ComponentDef* component = catalog::Find(node->type);
      for (const auto& [key, value] : events->AsObject()) {
        if (component == nullptr || catalog::FindEvent(*component, key) == nullptr) {
          return "unknown event '" + key + "' on " + node->type;
        }
        if (value.IsNull()) {
          node->RemoveEvent(key);
        } else {
          node->SetEvent(key, value.IsString() ? value.AsString() : std::string());
        }
      }
    }
    if (const json::Value* remove_events = operation.Find("removeEvents"); remove_events != nullptr) {
      for (const auto& key : remove_events->AsArray()) {
        node->RemoveEvent(key.AsString());
      }
    }
    return std::nullopt;
  }
  if (op == "remove") {
    const doc::OperationResult result = doc::RemoveNode(document, operation.GetString("id"));
    return result.ok ? std::nullopt : std::optional(result.error);
  }
  if (op == "move") {
    long long index = -1;
    if (const json::Value* raw = operation.Find("index"); raw != nullptr && raw->IsNumber()) {
      index = raw->AsInteger();
    }
    const doc::OperationResult result =
        doc::MoveNode(document, operation.GetString("id"), operation.GetString("parent", document.root.id), index);
    return result.ok ? std::nullopt : std::optional(result.error);
  }
  if (op == "shift") {
    int offset = 0;
    if (const json::Value* raw = operation.Find("offset"); raw != nullptr && raw->IsNumber()) {
      offset = static_cast<int>(raw->AsInteger());
    }
    const doc::OperationResult result = doc::ShiftNode(document, operation.GetString("id"), offset);
    return result.ok ? std::nullopt : std::optional(result.error);
  }
  if (op == "rename") {
    const std::string name = operation.GetString("name");
    if (name.empty()) return "rename requires a name";
    document.name = name;
    return std::nullopt;
  }
  return "unknown operation op: " + op;
}

void OutlineInto(std::string& out, const doc::Node& node, std::size_t depth) {
  out += std::string(depth * 2, ' ') + std::format("{} {} — {}\n", node.type, node.id, doc::SummaryOf(node));
  for (const auto& child : node.children) {
    OutlineInto(out, child, depth + 1);
  }
}

// One tools/call dispatch. Returns the MCP result object.
[[nodiscard]] json::Value CallTool(const std::string& name, const json::Value& arguments) {
  const auto fail = [](std::string message) {
    return json::Value::ObjectWith(
        {{"content", json::Value(std::vector<json::Value>{TextContent(std::move(message))})}, {"isError", json::Value(true)}});
  };

  if (name == "hui_catalog") {
    json::Value catalog = catalog::CatalogJson();
    catalog.Set("formatDoc", json::Value(kFormatDoc));
    return json::Value::ObjectWith(
        {{"content", json::Value(std::vector<json::Value>{TextContent(catalog.DumpPretty())})}});
  }
  if (name == "hui_new_document") {
    const std::string doc_name = arguments.GetString("name", "NewPage");
    const doc::Document document = doc::StarterDocument(doc_name);
    return json::Value::ObjectWith(
        {{"content", json::Value(std::vector<json::Value>{TextContent(io::Serialize(document))})}});
  }

  const json::Value* document_value = arguments.Find("document");
  if (document_value == nullptr) {
    return fail("hui: tool " + name + " requires a document");
  }
  auto document = io::ParseDocument(*document_value);
  if (!document) {
    return fail(document.error());
  }

  if (name == "hui_validate") {
    const auto issues = validate::Check(*document);
    return json::Value::ObjectWith(
        {{"content", json::Value(std::vector<json::Value>{TextContent(validate::IssuesJson(issues).DumpPretty())})}});
  }
  if (name == "hui_generate_code") {
    const std::string style = arguments.GetString("style", "module");
    const codegen::Options options{
        .style = style == "snippet" ? codegen::Style::Snippet : codegen::Style::Module,
        .source_name = document->name + ".hui.json",
    };
    return json::Value::ObjectWith(
        {{"content", json::Value(std::vector<json::Value>{TextContent(codegen::GenerateCpp(*document, options))})}});
  }
  if (name == "hui_outline") {
    std::string outline = std::format("document {} (format {})\n", document->name, document->format);
    OutlineInto(outline, document->root, 0);
    return json::Value::ObjectWith(
        {{"content", json::Value(std::vector<json::Value>{TextContent(outline)})}});
  }
  if (name == "hui_apply") {
    const json::Value* operations = arguments.Find("operations");
    if (operations == nullptr || !operations->IsArray()) {
      return fail("hui: hui_apply requires an operations array");
    }
    for (const auto& operation : operations->AsArray()) {
      if (const auto failure = ApplyOperation(*document, operation)) {
        return fail("hui: " + *failure);
      }
    }
    json::Value result = json::Value::ObjectWith({
        {"document", io::DocumentToJson(*document)},
        {"validation", validate::IssuesJson(validate::Check(*document))},
    });
    return json::Value::ObjectWith(
        {{"content", json::Value(std::vector<json::Value>{TextContent(result.DumpPretty())})}});
  }
  return fail("hui: unknown tool: " + name);
}

[[nodiscard]] json::Value ResourcesList() {
  const auto resource = [](const char* uri, const char* name, const char* description) {
    return json::Value::ObjectWith({
        {"uri", json::Value(uri)},
        {"name", json::Value(name)},
        {"description", json::Value(description)},
        {"mimeType", json::Value("application/json")},
    });
  };
  return json::Value(std::vector<json::Value>{
      resource("hui://catalog", "hui component catalog",
               "Every supported HuxerUI component with props, defaults, and child policy."),
      resource("hui://format", "hui document format", "The hui/1 layout document format."),
  });
}

[[nodiscard]] json::Value ResourceRead(const std::string& uri) {
  if (uri == "hui://catalog") {
    return json::Value::ObjectWith(
        {{"contents",
          json::Value(std::vector<json::Value>{json::Value::ObjectWith(
              {{"uri", json::Value(uri)},
               {"mimeType", json::Value("application/json")},
               {"text", json::Value(catalog::CatalogJson().DumpPretty())}})})}});
  }
  if (uri == "hui://format") {
    return json::Value::ObjectWith(
        {{"contents",
          json::Value(std::vector<json::Value>{json::Value::ObjectWith(
              {{"uri", json::Value(uri)}, {"mimeType", json::Value("text/markdown")}, {"text", json::Value(kFormatDoc)}})})}});
  }
  throw std::invalid_argument("hui: unknown resource: " + uri);
}

// ---------------------------------------------------------------- dispatch ---

[[nodiscard]] json::Value HandleRequest(const json::Value& request) {
  const json::Value* id = request.Find("id");
  const json::Value id_or_null = id != nullptr ? *id : json::Value(nullptr);
  const std::string method = request.GetString("method");

  if (method == "initialize") {
    const json::Value* params = request.Find("params");
    std::string protocol = "2024-11-05";
    if (params != nullptr) {
      const std::string requested = params->GetString("protocolVersion");
      if (requested == "2025-06-18" || requested == "2025-03-26" || requested == "2024-11-05" ||
          requested == "2024-10-07") {
        protocol = requested;
      }
    }
    return MakeResult(id_or_null,
                      json::Value::ObjectWith({
                          {"protocolVersion", json::Value(protocol)},
                          {"capabilities",
                           json::Value::ObjectWith({
                               {"tools", json::Value::ObjectWith({{"listChanged", json::Value(false)}})},
                               {"resources", json::Value::ObjectWith({{"subscribe", json::Value(false)},
                                                                       {"listChanged", json::Value(false)}})},
                           })},
                          {"serverInfo",
                           json::Value::ObjectWith({{"name", json::Value("hui")}, {"version", json::Value("0.1.0")}})},
                          {"instructions",
                           json::Value(std::string("Build HuxerUI layouts as hui/1 documents: read hui://catalog, "
                                                   "create a document with hui_new_document, edit it with hui_apply "
                                                   "(batches of add/update/move/remove operations over props, "
                                                   "modifiers, and events), validate with hui_validate, and generate "
                                                   "C++ with hui_generate_code. Events map an event key such as "
                                                   "onClick to an application handler name (or \"\" for a compiling "
                                                   "placeholder). Documents round-trip exactly; ids stay stable "
                                                   "across operations."))},
                      }));
  }
  if (method == "ping") {
    return MakeResult(id_or_null, json::Value::ObjectWith({}));
  }
  if (method == "tools/list") {
    return MakeResult(id_or_null, json::Value::ObjectWith({{"tools", ToolsList()}}));
  }
  if (method == "tools/call") {
    const json::Value* params = request.Find("params");
    if (params == nullptr) {
      return MakeError(id_or_null, -32602, "hui: tools/call requires params");
    }
    return MakeResult(id_or_null, CallTool(params->GetString("name"),
                                           params->Find("arguments") != nullptr ? *params->Find("arguments")
                                                                               : json::Value::ObjectWith({})));
  }
  if (method == "resources/list") {
    return MakeResult(id_or_null, json::Value::ObjectWith({{"resources", ResourcesList()}}));
  }
  if (method == "resources/read") {
    const json::Value* params = request.Find("params");
    if (params == nullptr) {
      return MakeError(id_or_null, -32602, "hui: resources/read requires params");
    }
    try {
      return MakeResult(id_or_null, ResourceRead(params->GetString("uri")));
    } catch (const std::exception& error) {
      return MakeError(id_or_null, -32602, error.what());
    }
  }
  return MakeError(id_or_null, -32601, "hui: method not found: " + method);
}

}  // namespace detail

/// Runs the MCP server over stdio until stdin closes. Returns the exit code.
int Run() {
  std::ios::sync_with_stdio(false);
  std::string line;
  while (std::getline(std::cin, line)) {
    if (line.empty()) continue;
    auto parsed = json::Value::Parse(line);
    if (!parsed) {
      std::cerr << "hui-mcp: dropping malformed frame: " << parsed.error() << "\n";
      continue;
    }
    if (!parsed->IsObject() || !parsed->Has("method")) continue;
    if (!parsed->Has("id")) continue;  // notification, including initialized
    try {
      const std::string response = detail::HandleRequest(*parsed).Dump();
      std::cout << response << "\n" << std::flush;
    } catch (const std::exception& error) {
      const json::Value id = parsed->Find("id") != nullptr ? *parsed->Find("id") : json::Value(nullptr);
      std::cout << detail::MakeError(id, -32603, std::string("hui: internal error: ") + error.what()).Dump() << "\n"
                << std::flush;
    }
  }
  return 0;
}

}  // namespace hui::mcp
