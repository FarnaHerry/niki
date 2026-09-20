// The designer application shell: toolbar, palette | canvas | inspector, and
// the code panel. Holds the document state, its history, and the file path the
// document is bound to.

export module hui.ui.app;

import std;
import huxerui;
import hui.core.codegen;
import hui.core.doc;
import hui.core.docio;
import hui.ui.canvas;
import hui.ui.codepanel;
import hui.ui.inspector;
import hui.ui.palette;
import hui.ui.structure;

using namespace huxerui;

export namespace hui::app {

[[huxerui::composable]]
View Designer() {
  auto document = UseState(doc::StarterDocument("CounterPage"));
  auto history = UseState(doc::History{});
  auto selection = UseState(std::string(""));
  auto drop_hint = UseState(std::string(""));
  auto status = UseState(std::string("welcome — drag a component onto the Column"));
  auto path = UseState(TextEditingValue::FromText("counter_page.hui.json"));

  // Every document change flows through here: snapshot the outgoing version
  // into history, then publish the next one.
  const auto commit = [document, history](doc::Document next) {
    const doc::Document current = document.Get();
    history.Update([&current](doc::History& h) { doc::Commit(h, current); });
    document = std::move(next);
  };

  const canvas::Editor editor{
      .doc = document,
      .selection = selection,
      .drop_hint = drop_hint,
      .apply = commit,
  };

  const auto load_from = [document, history, path, selection, status](const std::string& file) {
    auto loaded = io::LoadDocument(file);
    if (!loaded) {
      status = loaded.error();
      return;
    }
    const doc::Document current = document.Get();
    history.Update([&current](doc::History& h) { doc::Commit(h, current); });
    path = TextEditingValue::FromText(file);
    selection = std::string("");
    document = std::move(*loaded);
    status = "loaded " + file;
  };

  const auto save_to = [document, path, status]() {
    const std::string& file = path.Get().text;
    if (file.empty()) {
      status = std::string("set a document path first");
      return;
    }
    auto saved = io::SaveDocument(document.Get(), file);
    status = saved.has_value() ? "saved " + file : saved.error();
  };

  return Column{
      // Toolbar.
      Row{
          Text("hui", TextRole::Title),
          Text("HuxerUI Designer", TextRole::Label).With(Opacity(0.6F)),
          TextField(path.Get())
              .Placeholder("document path (.hui.json)")
              .With(Grow(1.0F))
              .OnChanged([path](const TextEditingValue& next) { path = next; }),
          Button("New").OnClick([document, history, path, selection, status]() {
            const doc::Document current = document.Get();
            history.Update([&current](doc::History& h) { doc::Commit(h, current); });
            document = doc::StarterDocument("NewPage");
            selection = std::string("");
            path = TextEditingValue::FromText("new_page.hui.json");
            status = std::string("new document");
          }),
          Button("Open").OnClick([load_from, path]() { load_from(path.Get().text); }),
          Button("Save").OnClick([save_to]() { save_to(); }),
          Button("Export").OnClick([document, path, status]() {
            const std::string& file = path.Get().text;
            if (file.empty()) {
              status = std::string("set a document path first");
              return;
            }
            const std::filesystem::path cpp_path = std::filesystem::path(file).replace_extension(".cppm");
            const std::string code = codegen::GenerateCpp(
                document.Get(), {.style = codegen::Style::Module,
                                 .source_name = std::filesystem::path(file).filename().string()});
            auto written = io::WriteTextFile(code, cpp_path);
            status = written.has_value() ? "exported " + cpp_path.string() : written.error();
          }),
          Button("Undo").OnClick([document, history, status]() {
            const doc::Document current = document.Get();
            std::optional<doc::Document> previous;
            history.Update([&](doc::History& h) { previous = doc::Undo(h, current); });
            if (previous.has_value()) {
              document = std::move(*previous);
              status = std::string("undo");
            } else {
              status = std::string("nothing to undo");
            }
          }),
          Button("Redo").OnClick([document, history, status]() {
            const doc::Document current = document.Get();
            std::optional<doc::Document> next;
            history.Update([&](doc::History& h) { next = doc::Redo(h, current); });
            if (next.has_value()) {
              document = std::move(*next);
              status = std::string("redo");
            } else {
              status = std::string("nothing to redo");
            }
          }),
      }.With(Spacing(8.0F), Padding(12.0F), CrossAlign(CrossAxisAlignment::Center)),

      // Body.
      Row{
          // Left: palette over structure.
          ScrollView(Column{
              palette::PaletteView(editor),
              Divider(),
              structure::StructureView(editor),
          }
                         .With(Spacing(8.0F))
                         .Key("left-column"))
              .With(Frame{.width = 260.0F}),

          // Center: canvas over code.
          Column{
              canvas::CanvasView(editor),
              codepanel::CodePanelView(editor),
          }.With(Grow(1.0F), Spacing(4.0F)),

          // Right: inspector.
          ScrollView(inspector::InspectorView(editor)).With(Frame{.width = 320.0F}),
      }.With(Spacing(8.0F), Grow(1.0F)),

      // Status line.
      Row{
          Text(status.Get(), TextRole::Label),
          Spacer(),
          Text("hui 0.1.0 · `hui mcp` serves this engine to AI agents", TextRole::Label).With(Opacity(0.5F)),
      }.With(Spacing(12.0F), Padding(8.0F), CrossAlign(CrossAxisAlignment::Center)),
  }
      .Key("designer");
}

View App() {
  return MaterialTheme{Designer()};
}

const Application application{
    App,
    {
        .window = {
            .title = "hui — HuxerUI Designer",
            .initial_size = {1440.0F, 940.0F},
            .minimum_size = Size{1080.0F, 720.0F},
        },
    },
};

}  // namespace hui::app
