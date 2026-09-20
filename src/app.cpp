// src/app.cpp — the designer shell.
//
// Custom title bar (the framework draws no caption controls; this window owns
// them), a file/edit toolbar, and the content islands. The panels are ported
// into their islands one at a time, so each island shows a placeholder until
// its turn comes.

#include <huxerui/huxerui.h>

#include <format>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ui/icons.h"
#include "ui/theme.h"
#include "ui/ui.h"

using namespace huxerui;

namespace {

/// One floating panel: rounded surface, hairline border, header, then body.
/// The body stretches, and scrolls when `scroll` is set — a panel taller than
/// the window scrolls instead of pushing the row it sits in. A panel that
/// arranges its own scrolling (the canvas pins a status line under its viewport)
/// passes `scroll = false`.
[[nodiscard]] View Island(std::string title, std::string hint, View body,
                          const hui::theme::Palette& palette, bool scroll = true) {
  View content = scroll ? View(ScrollView(std::move(body))) : std::move(body);
  return Column{
      Row{
          Text(std::move(title), TextRole::Label),
          Spacer(),
          Text(std::move(hint), TextRole::Label).With(Opacity(0.45F)),
      }.With(CrossAlign(CrossAxisAlignment::Center)),
      std::move(content).With(Grow(1.0F)),
  }
      .With(Padding(12.0F), Spacing(10.0F), CrossAlign(CrossAxisAlignment::Stretch),
            Background(palette.panel_bg), Border{.color = palette.card_border, .width = 1.0F},
            CornerRadius(12.0F));
}

[[huxerui::composable]]
View Designer() {
  auto dark = UseState(false);
  std::vector<hui::ui::Page> initial_pages;
  initial_pages.push_back(hui::ui::StarterPage("CounterPage", "counter_page.hui.json"));
  auto pages = UseStateList(std::move(initial_pages));
  auto active = UseState(std::size_t{0});
  auto status = UseState(std::string("welcome — click a component in the palette"));
  auto drop_hint = UseState(std::string(""));
  auto resize = UseState(hui::ui::ResizeGesture{});
  // Held in a State so the table survives recomposition: the canvas writes the
  // measured size of every node into it, and a resize gesture reads it back.
  auto metrics = UseState(std::make_shared<hui::ui::NodeMetrics>());

  const hui::theme::Palette palette = dark.Get() ? hui::theme::DarkPalette() : hui::theme::LightPalette();
  const hui::ui::Editor ed{
      .pages = pages,
      .active = active,
      .status = status,
      .drop_hint = drop_hint,
      .resize = resize,
      .metrics = metrics.Get(),
      .palette = palette,
  };
  const WindowHandle window = UseWindow();

  // A tab is a file, so there is no document-path field to keep in step with
  // the tabs: Open asks the desktop for one and Save uses the page's own.
  auto tasks = UseTaskScope();
  const auto picker = UseService<FilePicker>();

  View body = Column{
      // Title bar. caption_controls = Application keeps the framework from
      // adding a second set of window buttons or reserving a caption area.
      WindowTitleBar{
          Text("hui", TextRole::Title),
          Text(ed.Current().title, TextRole::Label).With(Opacity(0.6F)),
          Spacer(),
          hui::icons::Action(hui::icons::Theme(), dark.Get() ? "Use light theme" : "Use dark theme")
              .OnClick([dark] { dark = !dark.Get(); }),
          hui::icons::Action(hui::icons::Minimize(), "Minimize").OnClick([window] { window.Minimize(); }),
          hui::icons::Action(hui::icons::Maximize(), "Maximize or restore")
              .OnClick([window] { window.ToggleMaximize(); }),
          hui::icons::Action(hui::icons::Close(), "Close").OnClick([window] { window.Close(); }),
      }.With(Spacing(6.0F), Padding(EdgeInsets{.top = 4.0F, .right = 6.0F, .bottom = 4.0F, .left = 10.0F}),
             Background(palette.bar_bg)),

      // One tab per page, and a tab is the page's file. Everything below this
      // strip shows the active page.
      hui::ui::TabStripView(ed),

      // File and edit toolbar.
      Row{
          hui::icons::Action(hui::icons::New(), "New page").OnClick([ed] { ed.NewPage(); }),
          hui::icons::Action(hui::icons::Open(), "Open a document in its own tab").OnClick([ed, picker, tasks] {
            if (!picker || !picker->CanOpenFiles()) {
              ed.SetStatus("this desktop offers no file dialog; run `hui help` for the CLI");
              return;
            }
            tasks.Launch([ed, picker]() -> Task<void> {
              const std::optional<FileReference> chosen =
                  co_await picker->OpenFileAsync(FilePickerFilter{.name = "hui document",
                                                                  .extensions = {"json"}});
              if (!chosen.has_value()) {
                co_return;
              }
              const std::optional<File> file = chosen->AsFile();
              if (!file.has_value()) {
                ed.SetStatus("the chosen file has no local path");
                co_return;
              }
              ed.Open(file->Path());
            });
          }),
          hui::icons::Action(hui::icons::Save(), "Save").OnClick([ed] { ed.Save(); }),
          hui::icons::Action(hui::icons::Export(), "Export C++ module").OnClick([ed] { ed.Export(); }),
          Divider(Axis::Vertical).With(Frame{.height = 20.0F}),
          hui::icons::Action(hui::icons::Undo(), "Undo").OnClick([ed] { ed.Undo(); }),
          hui::icons::Action(hui::icons::Redo(), "Redo").OnClick([ed] { ed.Redo(); }),
      }.With(Spacing(6.0F), Padding(EdgeInsets{.top = 4.0F, .right = 10.0F, .bottom = 4.0F, .left = 10.0F}),
             CrossAlign(CrossAxisAlignment::Center), Background(palette.bar_bg)),

      // Content islands: components+structure | canvas | inspector, over the code island.
      Column{
          Row{
              Island("Components", "left · 260",
                     Column{
                         hui::ui::PaletteView(ed),
                         Divider(),
                         hui::ui::StructureView(ed),
                     }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Stretch)),
                     palette)
                  .With(Frame{.width = 260.0F}),
              Island("Canvas", "centre · grow", hui::ui::CanvasView(ed), palette, /*scroll=*/false)
                  .With(Grow(1.0F)),
              Island("Inspector", "right · 320", hui::ui::InspectorView(ed), palette)
                  .With(Frame{.width = 320.0F}),
          }.With(Spacing(10.0F), Grow(1.0F), CrossAlign(CrossAxisAlignment::Stretch)),
          Island("Code", "module · json", hui::ui::CodeView(ed), palette, /*scroll=*/false)
              .With(Frame{.height = 200.0F}),
      }.With(Spacing(10.0F), Grow(1.0F),
             Padding(EdgeInsets{.top = 10.0F, .right = 10.0F, .bottom = 10.0F, .left = 10.0F})),

      // Status line.
      Row{
          Text(ed.Status(), TextRole::Label),
          Spacer(),
          Text(std::format("{} node(s) · page {}/{} · {} · {}", hui::doc::AllIds(ed.Document().root).size(),
                           ed.ActiveIndex() + 1, pages.Size(), ed.Current().title,
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
