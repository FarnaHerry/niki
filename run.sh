#!/usr/bin/env bash
# Build (if needed) and run the designer.
#
# Starting this used to take half a minute even with nothing to do. That was not
# the build being slow: it was ninja rebuilding every target because its build
# log had been damaged. `ninja: warning: premature end of file; recovering` is
# what that looks like, and such a log does not heal on its own — a truncated
# entry stays at the end of the file and later runs append after it. Three
# things here fix and prevent it:
#
#   * The lock keeps this script's build step to one process. Two `ninja` runs
#     against one build directory are what truncate the log in the first place.
#   * A damaged log is dropped and the build repeated, so one slow start repairs
#     the directory instead of every start paying for it.
#   * The generator runs only when there is nothing configured yet, rather than
#     re-running on every start to produce the same files.
set -euo pipefail
cd "$(dirname "$0")"

build_dir="${HUI_BUILD_DIR:-build}"

if [[ ! -f "$build_dir/CMakeCache.txt" ]]; then
  cmake -S . -B "$build_dir" -G Ninja
fi

# The lock file lives inside the build directory, so two checkouts never share
# one, and it is held only across the build.
exec 9>"$build_dir/.hui-build.lock"
flock 9

build() {
  cmake --build "$build_dir" --parallel 2>&1
}

output=$(build) || {
  printf '%s\n' "$output" >&2
  exit 1
}

if [[ "$output" == *"premature end of file"* ]]; then
  # `.ninja_log` and `.ninja_deps` record what was built; they are caches, not
  # sources. Dropping them is what makes the next start fast again.
  printf 'hui: build log in %s was damaged; repairing it\n' "$build_dir" >&2
  rm -f "$build_dir/.ninja_log" "$build_dir/.ninja_deps"
  output=$(build) || {
    printf '%s\n' "$output" >&2
    exit 1
  }
fi

flock -u 9

# Stay quiet when there was nothing to do; print the log when there was.
if [[ "$output" != *"no work to do"* ]]; then
  printf '%s\n' "$output"
fi

exec "./$build_dir/hui" "$@"
