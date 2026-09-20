#include "ui/ui.h"

#include <filesystem>
#include <optional>
#include <utility>

import hui.core.codegen;
import hui.core.docio;

namespace hui::ui {

void Editor::Apply(doc::Document next) const {
  const doc::Document current = document.Get();
  history.Update([&current](doc::History& value) { doc::Commit(value, current); });
  document = std::move(next);
}

void Editor::Replace(doc::Document next, std::string message) const {
  Apply(std::move(next));
  selection = std::string();
  status = std::move(message);
}

void Editor::Undo() const {
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

void Editor::Redo() const {
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

void Editor::Open(const std::string& file) const {
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

void Editor::Save(const std::string& file) const {
  if (file.empty()) {
    status = std::string("set a document path first");
    return;
  }
  const auto saved = io::SaveDocument(document.Get(), file);
  status = saved.has_value() ? "saved " + file : saved.error();
}

void Editor::Export(const std::string& file) const {
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

}  // namespace hui::ui
