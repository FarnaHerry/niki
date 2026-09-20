#include "ui/resize.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

using namespace huxerui;

namespace hui::ui {

void NodeMetrics::Set(std::string id, Size size) {
  sizes_.insert_or_assign(std::move(id), size);
}

std::optional<Size> NodeMetrics::Get(std::string_view id) const {
  const auto found = sizes_.find(id);
  if (found == sizes_.end()) {
    return std::nullopt;
  }
  return found->second;
}

LayoutResult ResizeFrame::Measure(LayoutContext& context, ViewNode& node, Constraints constraints) {
  LayoutResult result;
  if (node.ChildCount() == 0) {
    return result.SetSize({});
  }

  ViewNode& content = node.ChildAt(0);
  const Size size = context.Measure(content, constraints.Loose());
  result.Place(content, {});

  const std::size_t count = std::min(node.ChildCount() - 1, kHandleCount);
  for (std::size_t index = 0; index < count; ++index) {
    ViewNode& handle = node.ChildAt(index + 1);
    const Constraints tight{kHandleSize, kHandleSize, kHandleSize, kHandleSize};
    static_cast<void>(context.Measure(handle, tight));
    const HandleSpot spot = kHandleSpots[index];
    // -1, 0, 1 select the leading edge, the middle and the trailing edge, and
    // the handle is inset so that it stays wholly inside the node: a handle
    // hanging over the edge would be outside this frame's bounds, where a
    // pointer over it may never be routed here at all.
    const float axis_x = size.width * (static_cast<float>(spot.horizontal) + 1.0F) * 0.5F;
    const float axis_y = size.height * (static_cast<float>(spot.vertical) + 1.0F) * 0.5F;
    const float inset = kHandleSize * 0.5F;
    result.Place(handle,
                 {std::clamp(axis_x - inset, 0.0F, std::max(0.0F, size.width - kHandleSize)),
                  std::clamp(axis_y - inset, 0.0F, std::max(0.0F, size.height - kHandleSize))});
  }

  // The frame reports the node's own size, so selecting a node never moves it
  // and a container's spacing does not change when its child is selected.
  return result.SetSize(size);
}

}  // namespace hui::ui
