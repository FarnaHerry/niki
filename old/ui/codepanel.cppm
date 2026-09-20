// The bottom panel: the generated C++ and the raw document JSON, side by side
// as tabs, with clipboard copy for pasting straight into an application.

export module hui.ui.codepanel;

import std;
import huxerui;
import hui.core.codegen;
import hui.core.doc;
import hui.core.docio;
import hui.ui.canvas;

using namespace huxerui;

export namespace hui::codepanel {

namespace detail {

[[nodiscard]] inline View CodeText(std::string code) {
  return ScrollView(SelectionArea(Text(std::move(code)).With(FontSize(12.0F))).With(Padding(8.0F)))
      .With(Grow(1.0F));
}

}  // namespace detail

[[huxerui::composable]]
inline View CodePanelView(const canvas::Editor& editor) {
  using namespace detail;

  const doc::Document& document = editor.doc.Get();
  auto tab = UseState(static_cast<std::size_t>(0));
  // Read at composition time; the handle outlives the declaration.
  const std::shared_ptr<Clipboard> clipboard = UseApplication().Clipboard();

  const std::string code = codegen::GenerateCpp(document, {.style = codegen::Style::Module});
  const std::string json_text = io::Serialize(document);

  View content;
  if (tab.Get() == 0) {
    content = CodeText(code).Key("code-view");
  } else {
    content = CodeText(json_text).Key("json-view");
  }

  return Column{
      Row{
          Tabs({"C++", "JSON"}, tab.Get()).OnChanged([tab](std::size_t next) mutable { tab = next; }),
          Spacer(),
          Button("Copy").OnClick([clipboard, code, json_text, tab] {
            if (clipboard != nullptr && clipboard->IsAvailable()) {
              clipboard->WriteText(tab.Get() == 0 ? code : json_text);
            }
          }),
      }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Center)),
      std::move(content),
  }
      .With(Spacing(8.0F), Padding(8.0F), Frame{.height = 240.0F}, Background(Color::Rgba32(0xFFFFFFFF)))
      .Key("codepanel");
}

}  // namespace hui::codepanel
