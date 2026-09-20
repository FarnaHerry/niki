// Entry translation unit.
//
// No arguments: the designer GUI. With a subcommand: the hui CLI (new,
// validate, codegen, catalog, selftest, mcp). The entry deliberately
// instantiates nothing itself — View-building templates live behind the
// imported modules' plain functions, so this file needs no `import std;`.

import huxerui;
import hui.cli;
import hui.ui.app;

int main(int argc, char** argv) {
  if (argc > 1) {
    return hui::cli::Run(argc, argv);
  }
  return huxerui::RunApplication();
}
