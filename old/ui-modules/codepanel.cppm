// The code panel: the generated C++ and the raw document JSON behind two tabs,
// with clipboard copy for pasting straight into an application.

export module hui.ui.codepanel;

import std;
import huxerui;
import hui.core.codegen;
import hui.core.doc;
import hui.core.docio;
import hui.ui.editor;
import hui.ui.icons;
import hui.ui.theme;

using namespace huxerui;

export namespace hui::codepanel {

namespace detail {

[[nodiscard]] inline View CodeText(std::string text, const theme::Palette& palette) {
  return ScrollView(SelectionArea(Text(std::move(text)).With(FontSize(12.0F))).With(Padding(8.0F)))
      .With(Grow(1.0F), Background(palette.panel_bg));
}

}  // namespace detail

[[huxerui::composable]]
View CodePanelView(const editor::Editor& ed) {
  using namespace detail;

  const doc::Document& document = ed.document.Get();
  auto tab = UseState(static_cast<std::size_t>(0));
  const std::shared_ptr<Clipboard> clipboard = UseApplication().Clipboard();

  const std::string code = codegen::GenerateCpp(document, {.style = codegen::Style::Module});
  const std::string json_text = io::Serialize(document);

  View content = tab.Get() == 0 ? CodeText(code, ed.palette).Key("code-view")
                                : CodeText(json_text, ed.palette).Key("json-view");

  return Column{
      Row{
          Tabs({"C++", "JSON"}, tab.Get()).OnChanged([tab](std::size_t next) mutable { tab = next; }),
          Spacer(),
          icons::Action(icons::Copy(), "Copy to clipboard").OnClick([clipboard, code, json_text, tab, ed] {
            if (clipboard == nullptr || !clipboard->IsAvailable()) {
              ed.status = "clipboard unavailable";
              return;
            }
            clipboard->WriteText(tab.Get() == 0 ? code : json_text);
            ed.status = tab.Get() == 0 ? "copied C++" : "copied JSON";
          }),
      }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Center)),
      std::move(content),
  }
      .With(Spacing(8.0F), Padding(8.0F), Frame{.height = 240.0F}, Background(ed.palette.panel_bg))
      .Key("codepanel");
}

}  // namespace hui::codepanel
