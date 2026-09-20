// The tab strip: one tab per page, each with a close button, and the add button.
//
// A tab is the page's identity on screen — its title, whether it has unsaved
// edits, and the click that makes it the page every panel is looking at. The
// close button is its own click target, and the innermost binding wins, so
// closing a tab never also activates it.

#include "ui/ui.h"

#include "ui/icons.h"

#include <huxerui/presentation.h>

#include <cstddef>
#include <format>
#include <string>
#include <utility>
#include <vector>

using namespace huxerui;

namespace hui::ui {
namespace {

[[nodiscard]] View Tab(const Editor& ed, std::size_t index, const std::function<void()>& on_tab_changed) {
  const Page& page = ed.pages.At(index);
  const bool active = ed.ActiveIndex() == index;
  const theme::Palette& palette = ed.palette;

  View close = icons::Action(icons::Close(), "Close page")
                   .With(Frame{.width = 20.0F, .height = 20.0F})
                   .OnClick([ed, index, on_tab_changed] {
                     ed.ClosePage(index);
                     on_tab_changed();
                   });

  return Column{
      Row{
          // A space keeps the title from shifting as the dirty mark appears.
          Text(page.dirty ? "•" : " ", TextRole::Label).With(Opacity(0.7F)),
          Text(page.title, TextRole::Label).With(Opacity(active ? 1.0F : 0.62F)),
          close,
      }
          .With(Spacing(4.0F), CrossAlign(CrossAxisAlignment::Center)),
  }
      .With(Padding(EdgeInsets{.top = 5.0F, .right = 6.0F, .bottom = 5.0F, .left = 12.0F}),
            Background(active ? palette.accent_soft : Color::Rgba32(0x00000000)),
            Border{.color = active ? palette.accent : Color::Rgba32(0x00000000), .width = 1.0F},
            CornerRadius(6.0F))
      .With(Tooltip(page.path.empty() ? std::string("never saved") : page.path))
      .OnClick([ed, index, on_tab_changed] {
        ed.Activate(index);
        on_tab_changed();
      });
}

}  // namespace

View TabStripView(const Editor& ed, std::function<void()> on_tab_changed) {
  std::vector<View> tabs;
  tabs.reserve(ed.pages.Size() + 1);
  for (std::size_t index = 0; index < ed.pages.Size(); ++index) {
    tabs.push_back(Tab(ed, index, on_tab_changed).Key(std::format("tab:{}", index)));
  }
  tabs.push_back(icons::Action(icons::New(), "New page").OnClick([ed, on_tab_changed] {
    ed.NewPage();
    on_tab_changed();
  }));

  return Row(std::move(tabs))
      .With(Spacing(4.0F), Padding(EdgeInsets{.top = 6.0F, .right = 10.0F, .bottom = 0.0F, .left = 10.0F}),
            CrossAlign(CrossAxisAlignment::Center), Background(ed.palette.bar_bg))
      .Key("tabs");
}

}  // namespace hui::ui
