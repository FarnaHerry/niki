// What the canvas needs to put resize handles around a node.
//
// A layout is the only place that knows how big a node actually ended up, and a
// pointer handler is the only place that can turn a drag into a new size. The
// two meet through a plain measurement table: the SizeProbe modifier writes into
// it from the layout pass, the handles read from it when a gesture starts, and
// neither of them wants a re-render, so it is deliberately not a State.
#pragma once

#include <huxerui/huxerui.h>

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace hui::ui {

/// The size each node was last measured at, keyed by node id. Written during
/// layout and read by pointer handlers, both on the UI thread.
struct NodeMetrics final {
  void Set(std::string id, huxerui::Size size);
  [[nodiscard]] std::optional<huxerui::Size> Get(std::string_view id) const;

 private:
  // Transparent comparator, so Get can look up an id it does not own.
  std::map<std::string, huxerui::Size, std::less<>> sizes_;
};

/// Records the size of the node it is attached to into a NodeMetrics table.
/// Layout-affecting only in the sense that it observes it: it changes nothing
/// about how the node measures.
struct SizeProbe final {
  class Extension;

  std::string id;
  std::shared_ptr<NodeMetrics> metrics;
};

class SizeProbe::Extension final : public huxerui::NodeExtension {
 public:
  Extension(huxerui::ViewNode& node, const SizeProbe& value) { Update(node, value); }

  void Update(huxerui::ViewNode&, const SizeProbe& value) {
    id_ = value.id;
    metrics_ = value.metrics;
  }

  [[nodiscard]] PaintInvalidation PrepareGeometry(huxerui::ViewNode& node,
                                                  huxerui::TextMeasurer&) override {
    if (metrics_) {
      metrics_->Set(id_, huxerui::Size{node.Bounds().width, node.Bounds().height});
    }
    return PaintInvalidation::None;
  }

 private:
  std::string id_;
  std::shared_ptr<NodeMetrics> metrics_;
};

/// Wraps one node in a frame that places eight handles around it, centred on the
/// four corners and the four edge midpoints. The first child is the node itself
/// and decides the frame's size; the handles are placed against that size, so
/// nothing outside the layout has to know where the node's edges ended up.
class ResizeFrame final : public huxerui::Layout<ResizeFrame> {
 public:
  using Layout::Layout;

  /// Side of one square handle, in logical pixels.
  static constexpr float kHandleSize = 9.0F;

  static huxerui::LayoutResult Measure(huxerui::LayoutContext& context, huxerui::ViewNode& node,
                                       huxerui::Constraints constraints);
};

/// One handle's place on the node: -1 is the leading edge, 0 the middle, 1 the
/// trailing edge, on each axis. Order matches what the canvas builds.
struct HandleSpot final {
  int horizontal = 0;
  int vertical = 0;
};

/// The eight spots, corners first so the most useful handles come first.
inline constexpr std::size_t kHandleCount = 8;
inline constexpr HandleSpot kHandleSpots[kHandleCount] = {
    {-1, -1}, {1, -1}, {-1, 1}, {1, 1}, {0, -1}, {-1, 0}, {0, 1}, {1, 0},
};

}  // namespace hui::ui
