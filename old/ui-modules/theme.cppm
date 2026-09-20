// The designer's own palette, separate from the HuxerUI theme a designed page
// uses. Both are applied together: this one paints the designer chrome, the
// Material light/dark theme is provided so built-in controls follow the mode.

export module hui.ui.theme;

import std;
import huxerui;

using namespace huxerui;

export namespace hui::theme {

/// Colours of the designer chrome. All values are 0xRRGGBBAA.
struct Palette final {
  Color app_bg{Color::Rgba32(0xF7F6FBFF)};      // window background
  Color panel_bg{Color::Rgba32(0xFFFFFFFF)};    // side panels
  Color bar_bg{Color::Rgba32(0xFFFFFFFF)};      // custom title bar
  Color card_bg{Color::Rgba32(0xFFFFFFFF)};     // palette card
  Color card_border{Color::Rgba32(0x00000022)}; // card outline
  Color accent{Color::Rgba32(0x2D6CDFFF)};      // selection / primary
  Color accent_soft{Color::Rgba32(0x2D6CDF33)}; // selected row fill
  Color danger{Color::Rgba32(0xB3261EFF)};      // validation errors
  Color warning{Color::Rgba32(0x8A6D00FF)};     // validation warnings
  Color device_bg{Color::Rgba32(0xFFFFFFFF)};   // designed-page surface
};

[[nodiscard]] inline Palette LightPalette() { return Palette{}; }

[[nodiscard]] inline Palette DarkPalette() {
  Palette palette;
  palette.app_bg = Color::Rgba32(0x16171BFF);
  palette.panel_bg = Color::Rgba32(0x1D1F25FF);
  palette.bar_bg = Color::Rgba32(0x1D1F25FF);
  palette.card_bg = Color::Rgba32(0x262A33FF);
  palette.card_border = Color::Rgba32(0xFFFFFF1F);
  palette.accent = Color::Rgba32(0x6C9CFFFF);
  palette.accent_soft = Color::Rgba32(0x6C9CFF40);
  palette.danger = Color::Rgba32(0xFF6B6BFF);
  palette.warning = Color::Rgba32(0xE0B341FF);
  palette.device_bg = Color::Rgba32(0x2A2E37FF);
  return palette;
}

}  // namespace hui::theme
