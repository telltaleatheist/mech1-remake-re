#include "gamez/zutil/zutl_zip.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>

#include "gamez/zutil/zutl_path.h"

namespace zutl_zip {

ScratchFile::ScratchFile(const void* data, size_t size) {
  static unsigned serial = 0;
  std::error_code ec;
  const std::filesystem::path dir = std::filesystem::temp_directory_path(ec);
  if (ec) return;
  const auto stamp = static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count());
  for (int tries = 0; tries < 8 && !file; ++tries) {
    path = dir / ("mw3-entry-" + std::to_string(stamp) + "-" + std::to_string(serial++) + ".tmp");
    if (std::filesystem::exists(path, ec)) continue;  // never one we did not make
    file = std::fopen(path.string().c_str(), "w+b");
  }
  if (!file) return;
  if (size && std::fwrite(data, 1, size, file) != size) {
    std::fclose(file);
    file = nullptr;
    std::filesystem::remove(path, ec);
    return;
  }
  std::rewind(file);
}

ScratchFile::~ScratchFile() {
  if (!file) return;
  std::fclose(file);
  std::error_code ec;
  std::filesystem::remove(path, ec);
}

int mod_json(size_t mod, const std::string& dir, const std::string& stem, const std::string& file, std::string* text,
             std::string* what, std::string* why) {
  text->clear();
  if (const std::string json = zutl::find_in_mod(mod, dir + stem + "/" + file); !json.empty()) {
    *what = json;
    std::FILE* f = std::fopen(json.c_str(), "rb");
    if (!f) return *why = "cannot be opened", -1;
    char buf[1 << 14];
    for (size_t got; (got = std::fread(buf, 1, sizeof buf, f)) > 0;) text->append(buf, got);
    std::fclose(f);
    return 1;
  }
  const std::string zip = zutl::find_in_mod(mod, dir + stem + ".zip");
  if (zip.empty()) return 0;
  Zip z;
  if (!z.open(zip)) return *what = zip, *why = "not a zip the game can read", -1;
  auto same = [](const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
      if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) return false;
    return true;
  };
  for (const Zip::Entry& e : z.entries) {
    if (!same(e.name, file)) continue;
    *what = zip + ":" + e.name;
    std::vector<uint8_t> data;
    if (!z.read(e, &data, why)) return -1;
    text->assign(data.begin(), data.end());
    return 1;
  }
  return 0;
}

}  // namespace zutl_zip
