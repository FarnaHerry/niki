#include "ui/ui.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

import hui.core.codegen;
import hui.core.docio;

namespace hui::ui {
namespace {

/// The tab label for a path: the file's own name. A page that has no file yet
/// keeps whatever name it was started with.
[[nodiscard]] std::string NameFor(const std::string& path, const doc::Document& document) {
  if (path.empty()) {
    return document.name;
  }
  std::string name = std::filesystem::path(path).filename().string();
  return name.empty() ? document.name : name;
}

/// "NewPage" -> "new_page", so a page's default file name follows the CLI's.
[[nodiscard]] std::string SnakeCase(std::string_view name) {
  std::string out;
  out.reserve(name.size());
  for (const char ch : name) {
    out.push_back(ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch - 'A' + 'a') : ch);
  }
  return out;
}

/// Every path the page remembers is normalised the same way, so opening the same
/// file twice cannot look like two different pages.
[[nodiscard]] std::string Normalize(const std::string& file) {
  return std::filesystem::path(file).lexically_normal().string();
}

}  // namespace

Page StarterPage(std::string title, std::string path) {
  Page page;
  page.document = doc::StarterDocument(title.empty() ? std::string("NewPage") : title);
  page.path = std::move(path);
  page.title = title.empty() ? NameFor(page.path, page.document) : std::move(title);
  return page;
}

std::size_t Editor::ActiveIndex() const {
  const std::size_t count = pages.Size();
  if (count == 0) {
    throw std::logic_error("hui: the designer always has at least one page");
  }
  return std::min(active.Get(), count - 1);
}

const Page& Editor::Current() const { return pages.At(ActiveIndex()); }

const doc::Document& Editor::Document() const { return Current().document; }

const std::string& Editor::Selection() const { return Current().selection; }

const std::string& Editor::Status() const { return status.Get(); }

const std::string& Editor::Hint() const { return drop_hint.Get(); }

void Editor::Edit(const std::function<void(Page&)>& mutate) const {
  Page page = Current();
  mutate(page);
  pages.Set(ActiveIndex(), std::move(page));
}

void Editor::Select(std::string id) const {
  Edit([&id](Page& page) { page.selection = std::move(id); });
  // Choosing a node means the keyboard is working on the canvas again, whatever
  // field had it before.
  focused_fields = 0;
}

void Editor::SetStatus(std::string message) const { status = std::move(message); }

void Editor::SetHint(std::string id) const { drop_hint = std::move(id); }

bool Editor::Editing() const { return focused_fields.Get() > 0; }

void Editor::FocusField(bool focused) const {
  focused_fields.Update([focused](int& count) { count = std::max(0, count + (focused ? 1 : -1)); });
}

void Editor::DeleteSelected() const {
  const std::string id = Selection();
  if (id.empty()) {
    return;
  }
  doc::Document next = Document();
  const doc::OperationResult result = doc::RemoveNode(next, id);
  if (!result.ok) {
    status = result.error;
    return;
  }
  Apply(std::move(next));
  Select(std::string());
  status = "deleted " + id;
}

void Editor::Apply(doc::Document next) const {
  Edit([&next](Page& page) {
    doc::Commit(page.history, page.document);
    page.document = std::move(next);
    page.dirty = true;
  });
}

bool Editor::Resizing() const { return !resize.Get().node.empty(); }

void Editor::BeginResize(std::string id) const {
  const std::optional<huxerui::Size> measured = metrics ? metrics->Get(id) : std::nullopt;
  Edit([&](Page& page) {
    resize = ResizeGesture{
        .node = std::move(id),
        .width = measured ? measured->width : 0.0F,
        .height = measured ? measured->height : 0.0F,
        .before = page.document,
    };
  });
}

void Editor::ResizeBy(const std::string& id, float delta_width, float delta_height) const {
  const ResizeGesture gesture = resize.Get();
  if (gesture.node.empty() || gesture.node != id) {
    return;
  }
  doc::Document next = Document();
  if (delta_width != 0.0F) {
    const float width = std::max(kMinimumNodeSize, gesture.width + delta_width);
    static_cast<void>(doc::SetModifier(next, id, "width", static_cast<double>(std::lround(width))));
  }
  if (delta_height != 0.0F) {
    const float height = std::max(kMinimumNodeSize, gesture.height + delta_height);
    static_cast<void>(doc::SetModifier(next, id, "height", static_cast<double>(std::lround(height))));
  }
  Edit([&next](Page& page) {
    page.document = std::move(next);
    page.dirty = true;
  });
}

void Editor::EndResize() const {
  const ResizeGesture gesture = resize.Get();
  if (gesture.node.empty()) {
    return;
  }
  resize = ResizeGesture{};
  Edit([&gesture](Page& page) { doc::Commit(page.history, gesture.before); });
  status = "resized " + gesture.node;
}

void Editor::AddPage(std::string title, std::string path, doc::Document document) const {
  Page page;
  page.document = std::move(document);
  page.path = std::move(path);
  page.title = title.empty() ? NameFor(page.path, page.document) : std::move(title);
  pages.PushBack(std::move(page));
  active = pages.Size() - 1;
  drop_hint = std::string();
}

void Editor::NewPage() const {
  const std::string name = std::format("Page{}", pages.Size() + 1);
  // A new page has no file yet: its tab shows the name, and Save gives it a
  // file derived from that name.
  AddPage(name, std::string(), doc::StarterDocument(name));
  status = std::format("new page {}", name);
}

void Editor::Activate(std::size_t index) const {
  if (index >= pages.Size()) {
    return;
  }
  active = index;
  drop_hint = std::string();
}

void Editor::ClosePage(std::size_t index) const {
  const std::size_t count = pages.Size();
  if (index >= count) {
    return;
  }
  if (count == 1) {
    pages.Set(0, StarterPage("NewPage", "new_page.hui.json"));
    active = std::size_t{0};
    drop_hint = std::string();
    status = std::string("closed the last page; started a new document");
    return;
  }
  pages.Erase(index);
  // The active page keeps its identity across a close: removing an earlier page
  // shifts it down by one, removing the active page leaves the index pointing at
  // the page that took its place.
  const std::size_t current = active.Get();
  if (current >= pages.Size()) {
    active = pages.Size() - 1;
  } else if (index < current) {
    active = current - 1;
  }
  drop_hint = std::string();
  status = std::format("closed page {}", index + 1);
}

void Editor::RenameActive(std::string title) const {
  Edit([&title](Page& page) {
    page.title = std::move(title);
    page.dirty = true;
  });
}

void Editor::Undo() const {
  bool moved = false;
  Edit([&moved](Page& page) {
    const std::optional<doc::Document> previous = doc::Undo(page.history, page.document);
    if (!previous.has_value()) {
      return;
    }
    page.document = *previous;
    page.dirty = true;
    moved = true;
  });
  status = moved ? std::string("undo") : std::string("nothing to undo");
}

void Editor::Redo() const {
  bool moved = false;
  Edit([&moved](Page& page) {
    const std::optional<doc::Document> next = doc::Redo(page.history, page.document);
    if (!next.has_value()) {
      return;
    }
    page.document = *next;
    page.dirty = true;
    moved = true;
  });
  status = moved ? std::string("redo") : std::string("nothing to redo");
}

void Editor::Open(const std::string& file) const {
  if (file.empty()) {
    status = std::string("set a document path first");
    return;
  }
  const std::string path = Normalize(file);
  for (std::size_t index = 0; index < pages.Size(); ++index) {
    if (pages.At(index).path == path) {
      Activate(index);
      status = "switched to " + file;
      return;
    }
  }
  auto loaded = io::LoadDocument(file);
  if (!loaded.has_value()) {
    status = loaded.error();
    return;
  }
  AddPage(std::string(), path, std::move(*loaded));
  status = "opened " + file;
}

void Editor::Save() const {
  const Page& page = Current();
  // A page that has never been saved writes to a file named after its tab. The
  // path is reported in the status line, so where the file landed is never a
  // guess.
  SaveAs(page.path.empty() ? SnakeCase(page.title) + ".hui.json" : page.path);
}

void Editor::SaveAs(const std::string& file) const {
  if (file.empty()) {
    status = std::string("set a document path first");
    return;
  }
  const auto saved = io::SaveDocument(Document(), file);
  if (!saved.has_value()) {
    status = saved.error();
    return;
  }
  Edit([&file](Page& page) {
    page.path = Normalize(file);
    page.title = NameFor(page.path, page.document);
    page.dirty = false;
  });
  status = "saved " + file;
}

void Editor::Export() const {
  const Page& page = Current();
  const std::filesystem::path file =
      page.path.empty() ? std::filesystem::path(SnakeCase(page.title) + ".hui.json")
                        : std::filesystem::path(page.path);
  const std::filesystem::path cpp_path = std::filesystem::path(file).replace_extension(".cppm");
  const std::string code =
      codegen::GenerateCpp(Document(),
                           {.style = codegen::Style::Module,
                            .source_name = std::filesystem::path(file).filename().string()});
  const auto written = io::WriteTextFile(code, cpp_path);
  status = written.has_value() ? "exported " + cpp_path.string() : written.error();
}

}  // namespace hui::ui
