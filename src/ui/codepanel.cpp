// The code island: the C++ the document generates and the document's own JSON,
// behind two tabs, with the current one a button away from the clipboard.

#include "ui/ui.h"

#include "ui/icons.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

import hui.core.codegen;
import hui.core.docio;

using namespace huxerui;

namespace hui::ui {
namespace {

/// The text surface both tabs share. It scrolls on its own, so the island hands
/// it the whole body and does not wrap it again.
[[nodiscard]] View CodeText(std::string text, const theme::Palette& palette, std::string key) {
  return ScrollView(SelectionArea(Text(std::move(text)).With(FontSize(12.0F))).With(Padding(8.0F)))
      .With(Grow(1.0F), Background(palette.panel_bg))
      .Key(std::move(key));
}

}  // namespace

[[huxerui::composable]]
View CodeView(const Editor& ed) {
  auto tab = UseState(std::size_t{0});
  const std::shared_ptr<Clipboard> clipboard = UseApplication().Clipboard();

  const std::string code = codegen::GenerateCpp(ed.Document(), {.style = codegen::Style::Module});
  const std::string json_text = io::Serialize(ed.Document());

  View content = tab.Get() == 0 ? CodeText(code, ed.palette, "code-view")
                                : CodeText(json_text, ed.palette, "json-view");

  return Column{
      Row{
          Tabs({"C++", "JSON"}, tab.Get()).OnChanged([tab](std::size_t next) mutable { tab = next; }),
          Spacer(),
          icons::Action(icons::Copy(), "Copy to clipboard")
              .OnClick([clipboard, code, json_text, tab, ed] {
                if (clipboard == nullptr || !clipboard->IsAvailable()) {
                  ed.SetStatus("clipboard unavailable");
                  return;
                }
                clipboard->WriteText(tab.Get() == 0 ? code : json_text);
                ed.SetStatus(tab.Get() == 0 ? "copied C++" : "copied JSON");
              }),
      }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Center)),
      std::move(content),
  }
      .With(Spacing(8.0F), Grow(1.0F))
      .Key("codepanel");
}

}  // namespace hui::ui
