// The designer application shell — rebuilt panel by panel.
//
// Step 9: island layout. Every content block is a floating card (rounded
// surface, subtle border, header) so the regions are visibly separate before
// any of them is filled in. The title bar stays full-bleed because it is window
// chrome, and the file/edit toolbar sits directly under it.
//
// Deliberately no ScrollView yet: a ScrollView that is a direct child of a Row
// measures to nothing here and drops out of the scene. Panels that need
// scrolling get a bounded height from an explicit Frame instead.

export module hui.ui.app;

import std;
import huxerui;
import hui.core.doc;
import hui.ui.editor;
import hui.ui.icons;
import hui.ui.theme;

using namespace huxerui;

export namespace hui::app {

/// One floating panel: rounded surface, hairline border, header, then body.
[[nodiscard]] inline View Island(std::string title, std::string hint, View body, const theme::Palette& palette) {
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

/// Stands in for a region until its panel is bound into the island.
[[nodiscard]] inline View RegionPlaceholder(std::string text) {
  return Text(std::move(text), TextRole::Label).With(Opacity(0.4F), Padding(4.0F));
}

[[huxerui::composable]]
View Designer() {
  auto document = UseState(doc::StarterDocument("CounterPage"));
  auto selection = UseState(std::string(""));
  auto status = UseState(std::string("welcome — click a component in the palette"));
  auto history = UseState(doc::History{});
  auto drop_hint = UseState(std::string(""));
  auto dark = UseState(false);
  auto path = UseState(TextEditingValue::FromText("counter_page.hui.json"));

  const theme::Palette palette = dark.Get() ? theme::DarkPalette() : theme::LightPalette();
  const editor::Editor ed{
      .document = document,
      .selection = selection,
      .status = status,
      .history = history,
      .drop_hint = drop_hint,
      .palette = palette,
  };

  const WindowHandle window = UseWindow();

  View body = Column{
      // Custom title bar. caption_controls = Application above means the
      // framework adds no second set of controls and reserves no caption area,
      // so this row spans the full width. WindowTitleBar still marks itself as
      // a native drag region.
      WindowTitleBar{
          Text("hui", TextRole::Title),
          Text(document.Get().name, TextRole::Label).With(Opacity(0.6F)),
          Spacer(),
          icons::Action(icons::Theme(), dark.Get() ? "Use light theme" : "Use dark theme")
              .OnClick([dark] { dark = !dark.Get(); }),
          icons::Action(icons::Minimize(), "Minimize").OnClick([window] { window.Minimize(); }),
          icons::Action(icons::Maximize(), "Maximize or restore").OnClick([window] { window.ToggleMaximize(); }),
          icons::Action(icons::Close(), "Close").OnClick([window] { window.Close(); }),
      }.With(Spacing(6.0F), Padding(EdgeInsets{.top = 4.0F, .right = 6.0F, .bottom = 4.0F, .left = 10.0F}),
             Background(palette.bar_bg)),

      // File and edit toolbar.
      Row{
          TextField(path.Get())
              .Placeholder("document path (.hui.json)")
              .OnChanged([path](const TextEditingValue& next) { path = next; })
              .With(Grow(1.0F)),
          icons::Action(icons::New(), "New document").OnClick([ed, path] {
            ed.Replace(doc::StarterDocument("NewPage"), "new document");
            path = TextEditingValue::FromText("new_page.hui.json");
          }),
          icons::Action(icons::Open(), "Open document").OnClick([ed, path] { ed.Open(path.Get().text); }),
          icons::Action(icons::Save(), "Save document").OnClick([ed, path] { ed.Save(path.Get().text); }),
          icons::Action(icons::Export(), "Export C++ module").OnClick([ed, path] { ed.Export(path.Get().text); }),
          Divider(Axis::Vertical).With(Frame{.height = 20.0F}),
          icons::Action(icons::Undo(), "Undo").OnClick([ed] { ed.Undo(); }),
          icons::Action(icons::Redo(), "Redo").OnClick([ed] { ed.Redo(); }),
      }.With(Spacing(6.0F), Padding(EdgeInsets{.top = 4.0F, .right = 10.0F, .bottom = 4.0F, .left = 10.0F}),
             CrossAlign(CrossAxisAlignment::Center), Background(palette.bar_bg)),

      // Content islands: palette+structure | canvas | inspector, over the code island.
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
      }.With(Spacing(10.0F), Grow(1.0F), Padding(EdgeInsets{.top = 10.0F, .right = 10.0F, .bottom = 10.0F, .left = 10.0F})),

      // Status line.
      Row{
          Text(status.Get(), TextRole::Label),
          Spacer(),
          Text(std::format("{} node(s) · {} · {}", doc::AllIds(document.Get().root).size(), document.Get().name,
                           dark.Get() ? "dark" : "light"),
               TextRole::Label)
              .With(Opacity(0.5F)),
      }.With(Spacing(12.0F), Padding(EdgeInsets{.top = 6.0F, .right = 12.0F, .bottom = 8.0F, .left = 12.0F}),
             CrossAlign(CrossAxisAlignment::Center)),
  }.With(Background(palette.app_bg), CrossAlign(CrossAxisAlignment::Stretch));

  if (dark.Get()) {
    return MaterialDarkTheme(std::move(body));
  }
  return MaterialTheme(std::move(body));
}

View App() {
  return Designer();
}

const Application application{
    App,
    {
        .window = {
            .title = "hui — HuxerUI Designer",
            .initial_size = {1440.0F, 940.0F},
            .minimum_size = Size{1080.0F, 720.0F},
            .content_mode = WindowContentMode::EdgeToEdge,
            .chrome_mode = WindowChromeMode::Custom,
            .title_bar_height = 44.0F,
            // The title bar below draws its own minimize / maximize / close
            // controls, so the framework must not add a second set nor reserve
            // a caption area for them.
            .caption_controls = WindowCaptionControls::Application,
        },
    },
};

}  // namespace hui::app
