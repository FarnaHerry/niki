// The designer's own palette, separate from the HuxerUI theme a designed page
// uses. Both are applied together: this one paints the designer chrome, the
// Material light/dark theme is provided so built-in controls follow the mode.
#pragma once

#include <huxerui/color.h>

namespace hui::theme {

/// Colours of the designer chrome. All values are 0xRRGGBBAA.
struct Palette final {
  huxerui::Color app_bg{huxerui::Color::Rgba32(0xF7F6FBFF)};       // window background
  huxerui::Color panel_bg{huxerui::Color::Rgba32(0xFFFFFFFF)};     // island surface
  huxerui::Color bar_bg{huxerui::Color::Rgba32(0xFFFFFFFF)};       // title bar / toolbar
  huxerui::Color card_bg{huxerui::Color::Rgba32(0xFFFFFFFF)};      // list rows and cards
  huxerui::Color card_border{huxerui::Color::Rgba32(0x00000022)};  // hairline outline
  huxerui::Color accent{huxerui::Color::Rgba32(0x2D6CDFFF)};       // selection / primary
  huxerui::Color accent_soft{huxerui::Color::Rgba32(0x2D6CDF33)};  // selected fill
  huxerui::Color danger{huxerui::Color::Rgba32(0xB3261EFF)};       // validation errors
  huxerui::Color warning{huxerui::Color::Rgba32(0x8A6D00FF)};      // validation warnings
  huxerui::Color device_bg{huxerui::Color::Rgba32(0xFFFFFFFF)};    // designed-page surface
};

[[nodiscard]] inline Palette LightPalette() { return Palette{}; }

[[nodiscard]] inline Palette DarkPalette() {
  Palette palette;
  palette.app_bg = huxerui::Color::Rgba32(0x16171BFF);
  palette.panel_bg = huxerui::Color::Rgba32(0x1D1F25FF);
  palette.bar_bg = huxerui::Color::Rgba32(0x1D1F25FF);
  palette.card_bg = huxerui::Color::Rgba32(0x262A33FF);
  palette.card_border = huxerui::Color::Rgba32(0xFFFFFF1F);
  palette.accent = huxerui::Color::Rgba32(0x6C9CFFFF);
  palette.accent_soft = huxerui::Color::Rgba32(0x6C9CFF40);
  palette.danger = huxerui::Color::Rgba32(0xFF6B6BFF);
  palette.warning = huxerui::Color::Rgba32(0xE0B341FF);
  palette.device_bg = huxerui::Color::Rgba32(0x2A2E37FF);
  return palette;
}

}  // namespace hui::theme
