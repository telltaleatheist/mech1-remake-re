#include "mech3/compact.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "gamez/zutil/zutl_json.h"
#include "rekit/archive.h"
#include "rekit/files.h"
#include "rekit/hash.h"

namespace mech3::compact {
namespace fs = std::filesystem;

namespace {
std::string text_of(const zutl_json::Value* v) {
  return v && v->kind == zutl_json::Value::String ? v->text : std::string();
}
}  // namespace

bool parse(const std::string& text, Manifest* out, std::string* why) {
  *out = {};
  zutl_json::Value root;
  if (!zutl_json::parse(text, &root, why)) return false;
  if (root.kind != zutl_json::Value::Object || text_of(root.get("format")) != kFormat)
    return *why = std::string("not a \"") + kFormat + "\" manifest", false;
  out->game = text_of(root.get("game")), out->version = text_of(root.get("version"));
  out->name = text_of(root.get("name")), out->by = text_of(root.get("by"));
  if (out->game != "mw3" && out->game != "pm") return *why = "its game is neither mw3 nor pm", false;
  const zutl_json::Value* files = root.get("files");
  if (!files || files->kind != zutl_json::Value::Array || files->items.empty()) return *why = "it lists no files", false;
  for (const zutl_json::Value& f : files->items) {
    File x;
    x.path = text_of(f.get("path")), x.sha256 = text_of(f.get("sha256"));
    const zutl_json::Value* size = f.get("size");
    if (x.path.empty() || rekit::safe_relative(x.path) != x.path) return *why = "a file's path is not safe: " + x.path, false;
    if (x.sha256.size() != 64 || x.sha256.find_first_not_of("0123456789abcdef") != std::string::npos)
      return *why = x.path + "'s sha256 is not 64 lower-case hex digits", false;
    if (!size || size->kind != zutl_json::Value::Number || size->text.empty() ||
        size->text.find_first_not_of("0123456789") != std::string::npos || size->text.size() > 12)
      return *why = x.path + " has no size in bytes", false;
    x.size = std::stoull(size->text);
    out->files.push_back(std::move(x));
  }
  return true;
}

std::string words(const Manifest& m) {
  return (m.name.empty() ? std::string(m.game == "pm" ? "MechWarrior 3: Pirate's Moon" : "MechWarrior 3") : m.name) + " (compact" +
         (m.by.empty() ? std::string() : ", " + m.by) + ")";
}

bool peek(const std::string& pick, bool on_disk, const std::string& root, Manifest* out) {
  std::string text, why;
  std::error_code ec;
  if (on_disk || fs::is_directory(pick, ec)) {
    std::ifstream f(fs::path(pick) / root / kManifest, std::ios::binary);
    if (!f) return false;
    std::ostringstream s;
    s << f.rdbuf();
    text = s.str();
  } else {
    rekit::ArchiveReader r;
    if (!r.open_file(pick)) return false;
    rekit::ArchiveReader::Entry e;
    while (r.next(&e)) {
      if (e.dir) continue;
      if (e.path != root + kManifest) return false;  // the manifest is the first file, or there is none
      if (e.size > (8 << 20)) return false;
      uint8_t buf[1 << 16];
      for (int64_t n; (n = r.read(buf, sizeof buf)) > 0 && text.size() < (8u << 20);) text.append(reinterpret_cast<char*>(buf), static_cast<size_t>(n));
      break;
    }
  }
  return !text.empty() && parse(text, out, &why);
}

bool verify(const std::string& dir, const std::string& game, Manifest* out, std::string* why) {
  std::ifstream f(fs::path(dir) / kManifest, std::ios::binary);
  std::ostringstream s;
  s << f.rdbuf();
  std::string bad;
  if (!f || !parse(s.str(), out, &bad)) return *why = "Its list of files (" + std::string(kManifest) + ") could not be read: " + bad + ".", false;
  if (out->game != game)
    return *why = std::string("This compact copy is ") + (out->game == "pm" ? "Pirate's Moon" : "MechWarrior 3") + "'s, not this game's.", false;
  std::error_code ec;
  for (const File& x : out->files) {
    const fs::path p = fs::path(dir) / fs::path(x.path);
    const uintmax_t size = fs::file_size(p, ec);
    if (ec) return *why = "Its file " + x.path + " is missing.", false;
    if (size != x.size || rekit::sha256_file_hex(p.string()) != x.sha256)
      return *why = "Its file " + x.path + " is not as its list says (damaged or changed).", false;
  }
  return true;
}

}  // namespace mech3::compact
