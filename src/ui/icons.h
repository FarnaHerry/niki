// The designer's icon set, recorded as vector assets rather than loaded from
// image files: no resource packaging, and IconButton's style tints them so the
// light/dark switch needs no second set.
#pragma once

#include <huxerui/vector.h>
#include <huxerui/view.h>

#include <string>

namespace hui::icons {

[[nodiscard]] huxerui::VectorAsset New();
[[nodiscard]] huxerui::VectorAsset Open();
[[nodiscard]] huxerui::VectorAsset Save();
[[nodiscard]] huxerui::VectorAsset Export();
[[nodiscard]] huxerui::VectorAsset Undo();
[[nodiscard]] huxerui::VectorAsset Redo();
[[nodiscard]] huxerui::VectorAsset Copy();
[[nodiscard]] huxerui::VectorAsset Delete();
[[nodiscard]] huxerui::VectorAsset ArrowUp();
[[nodiscard]] huxerui::VectorAsset ArrowDown();
[[nodiscard]] huxerui::VectorAsset Minimize();
[[nodiscard]] huxerui::VectorAsset Maximize();
[[nodiscard]] huxerui::VectorAsset Close();
[[nodiscard]] huxerui::VectorAsset Theme();

/// One icon-only button with a tooltip and the accessible label IconButton needs.
[[nodiscard]] huxerui::View Action(huxerui::VectorAsset icon, std::string label);

}  // namespace hui::icons
