#include "icons.h"

#include <huxerui/presentation.h>

#include <cstddef>
#include <functional>
#include <initializer_list>
#include <utility>

using namespace huxerui;

namespace hui::icons {
namespace {

constexpr float kGrid = 24.0F;
constexpr StrokeStyle kStroke{.width = 1.7F, .cap = StrokeCap::Round, .join = StrokeJoin::Round};

[[nodiscard]] VectorAsset Make(const std::function<void(VectorBuilder&)>& draw) {
  return VectorAsset::Create(Size{kGrid, kGrid}, draw);
}

[[nodiscard]] Path Line(Point from, Point to) { return Path().MoveTo(from).LineTo(to); }

[[nodiscard]] Path Polyline(std::initializer_list<Point> points) {
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

[[nodiscard]] Path Circle(Point center, float radius) {
  Path path;
  path.MoveTo({center.x - radius, center.y})
      .ArcTo({radius, radius}, 0.0F, ArcSize::Small, ArcDirection::Clockwise, {center.x + radius, center.y})
      .ArcTo({radius, radius}, 0.0F, ArcSize::Small, ArcDirection::Clockwise, {center.x - radius, center.y})
      .Close();
  return path;
}

}  // namespace

VectorAsset New() {
  return Make([](VectorBuilder& builder) {
    builder.StrokePath(Line({12.0F, 5.0F}, {12.0F, 19.0F}), Color::White(), kStroke);
    builder.StrokePath(Line({5.0F, 12.0F}, {19.0F, 12.0F}), Color::White(), kStroke);
  });
}

VectorAsset Open() {
  return Make([](VectorBuilder& builder) {
    builder.StrokePath(Polyline({{3.0F, 19.0F}, {3.0F, 6.5F}, {9.5F, 6.5F}, {12.0F, 9.5F}, {21.0F, 9.5F},
                                 {21.0F, 19.0F}, {3.0F, 19.0F}}),
                       Color::White(), kStroke);
  });
}

VectorAsset Save() {
  return Make([](VectorBuilder& builder) {
    builder.StrokePath(Path::RoundedRect({4.0F, 4.0F, 16.0F, 16.0F}, CornerRadii{2.5F}), Color::White(), kStroke);
    builder.StrokePath(Polyline({{8.5F, 4.5F}, {8.5F, 10.5F}, {15.5F, 10.5F}, {15.5F, 4.5F}}), Color::White(),
                       kStroke);
    builder.StrokePath(Path::RoundedRect({7.5F, 13.5F, 9.0F, 6.5F}, CornerRadii{1.5F}), Color::White(), kStroke);
  });
}

VectorAsset Export() {
  return Make([](VectorBuilder& builder) {
    builder.StrokePath(Polyline({{11.0F, 5.0F}, {5.0F, 5.0F}, {5.0F, 19.0F}, {19.0F, 19.0F}, {19.0F, 13.0F}}),
                       Color::White(), kStroke);
    builder.StrokePath(Line({12.5F, 11.5F}, {19.5F, 4.5F}), Color::White(), kStroke);
    builder.StrokePath(Polyline({{14.0F, 4.5F}, {19.5F, 4.5F}, {19.5F, 10.0F}}), Color::White(), kStroke);
  });
}

VectorAsset Undo() {
  return Make([](VectorBuilder& builder) {
    builder.StrokePath(Path().MoveTo({7.0F, 11.0F}).QuadraticTo({12.0F, 5.0F}, {17.0F, 11.0F}).LineTo({17.0F, 17.5F}),
                       Color::White(), kStroke);
    builder.StrokePath(Polyline({{4.0F, 8.0F}, {7.0F, 11.0F}, {10.0F, 8.0F}}), Color::White(), kStroke);
  });
}

VectorAsset Redo() {
  return Make([](VectorBuilder& builder) {
    builder.StrokePath(Path().MoveTo({17.0F, 11.0F}).QuadraticTo({12.0F, 5.0F}, {7.0F, 11.0F}).LineTo({7.0F, 17.5F}),
                       Color::White(), kStroke);
    builder.StrokePath(Polyline({{20.0F, 8.0F}, {17.0F, 11.0F}, {14.0F, 8.0F}}), Color::White(), kStroke);
  });
}

VectorAsset Copy() {
  return Make([](VectorBuilder& builder) {
    builder.StrokePath(Path::RoundedRect({3.5F, 3.5F, 12.0F, 12.0F}, CornerRadii{2.0F}), Color::White(), kStroke);
    builder.StrokePath(Path::RoundedRect({8.5F, 8.5F, 12.0F, 12.0F}, CornerRadii{2.0F}), Color::White(), kStroke);
  });
}

VectorAsset Delete() {
  return Make([](VectorBuilder& builder) {
    builder.StrokePath(Line({4.0F, 7.0F}, {20.0F, 7.0F}), Color::White(), kStroke);
    builder.StrokePath(Polyline({{9.5F, 7.0F}, {9.5F, 4.5F}, {14.5F, 4.5F}, {14.5F, 7.0F}}), Color::White(), kStroke);
    builder.StrokePath(Polyline({{6.5F, 7.0F}, {7.6F, 19.5F}, {16.4F, 19.5F}, {17.5F, 7.0F}}), Color::White(),
                       kStroke);
  });
}

VectorAsset ArrowUp() {
  return Make([](VectorBuilder& builder) {
    builder.StrokePath(Line({12.0F, 19.0F}, {12.0F, 5.5F}), Color::White(), kStroke);
    builder.StrokePath(Polyline({{6.5F, 11.0F}, {12.0F, 5.5F}, {17.5F, 11.0F}}), Color::White(), kStroke);
  });
}

VectorAsset ArrowDown() {
  return Make([](VectorBuilder& builder) {
    builder.StrokePath(Line({12.0F, 5.0F}, {12.0F, 18.5F}), Color::White(), kStroke);
    builder.StrokePath(Polyline({{6.5F, 13.0F}, {12.0F, 18.5F}, {17.5F, 13.0F}}), Color::White(), kStroke);
  });
}

VectorAsset Minimize() {
  return Make([](VectorBuilder& builder) {
    builder.StrokePath(Line({5.5F, 12.0F}, {18.5F, 12.0F}), Color::White(), kStroke);
  });
}

VectorAsset Maximize() {
  return Make([](VectorBuilder& builder) {
    builder.StrokePath(Path::RoundedRect({5.5F, 5.5F, 13.0F, 13.0F}, CornerRadii{1.5F}), Color::White(), kStroke);
  });
}

VectorAsset Close() {
  return Make([](VectorBuilder& builder) {
    builder.StrokePath(Line({6.5F, 6.5F}, {17.5F, 17.5F}), Color::White(), kStroke);
    builder.StrokePath(Line({17.5F, 6.5F}, {6.5F, 17.5F}), Color::White(), kStroke);
  });
}

VectorAsset Theme() {
  return Make([](VectorBuilder& builder) {
    const Path circle = Circle({12.0F, 12.0F}, 8.0F);
    builder.StrokePath(circle, Color::White(), kStroke);
    builder.PushClip(Polyline({{0.0F, 0.0F}, {12.0F, 0.0F}, {12.0F, 24.0F}, {0.0F, 24.0F}}).Close());
    builder.FillPath(circle, Color::White());
    builder.PopClip();
  });
}

View Action(VectorAsset icon, std::string label) {
  const std::string text = label;
  return IconButton(ImageVariant(std::move(icon)), label).With(Tooltip(std::move(text)));
}

}  // namespace hui::icons
