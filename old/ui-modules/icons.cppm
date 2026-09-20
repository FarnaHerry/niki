// The designer's icon set, drawn as recorded vector assets rather than loaded
// from image files: no resource packaging, no build-system dependency, and the
// theme tints them because IconButton's style owns the icon colour.
//
// Every icon is recorded on a 24x24 grid with a 1.7px round stroke, the shape
// convention Material icons use.

export module hui.ui.icons;

import std;
import huxerui;

using namespace huxerui;

export namespace hui::icons {

namespace detail {

inline constexpr float kGrid = 24.0F;
inline constexpr StrokeStyle kStroke{.width = 1.7F, .cap = StrokeCap::Round, .join = StrokeJoin::Round};

[[nodiscard]] inline VectorAsset Make(const std::function<void(VectorBuilder&)>& draw) {
  return VectorAsset::Create(Size{kGrid, kGrid}, draw);
}

[[nodiscard]] inline Path Line(Point from, Point to) { return Path().MoveTo(from).LineTo(to); }

[[nodiscard]] inline Path Polyline(std::initializer_list<Point> points) {
  Path path;
  bool first = true;
  for (const Point point : points) {
    if (first) {
      path.MoveTo(point);
      first = false;
    } else {
      path.LineTo(point);
    }
  }
  return path;
}

[[nodiscard]] inline Path Circle(Point center, float radius) {
  Path path;
  path.MoveTo({center.x - radius, center.y})
      .ArcTo({radius, radius}, 0.0F, ArcSize::Small, ArcDirection::Clockwise, {center.x + radius, center.y})
      .ArcTo({radius, radius}, 0.0F, ArcSize::Small, ArcDirection::Clockwise, {center.x - radius, center.y})
      .Close();
  return path;
}

[[nodiscard]] inline Path HalfPlane() {
  return Polyline({{0.0F, 0.0F}, {kGrid * 0.5F, 0.0F}, {kGrid * 0.5F, kGrid}, {0.0F, kGrid}}).Close();
}

}  // namespace detail

[[nodiscard]] inline VectorAsset New() {
  return detail::Make([](VectorBuilder& builder) {
    builder.StrokePath(detail::Line({12.0F, 5.0F}, {12.0F, 19.0F}), Color::White(), detail::kStroke);
    builder.StrokePath(detail::Line({5.0F, 12.0F}, {19.0F, 12.0F}), Color::White(), detail::kStroke);
  });
}

[[nodiscard]] inline VectorAsset Open() {
  return detail::Make([](VectorBuilder& builder) {
    builder.StrokePath(
        detail::Polyline({{3.0F, 19.0F}, {3.0F, 6.5F}, {9.5F, 6.5F}, {12.0F, 9.5F}, {21.0F, 9.5F}, {21.0F, 19.0F},
                          {3.0F, 19.0F}}),
        Color::White(), detail::kStroke);
  });
}

[[nodiscard]] inline VectorAsset Save() {
  return detail::Make([](VectorBuilder& builder) {
    builder.StrokePath(Path::RoundedRect({4.0F, 4.0F, 16.0F, 16.0F}, CornerRadii{2.5F}), Color::White(),
                       detail::kStroke);
    builder.StrokePath(detail::Polyline({{8.5F, 4.5F}, {8.5F, 10.5F}, {15.5F, 10.5F}, {15.5F, 4.5F}}),
                       Color::White(), detail::kStroke);
    builder.StrokePath(Path::RoundedRect({7.5F, 13.5F, 9.0F, 6.5F}, CornerRadii{1.5F}), Color::White(),
                       detail::kStroke);
  });
}

[[nodiscard]] inline VectorAsset Export() {
  return detail::Make([](VectorBuilder& builder) {
    builder.StrokePath(
        detail::Polyline({{11.0F, 5.0F}, {5.0F, 5.0F}, {5.0F, 19.0F}, {19.0F, 19.0F}, {19.0F, 13.0F}}),
        Color::White(), detail::kStroke);
    builder.StrokePath(detail::Line({12.5F, 11.5F}, {19.5F, 4.5F}), Color::White(), detail::kStroke);
    builder.StrokePath(detail::Polyline({{14.0F, 4.5F}, {19.5F, 4.5F}, {19.5F, 10.0F}}), Color::White(),
                       detail::kStroke);
  });
}

[[nodiscard]] inline VectorAsset Undo() {
  return detail::Make([](VectorBuilder& builder) {
    builder.StrokePath(
        Path().MoveTo({7.0F, 11.0F}).QuadraticTo({12.0F, 5.0F}, {17.0F, 11.0F}).LineTo({17.0F, 17.5F}),
        Color::White(), detail::kStroke);
    builder.StrokePath(detail::Polyline({{4.0F, 8.0F}, {7.0F, 11.0F}, {10.0F, 8.0F}}), Color::White(),
                       detail::kStroke);
  });
}

[[nodiscard]] inline VectorAsset Redo() {
  return detail::Make([](VectorBuilder& builder) {
    builder.StrokePath(
        Path().MoveTo({17.0F, 11.0F}).QuadraticTo({12.0F, 5.0F}, {7.0F, 11.0F}).LineTo({7.0F, 17.5F}),
        Color::White(), detail::kStroke);
    builder.StrokePath(detail::Polyline({{20.0F, 8.0F}, {17.0F, 11.0F}, {14.0F, 8.0F}}), Color::White(),
                       detail::kStroke);
  });
}

[[nodiscard]] inline VectorAsset Copy() {
  return detail::Make([](VectorBuilder& builder) {
    builder.StrokePath(Path::RoundedRect({3.5F, 3.5F, 12.0F, 12.0F}, CornerRadii{2.0F}), Color::White(),
                       detail::kStroke);
    builder.StrokePath(Path::RoundedRect({8.5F, 8.5F, 12.0F, 12.0F}, CornerRadii{2.0F}), Color::White(),
                       detail::kStroke);
  });
}

[[nodiscard]] inline VectorAsset Delete() {
  return detail::Make([](VectorBuilder& builder) {
    builder.StrokePath(detail::Line({4.0F, 7.0F}, {20.0F, 7.0F}), Color::White(), detail::kStroke);
    builder.StrokePath(detail::Polyline({{9.5F, 7.0F}, {9.5F, 4.5F}, {14.5F, 4.5F}, {14.5F, 7.0F}}),
                       Color::White(), detail::kStroke);
    builder.StrokePath(detail::Polyline({{6.5F, 7.0F}, {7.6F, 19.5F}, {16.4F, 19.5F}, {17.5F, 7.0F}}),
                       Color::White(), detail::kStroke);
  });
}

[[nodiscard]] inline VectorAsset ArrowUp() {
  return detail::Make([](VectorBuilder& builder) {
    builder.StrokePath(detail::Line({12.0F, 19.0F}, {12.0F, 5.5F}), Color::White(), detail::kStroke);
    builder.StrokePath(detail::Polyline({{6.5F, 11.0F}, {12.0F, 5.5F}, {17.5F, 11.0F}}), Color::White(),
                       detail::kStroke);
  });
}

[[nodiscard]] inline VectorAsset ArrowDown() {
  return detail::Make([](VectorBuilder& builder) {
    builder.StrokePath(detail::Line({12.0F, 5.0F}, {12.0F, 18.5F}), Color::White(), detail::kStroke);
    builder.StrokePath(detail::Polyline({{6.5F, 13.0F}, {12.0F, 18.5F}, {17.5F, 13.0F}}), Color::White(),
                       detail::kStroke);
  });
}

[[nodiscard]] inline VectorAsset Minimize() {
  return detail::Make([](VectorBuilder& builder) {
    builder.StrokePath(detail::Line({5.5F, 12.0F}, {18.5F, 12.0F}), Color::White(), detail::kStroke);
  });
}

[[nodiscard]] inline VectorAsset Maximize() {
  return detail::Make([](VectorBuilder& builder) {
    builder.StrokePath(Path::RoundedRect({5.5F, 5.5F, 13.0F, 13.0F}, CornerRadii{1.5F}), Color::White(),
                       detail::kStroke);
  });
}

[[nodiscard]] inline VectorAsset Close() {
  return detail::Make([](VectorBuilder& builder) {
    builder.StrokePath(detail::Line({6.5F, 6.5F}, {17.5F, 17.5F}), Color::White(), detail::kStroke);
    builder.StrokePath(detail::Line({17.5F, 6.5F}, {6.5F, 17.5F}), Color::White(), detail::kStroke);
  });
}

/// Half-filled circle: reads as "switch the colour scheme" in both directions.
[[nodiscard]] inline VectorAsset Theme() {
  return detail::Make([](VectorBuilder& builder) {
    const Path circle = detail::Circle({12.0F, 12.0F}, 8.0F);
    builder.StrokePath(circle, Color::White(), detail::kStroke);
    builder.PushClip(detail::HalfPlane());
    builder.FillPath(circle, Color::White());
    builder.PopClip();
  });
}

/// One icon-only button with a tooltip and the accessible label IconButton needs.
[[nodiscard]] inline View Action(VectorAsset icon, std::string label) {
  const std::string text = label;
  return IconButton(ImageVariant(std::move(icon)), label).With(Tooltip(std::move(text)));
}

}  // namespace hui::icons
