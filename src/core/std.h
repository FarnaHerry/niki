// The standard library surface the headless engine uses, in one place.
//
// The engine is plain C++23 headers rather than module units: the UI layer is
// header-based against <huxerui/huxerui.h>, so keeping the engine headers too
// means no translation unit ever mixes `import std;` with the STL that
// HuxerUI's umbrella header already includes.
#pragma once

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>
