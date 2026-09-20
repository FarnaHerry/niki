// platform/linux/main.cpp — the entry translation unit.
//
// No arguments: the designer GUI. With a subcommand: the hui CLI (new,
// validate, codegen, catalog, selftest, mcp). The entry instantiates nothing
// itself, so this file needs no View-building includes beyond the app header.

#include <huxerui/app.h>

import hui.cli;

int main(int argc, char** argv) {
  if (argc > 1) {
    return hui::cli::Run(argc, argv);
  }
  return huxerui::RunApplication();
}
