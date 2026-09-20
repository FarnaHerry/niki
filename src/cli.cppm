// The hui command line: document lifecycle for humans, scripts, and CI.
//
//   hui new <name> [-o <file>]        create a starter document
//   hui validate <file...>            check documents, report issues
//   hui codegen <file> [-o <file>]    generate HuxerUI C++ (--style module|snippet)
//   hui catalog [type] [--json]       list supported components
//   hui selftest                      run the core test suite
//   hui mcp                           run the MCP server on stdio
//
// No arguments never reaches here: main() runs the designer GUI instead.

export module hui.cli;

import std;
import hui.core.catalog;
import hui.core.codegen;
import hui.core.doc;
import hui.core.docio;
import hui.core.json;
import hui.core.validate;
import hui.mcp;

export namespace hui::cli {

namespace detail {

[[nodiscard]] inline std::filesystem::path DefaultDocumentPath(std::string_view name) {
  std::string file;
  for (const char ch : name) {
    file.push_back(ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch - 'A' + 'a') : ch);
  }
  return std::filesystem::path(file).replace_extension(".hui.json");
}

inline void PrintUsage(std::ostream& out) {
  out << "hui 0.1.0 — drag-and-drop HuxerUI designer with CLI and MCP\n"
      << "\n"
      << "usage:\n"
      << "  hui                        run the designer GUI\n"
      << "  hui new <name> [-o file]   create a starter document\n"
      << "  hui validate <file...>     check documents and report issues\n"
      << "  hui codegen <file>         print HuxerUI C++ (--style module|snippet, -o file)\n"
      << "  hui catalog [type]         list components (--json for the full schema)\n"
      << "  hui selftest               run the core test suite\n"
      << "  hui mcp                    run the MCP server over stdio\n";
}

inline void PrintIssues(std::ostream& out, const std::filesystem::path& path,
                        const std::vector<validate::Issue>& issues) {
  if (issues.empty()) {
    out << std::format("{}: ok\n", path.string());
    return;
  }
  for (const auto& issue : issues) {
    const std::string where = issue.node_id.empty() ? path.string() : path.string() + " [" + issue.node_id + "]";
    out << std::format("{}: {}: {}\n", where, issue.severity, issue.message);
  }
}

// ------------------------------------------------------------------ selftest --

int SelfTest() {
  int failures = 0;
  const auto check = [&](bool condition, std::string_view name) {
    if (condition) {
      std::cout << std::format("ok   {}\n", name);
    } else {
      ++failures;
      std::cout << std::format("FAIL {}\n", name);
    }
  };

  // JSON round trip.
  {
    const std::string text = R"({"name":"CounterPage","count":3,"ok":true,"items":["A","B"],"nested":{"x":1.5}})";
    const auto parsed = json::Value::Parse(text);
    check(parsed.has_value(), "json: parse object");
    if (parsed) {
      check(parsed->GetString("name") == "CounterPage", "json: string member");
      check(parsed->GetNumber("count") == 3.0, "json: number member");
      check(parsed->GetBool("ok"), "json: bool member");
      check(parsed->Find("items") != nullptr && parsed->Find("items")->AsArray().size() == 2, "json: array member");
      const auto reparsed = json::Value::Parse(parsed->Dump());
      check(reparsed.has_value() && reparsed->Dump() == parsed->Dump(), "json: dump round trip");
    }
    check(!json::Value::Parse("{\"broken\": }").has_value(), "json: rejects broken input");
    check(!json::Value::Parse("{} trailing").has_value(), "json: rejects trailing content");
  }

  // Catalog sanity.
  {
    const auto& components = catalog::All();
    check(components.size() >= 20, "catalog: component count");
    bool unique = true;
    bool enums_ok = true;
    std::set<std::string> types;
    for (const auto& component : components) {
      unique = unique && types.insert(component.type).second;
      for (const auto& prop : component.props) {
        enums_ok = enums_ok && (prop.kind != catalog::PropKind::Enum || !prop.enum_values.empty());
      }
    }
    check(unique, "catalog: unique type names");
    check(enums_ok, "catalog: enum props carry values");
    check(catalog::Find("Column") != nullptr && catalog::Find("Nope") == nullptr, "catalog: lookup");
    check(catalog::CatalogJson().IsObject(), "catalog: json export");
    const catalog::ComponentDef* button = catalog::Find("Button");
    const catalog::ComponentDef* checkbox = catalog::Find("Checkbox");
    check(button != nullptr && catalog::FindEvent(*button, "onClick") != nullptr, "catalog: universal onClick");
    check(checkbox != nullptr && catalog::FindEvent(*checkbox, "onChanged") != nullptr, "catalog: component event");
    check(button != nullptr && catalog::FindEvent(*button, "onChanged") == nullptr, "catalog: events stay per type");
  }

  // Document IO round trip.
  {
    const doc::Document original = doc::StarterDocument("RoundTrip");
    auto saved = io::Serialize(original);
    auto loaded = io::ParseDocumentText(saved);
    check(loaded.has_value(), "docio: serialize and parse");
    if (loaded) {
      check(loaded->name == "RoundTrip" && io::Serialize(*loaded) == saved, "docio: round trip stable");
    }
    auto bad = io::ParseDocumentText("{\"format\":\"hui/1\",\"name\":\"X\"}");
    check(!bad.has_value(), "docio: missing root rejected");

    doc::Document with_events = doc::StarterDocument("Events");
    with_events.root.SetEvent("onClick", "OnRootClick");
    with_events.root.SetEvent("onChanged", std::string());
    const std::string events_text = io::Serialize(with_events);
    auto reloaded = io::ParseDocumentText(events_text);
    check(reloaded.has_value() && doc::EventText(reloaded->root, "onClick") == "OnRootClick" &&
              doc::HasEvent(reloaded->root, "onChanged") && doc::EventText(reloaded->root, "onChanged").empty() &&
              io::Serialize(*reloaded) == events_text,
          "docio: events round trip");
  }

  // Validation catches.
  {
    doc::Document document = doc::StarterDocument("BadDoc");
    doc::Node& text = document.root.children.emplace_back(doc::MakeNode("Text", "n9"));
    text.children.push_back(doc::MakeNode("Button", "n10"));  // leaf with a child
    doc::Node& select = document.root.children.emplace_back(doc::MakeNode("Select", "n11"));
    select.SetProp("options", std::vector<std::string>{});  // no options
    doc::Node& row = document.root.children.emplace_back(doc::MakeNode("Row", "n12"));
    doc::Node& bad_enum = row.children.emplace_back(doc::MakeNode("Text", "n13"));
    bad_enum.SetProp("role", std::string("Diagonal"));  // unknown enum value
    bad_enum.SetModifier("padding", -4.0);              // negative padding
    doc::Node& bad_event = document.root.children.emplace_back(doc::MakeNode("Button", "n14"));
    bad_event.SetEvent("onNope", "OnNope");  // no such event on Button
    doc::Node& bad_handler = document.root.children.emplace_back(doc::MakeNode("Button", "n15"));
    bad_handler.SetEvent("onClick", "not a handler");  // not a C++ identifier

    const auto issues = validate::Check(document);
    check(validate::HasErrors(issues), "validate: reports errors");
    bool leaf_child = false;
    bool empty_select = false;
    bool bad_enum_value = false;
    bool negative_padding = false;
    bool bad_event_key = false;
    bool bad_event_handler = false;
    for (const auto& issue : issues) {
      leaf_child = leaf_child || issue.message.find("no children") != std::string::npos;
      empty_select = empty_select || issue.message.find("at least one option") != std::string::npos;
      bad_enum_value = bad_enum_value || issue.message.find("Diagonal") != std::string::npos;
      negative_padding = negative_padding || issue.message.find("negative") != std::string::npos;
      bad_event_key = bad_event_key || issue.message.find("has no event") != std::string::npos;
      bad_event_handler = bad_event_handler || issue.message.find("not a C++ identifier") != std::string::npos;
    }
    check(leaf_child, "validate: leaf with child");
    check(empty_select, "validate: Select without options");
    check(bad_enum_value, "validate: unknown enum value");
    check(negative_padding, "validate: negative padding");
    check(bad_event_key, "validate: unknown event key");
    check(bad_event_handler, "validate: event handler identifier");
  }

  // Operations.
  {
    doc::Document document = doc::StarterDocument("Ops");
    const std::string child_id = doc::NextId(document);
    check(doc::AddChild(document, "n1", doc::MakeNode("Button", child_id)).ok, "ops: add child");
    check(doc::AddChild(document, child_id, doc::MakeNode("Text", doc::NextId(document))).error.find("no children") !=
              std::string::npos,
          "ops: reject child under leaf");
    check(doc::MoveNode(document, child_id, child_id).error.find("own subtree") != std::string::npos,
          "ops: reject move into own subtree");
    check(doc::ShiftNode(document, child_id, -1).ok, "ops: shift node");
    check(doc::RemoveNode(document, "n1").error.find("root") != std::string::npos, "ops: protect the root");
    check(doc::RemoveNode(document, child_id).ok, "ops: remove node");

    doc::History history;
    doc::Document before = document;
    doc::Commit(history, before);
    document.name = "Changed";
    check(doc::CanUndo(history), "ops: history undo available");
    auto undone = doc::Undo(history, document);
    check(undone.has_value() && undone->name == before.name, "ops: undo restores");
    auto redone = doc::Redo(history, *undone);
    check(redone.has_value() && redone->name == "Changed", "ops: redo restores");
  }

  // Code generation golden shape.
  {
    const std::string counter = R"({
      "format": "hui/1", "name": "CounterPage",
      "root": {"id": "n1", "type": "Column",
        "props": {"spacing": 12.0},
        "modifiers": {"padding": 24.0},
        "children": [
          {"id": "n2", "type": "Text", "props": {"text": "Count: 0", "role": "Title"}},
          {"id": "n3", "type": "Row", "children": [
            {"id": "n4", "type": "Button", "props": {"label": "Increment"},
             "events": {"onClick": "OnIncrement"}},
            {"id": "n5", "type": "Spacer"},
            {"id": "n6", "type": "Button", "props": {"label": "Reset"},
             "events": {"onClick": ""}}
          ], "props": {"spacing": 8.0}}
        ]}
    })";
    auto parsed = io::ParseDocumentText(counter);
    check(parsed.has_value(), "codegen: parse golden document");
    if (parsed) {
      const std::string module_cpp = codegen::GenerateCpp(*parsed, {.style = codegen::Style::Module});
      check(module_cpp.find("export module counter_page;") != std::string::npos, "codegen: module name");
      check(module_cpp.find("export View CounterPage()") != std::string::npos, "codegen: view function");
      check(module_cpp.find("Text(\"Count: 0\", TextRole::Title)") != std::string::npos, "codegen: text role");
      check(module_cpp.find("}.With(Spacing(8.0F))") != std::string::npos, "codegen: row spacing");
      check(module_cpp.find("}.With(Padding(24.0F), Spacing(12.0F))") != std::string::npos,
            "codegen: with-list order");
      check(module_cpp.find("Button(\"Increment\").OnClick(OnIncrement)") != std::string::npos,
            "codegen: named event binding");
      check(module_cpp.find("TODO onClick handler for Button n6") != std::string::npos,
            "codegen: placeholder event binding");
      check(module_cpp.find("//   void OnIncrement();") != std::string::npos &&
                module_cpp.find("Button n4 onClick") != std::string::npos,
            "codegen: handler manifest");
      const std::string snippet = codegen::GenerateCpp(*parsed, {.style = codegen::Style::Snippet});
      check(snippet.starts_with("return Column {") && snippet.find("export module") == std::string::npos,
            "codegen: snippet style");
      check(snippet.find(".OnClick(OnIncrement)") != std::string::npos, "codegen: snippet keeps events");
      // Validation of the same document is clean.
      const auto issues = validate::Check(*parsed);
      check(!validate::HasErrors(issues), "codegen: golden document validates");
    }

    // Three or more modifiers take the multi-line branch; it must not leave a
    // trailing comma behind.
    const std::string many = R"({
      "format": "hui/1", "name": "ManyMods",
      "root": {"id": "n1", "type": "Column", "props": {"spacing": 4.0},
        "modifiers": {"padding": 8.0, "cornerRadius": 6.0, "fontSize": 15.0},
        "children": [{"id": "n2", "type": "Text", "props": {"text": "hi"}}]}
    })";
    auto parsed_many = io::ParseDocumentText(many);
    check(parsed_many.has_value(), "codegen: parse multi-modifier document");
    if (parsed_many) {
      const std::string many_cpp = codegen::GenerateCpp(*parsed_many, {.style = codegen::Style::Module});
      check(many_cpp.find("CornerRadius(6.0F)") != std::string::npos, "codegen: multi-line modifiers emitted");
      check(many_cpp.find(",\n  )") == std::string::npos, "codegen: no trailing comma in With");
    }
  }

  std::cout << std::format("\n{} failure(s)\n", failures);
  return failures == 0 ? 0 : 1;
}

// ---------------------------------------------------------------- subcommands --

int RunNew(const std::vector<std::string>& args) {
  std::string name;
  std::string output;
  for (std::size_t i = 0; i < args.size(); ++i) {
    if (args[i] == "-o" && i + 1 < args.size()) {
      output = args[++i];
    } else if (name.empty()) {
      name = args[i];
    }
  }
  if (name.empty()) {
    std::cerr << "hui new: a document name is required\n";
    return 2;
  }
  const doc::Document document = doc::StarterDocument(name);
  if (output.empty()) {
    output = DefaultDocumentPath(name).string();
  }
  const auto saved = io::SaveDocument(document, output);
  if (!saved) {
    std::cerr << saved.error() << "\n";
    return 1;
  }
  std::cout << std::format("created {}\n", output);
  return 0;
}

int RunValidate(const std::vector<std::string>& args) {
  if (args.empty()) {
    std::cerr << "hui validate: at least one file is required\n";
    return 2;
  }
  int exit_code = 0;
  for (const auto& file : args) {
    auto document = io::LoadDocument(file);
    if (!document) {
      std::cerr << document.error() << "\n";
      exit_code = 1;
      continue;
    }
    const auto issues = validate::Check(*document);
    PrintIssues(std::cout, file, issues);
    if (validate::HasErrors(issues)) {
      exit_code = 1;
    }
  }
  return exit_code;
}

int RunCodegen(const std::vector<std::string>& args) {
  std::string file;
  std::string output;
  codegen::Style style = codegen::Style::Module;
  for (std::size_t i = 0; i < args.size(); ++i) {
    if (args[i] == "-o" && i + 1 < args.size()) {
      output = args[++i];
    } else if (args[i] == "--style" && i + 1 < args.size()) {
      const std::string value = args[++i];
      if (value == "snippet") {
        style = codegen::Style::Snippet;
      } else if (value != "module") {
        std::cerr << std::format("hui codegen: unknown style '{}'\n", value);
        return 2;
      }
    } else if (file.empty()) {
      file = args[i];
    }
  }
  if (file.empty()) {
    std::cerr << "hui codegen: a document file is required\n";
    return 2;
  }
  auto document = io::LoadDocument(file);
  if (!document) {
    std::cerr << document.error() << "\n";
    return 1;
  }
  const std::string code = codegen::GenerateCpp(
      *document, {.style = style, .source_name = std::filesystem::path(file).filename().string()});
  if (output.empty()) {
    std::cout << code;
    return 0;
  }
  const auto written = io::WriteTextFile(code, output);
  if (!written) {
    std::cerr << written.error() << "\n";
    return 1;
  }
  std::cout << std::format("wrote {}\n", output);
  return 0;
}

int RunCatalog(const std::vector<std::string>& args) {
  bool as_json = false;
  std::string type;
  for (const auto& arg : args) {
    if (arg == "--json") {
      as_json = true;
    } else if (type.empty()) {
      type = arg;
    }
  }
  if (as_json) {
    std::cout << catalog::CatalogJson().DumpPretty() << "\n";
    return 0;
  }
  if (!type.empty()) {
    const auto* component = catalog::Find(type);
    if (component == nullptr) {
      std::cerr << std::format("hui catalog: unknown component '{}'\n", type);
      return 1;
    }
    std::cout << std::format("{} [{}] {}\n", component->type, component->category, component->description);
    std::cout << std::format("children: {}\n", catalog::ChildrenPolicyName(component->children));
    for (const auto& prop : component->props) {
      std::cout << std::format("  {} = {}{}\n", prop.key, prop.default_value.Dump(),
                               prop.kind == catalog::PropKind::Enum
                                   ? " (" + std::ranges::fold_left(prop.enum_values, std::string(),
                                                                   [](const std::string& acc, const std::string& v) {
                                                                     return acc.empty() ? v : acc + "|" + v;
                                                                   }) + ")"
                                   : "");
    }
    for (const auto* event : catalog::EventsOf(*component)) {
      std::cout << std::format("  .{} {} — {}\n", event->key, event->signature, event->description);
    }
    return 0;
  }
  std::string category;
  for (const auto& component : catalog::All()) {
    if (component.category != category) {
      category = component.category;
      std::cout << std::format("\n{}:\n", category);
    }
    std::cout << std::format("  {} {} — {}\n", component.glyph, component.type, component.description);
  }
  std::cout << "\nmodifiers (every node): ";
  for (std::size_t i = 0; i < catalog::Modifiers().size(); ++i) {
    if (i > 0) std::cout << ", ";
    std::cout << catalog::Modifiers()[i].key;
  }
  std::cout << "\nevents (every node): ";
  for (std::size_t i = 0; i < catalog::UniversalEvents().size(); ++i) {
    if (i > 0) std::cout << ", ";
    std::cout << catalog::UniversalEvents()[i].key;
  }
  std::cout << "  — per-component events: hui catalog <type>\n";
  return 0;
}

}  // namespace detail

/// Dispatches one CLI invocation; returns the process exit code.
int Run(int argc, char** argv) {
  const std::vector<std::string> args(argv + 1, argv + argc);
  if (args.empty()) {
    detail::PrintUsage(std::cerr);
    return 2;
  }
  const std::string& command = args.front();
  const std::vector<std::string> rest(args.begin() + 1, args.end());

  if (command == "new") return detail::RunNew(rest);
  if (command == "validate") return detail::RunValidate(rest);
  if (command == "codegen") return detail::RunCodegen(rest);
  if (command == "catalog") return detail::RunCatalog(rest);
  if (command == "selftest") return detail::SelfTest();
  if (command == "mcp") return mcp::Run();
  if (command == "help" || command == "--help" || command == "-h") {
    detail::PrintUsage(std::cout);
    return 0;
  }
  std::cerr << std::format("hui: unknown command '{}'\n\n", command);
  detail::PrintUsage(std::cerr);
  return 2;
}

}  // namespace hui::cli
