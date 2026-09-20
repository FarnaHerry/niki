// The one drag payload the designer speaks. A single payload type keeps one
// DropTarget per container node: a palette card creates (move=false, ref =
// component type), an existing canvas node relocates (move=true, ref = node id).

export module hui.ui.dnd;

import std;

export namespace hui::dnd {

struct Payload final {
  bool move = false;
  std::string ref;
};

}  // namespace hui::dnd
