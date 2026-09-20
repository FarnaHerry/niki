// platform/windows/main.cpp — the entry translation unit.
//
// Linked as a windows-subsystem executable with mainCRTStartup so the portable
// main() entry is preserved. That means the CLI paths run with no console
// attached: they still read argv and still set the exit code, but their output
// goes nowhere unless the shell redirects it.
//
// No arguments: the designer GUI. With a subcommand: the hui CLI (new,
// validate, codegen, catalog, selftest, mcp).

#include <huxerui/app.h>

import hui.cli;

int main(int argc, char** argv) {
  if (argc > 1) {
    return hui::cli::Run(argc, argv);
  }
  return huxerui::RunApplication();
}
