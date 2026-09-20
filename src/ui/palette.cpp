#include "ui/ui.h"

#include <string>
#include <utility>
#include <vector>

using namespace huxerui;

namespace hui::ui {
namespace {

/// One palette card. Dragging it carries the same payload a canvas node would,
/// so a container's drop target places it without the palette knowing which
/// container it is. There is no click action: a component arrives by being
/// dropped somewhere, never by a click that guesses the destination.
[[nodiscard]] View Card(const Editor& ed, const catalog::ComponentDef& component) {
  const std::string type = component.type;
  const std::string glyph = component.glyph;
  return Row{
      Text(component.glyph, TextRole::Label).With(Frame{.width = 22.0F}),
      Text(component.type, TextRole::Label).With(Grow(1.0F)),
  }
      .With(Spacing(8.0F), Padding(6.0F), CrossAlign(CrossAxisAlignment::Center),
            Background(ed.palette.card_bg), Border{.color = ed.palette.card_border, .width = 1.0F},
            CornerRadius(6.0F))
      .With(DragSource(DropPayload{.move = false, .ref = type},
                       [type, glyph, card_bg = ed.palette.card_bg, accent = ed.palette.accent] {
                         return Row{
                             Text(glyph + " " + type, TextRole::Label),
                             Text("new component", TextRole::Label).With(Opacity(0.6F)),
                         }
                             .With(Spacing(8.0F), Padding(8.0F), Background(card_bg),
                                   Border{.color = accent, .width = 1.0F}, CornerRadius(6.0F));
                       }));
}

}  // namespace

View PaletteView(const Editor& ed) {
  std::vector<View> rows;
  std::string category;
  for (const auto& component : catalog::All()) {
    if (component.category != category) {
      category = component.category;
      rows.push_back(Text(category, TextRole::Label).With(Padding(4.0F), Opacity(0.6F)));
    }
    rows.push_back(Card(ed, component));
  }

  return Column{
      Text("Components", TextRole::Title),
      Text("drag a card onto a container in the canvas", TextRole::Label).With(Opacity(0.6F)),
      Column(std::move(rows)).With(Spacing(4.0F)),
  }
      .With(Spacing(8.0F))
      .Key("palette");
}

}  // namespace hui::ui
