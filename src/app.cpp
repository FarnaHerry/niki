// src/app.cpp — the designer shell.
//
// Custom title bar (the framework draws no caption controls; this window owns
// them), a file/edit toolbar, and the content islands. The panels are ported
// into their islands one at a time, so each island shows a placeholder until
// its turn comes.

#include <huxerui/huxerui.h>

#include <format>
#include <string>
#include <utility>

#include "ui/icons.h"
#include "ui/theme.h"

using namespace huxerui;

namespace {

/// One floating panel: rounded surface, hairline border, header, then body.
[[nodiscard]] View Island(std::string title, std::string hint, View body,
                          const hui::theme::Palette& palette) {
  return Column{
      Row{
          Text(std::move(title), TextRole::Label),
          Spacer(),
          Text(std::move(hint), TextRole::Label).With(Opacity(0.45F)),
      }.With(CrossAlign(CrossAxisAlignment::Center)),
      std::move(body),
  }
      .With(Padding(12.0F), Spacing(10.0F), Background(palette.panel_bg),
            Border{.color = palette.card_border, .width = 1.0F}, CornerRadius(12.0F));
}

[[nodiscard]] View RegionPlaceholder(std::string text) {
  return Text(std::move(text), TextRole::Label).With(Opacity(0.4F), Padding(4.0F));
}

[[huxerui::composable]]
View Designer() {
  auto dark = UseState(false);
  auto path = UseState(TextEditingValue::FromText("counter_page.hui.json"));
  auto status = UseState(std::string("welcome — the panels are ported island by island"));

  const hui::theme::Palette palette = dark.Get() ? hui::theme::DarkPalette() : hui::theme::LightPalette();
  const WindowHandle window = UseWindow();

  View body = Column{
      // Title bar. caption_controls = Application keeps the framework from
      // adding a second set of window buttons or reserving a caption area.
      WindowTitleBar{
          Text("hui", TextRole::Title),
          Text("HuxerUI Designer", TextRole::Label).With(Opacity(0.6F)),
          Spacer(),
          hui::icons::Action(hui::icons::Theme(), dark.Get() ? "Use light theme" : "Use dark theme")
              .OnClick([dark] { dark = !dark.Get(); }),
          hui::icons::Action(hui::icons::Minimize(), "Minimize").OnClick([window] { window.Minimize(); }),
          hui::icons::Action(hui::icons::Maximize(), "Maximize or restore")
              .OnClick([window] { window.ToggleMaximize(); }),
          hui::icons::Action(hui::icons::Close(), "Close").OnClick([window] { window.Close(); }),
      }.With(Spacing(6.0F), Padding(EdgeInsets{.top = 4.0F, .right = 6.0F, .bottom = 4.0F, .left = 10.0F}),
             Background(palette.bar_bg)),

      // File and edit toolbar.
      Row{
          TextField(path.Get())
              .Placeholder("document path (.hui.json)")
              .OnChanged([path](const TextEditingValue& next) { path = next; })
              .With(Grow(1.0F)),
          hui::icons::Action(hui::icons::New(), "New document").OnClick([status] {
            status = std::string("new document");
          }),
          hui::icons::Action(hui::icons::Open(), "Open document").OnClick([status] {
            status = std::string("open document");
          }),
          hui::icons::Action(hui::icons::Save(), "Save document").OnClick([status] {
            status = std::string("save document");
          }),
          hui::icons::Action(hui::icons::Export(), "Export C++ module").OnClick([status] {
            status = std::string("export C++ module");
          }),
          Divider(Axis::Vertical).With(Frame{.height = 20.0F}),
          hui::icons::Action(hui::icons::Undo(), "Undo").OnClick([status] { status = std::string("undo"); }),
          hui::icons::Action(hui::icons::Redo(), "Redo").OnClick([status] { status = std::string("redo"); }),
      }.With(Spacing(6.0F), Padding(EdgeInsets{.top = 4.0F, .right = 10.0F, .bottom = 4.0F, .left = 10.0F}),
             CrossAlign(CrossAxisAlignment::Center), Background(palette.bar_bg)),

      // Content islands: components+structure | canvas | inspector, over the code island.
      Column{
          Row{
              Island("Components", "left · 260", RegionPlaceholder("palette + structure region"), palette)
                  .With(Frame{.width = 260.0F}),
              Island("Canvas", "centre · grow", RegionPlaceholder("canvas region"), palette).With(Grow(1.0F)),
              Island("Inspector", "right · 320", RegionPlaceholder("inspector region"), palette)
                  .With(Frame{.width = 320.0F}),
          }.With(Spacing(10.0F), Grow(1.0F), CrossAlign(CrossAxisAlignment::Stretch)),
          Island("Code", "module · json", RegionPlaceholder("generated code region"), palette)
              .With(Frame{.height = 200.0F}),
      }.With(Spacing(10.0F), Grow(1.0F),
             Padding(EdgeInsets{.top = 10.0F, .right = 10.0F, .bottom = 10.0F, .left = 10.0F})),

      // Status line.
      Row{
          Text(status.Get(), TextRole::Label),
          Spacer(),
          Text(std::format("hui 0.1.0 · {}", dark.Get() ? "dark" : "light"), TextRole::Label).With(Opacity(0.5F)),
      }.With(Spacing(12.0F), Padding(EdgeInsets{.top = 6.0F, .right = 12.0F, .bottom = 8.0F, .left = 12.0F}),
             CrossAlign(CrossAxisAlignment::Center)),
  }.With(Background(palette.app_bg), CrossAlign(CrossAxisAlignment::Stretch));

  if (dark.Get()) {
    return MaterialDarkTheme(std::move(body));
  }
  return MaterialTheme(std::move(body));
}

}  // namespace

View App() { return Designer(); }

const Application application{
    App,
    {
        .window =
            {
                .title = "hui — HuxerUI Designer",
                .initial_size = {1440.0F, 940.0F},
                .minimum_size = Size{1080.0F, 720.0F},
                .content_mode = WindowContentMode::EdgeToEdge,
                .chrome_mode = WindowChromeMode::Custom,
                .title_bar_height = 44.0F,
                // The title bar draws its own minimize / maximize / close.
                .caption_controls = WindowCaptionControls::Application,
            },
    },
};
