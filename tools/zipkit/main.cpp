// zipkit's command line (README.md).
#include <cstdio>
#include <cstring>
#include <string>

#include "zipkit.h"

namespace {
int usage() {
  std::fprintf(stderr,
               "zipkit %s: MechWarrior 3's files in Pirate's Moon's formats.\n"
               "  zipkit mw3-to-pm --install <MechWarrior 3 folder> --out <folder> --world <world>:<new name> ...\n"
               "  e.g. zipkit mw3-to-pm --install \"C:\\Games\\MechWarrior3\" --out data --world c2:mw3c2 --world c4b:mw3c4b\n"
               "  zipkit --version\n",
               zipkit::kVersion);
  return 2;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc == 2 && !std::strcmp(argv[1], "--version")) return std::printf("zipkit %s\n", zipkit::kVersion), 0;
  if (argc < 2 || std::strcmp(argv[1], "mw3-to-pm")) return usage();
  zipkit::Options o;
  for (int i = 2; i < argc; ++i) {
    const std::string a = argv[i];
    if (i + 1 >= argc) return usage();
    const std::string v = argv[++i];
    if (a == "--install") o.install = v;
    else if (a == "--out") o.out = v;
    else if (a == "--world") {
      const size_t c = v.find(':');
      if (c == std::string::npos || c == 0 || c + 1 == v.size()) return usage();
      o.worlds.emplace_back(v.substr(0, c), v.substr(c + 1));
    } else return usage();
  }
  if (o.install.empty() || o.out.empty() || o.worlds.empty()) return usage();
  o.progress = [](const std::string& world, const std::string& step, int done, int total) {
    std::printf("%s: %s %d/%d\n", world.c_str(), step.c_str(), done + 1, total);
    std::fflush(stdout);
  };
  std::string error;
  if (!zipkit::mw3_to_pm(o, &error)) {
    std::fprintf(stderr, "zipkit: %s\n", error.c_str());
    return 1;
  }
  std::printf("done\n");
  return 0;
}
