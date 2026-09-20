// Shared designer state. One Editor value is threaded through every panel; it
// holds the composition-scoped document/selection/status/history handles, the
// active palette, and the single place a document mutation is published from.

export module hui.ui.editor;

import std;
import huxerui;
import hui.core.codegen;
import hui.core.doc;
import hui.core.docio;
import hui.ui.theme;

using namespace huxerui;

export namespace hui::editor {

struct Editor final {
  State<doc::Document> document;
  State<std::string> selection;
  State<std::string> status;
  State<doc::History> history;
  State<std::string> drop_hint;  // container currently highlighted by a drag
  theme::Palette palette;

  /// Publishes a mutated document: the outgoing version is snapshotted into
  /// history first, so every panel action is undoable.
  void Apply(doc::Document next) const {
    const doc::Document current = document.Get();
    history.Update([&current](doc::History& value) { doc::Commit(value, current); });
    document = std::move(next);
  }

  /// Publishes a whole new document (New / Open) and drops the selection.
  void Replace(doc::Document next, std::string message) const {
    Apply(std::move(next));
    selection = std::string();
    status = std::move(message);
  }

  void Undo() const {
    const doc::Document current = document.Get();
    std::optional<doc::Document> previous;
    history.Update([&](doc::History& value) { previous = doc::Undo(value, current); });
    if (!previous.has_value()) {
      status = std::string("nothing to undo");
      return;
    }
    document = std::move(*previous);
    status = std::string("undo");
  }

  void Redo() const {
    const doc::Document current = document.Get();
    std::optional<doc::Document> next;
    history.Update([&](doc::History& value) { next = doc::Redo(value, current); });
    if (!next.has_value()) {
      status = std::string("nothing to redo");
      return;
    }
    document = std::move(*next);
    status = std::string("redo");
  }

  void Open(const std::string& file) const {
    if (file.empty()) {
      status = std::string("set a document path first");
      return;
    }
    auto loaded = io::LoadDocument(file);
    if (!loaded.has_value()) {
      status = loaded.error();
      return;
    }
    Replace(std::move(*loaded), "loaded " + file);
  }

  void Save(const std::string& file) const {
    if (file.empty()) {
      status = std::string("set a document path first");
      return;
    }
    const auto saved = io::SaveDocument(document.Get(), file);
    status = saved.has_value() ? "saved " + file : saved.error();
  }

  void Export(const std::string& file) const {
    if (file.empty()) {
      status = std::string("set a document path first");
      return;
    }
    const std::filesystem::path cpp_path = std::filesystem::path(file).replace_extension(".cppm");
    const std::string code = codegen::GenerateCpp(
        document.Get(),
        {.style = codegen::Style::Module, .source_name = std::filesystem::path(file).filename().string()});
    const auto written = io::WriteTextFile(code, cpp_path);
    status = written.has_value() ? "exported " + cpp_path.string() : written.error();
  }
};

}  // namespace hui::editor
