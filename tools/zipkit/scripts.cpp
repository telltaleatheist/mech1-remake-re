// zipkit: a world's build scripts from MW3's interp.zbd, written loose under
// new names. A loose script whose name is in PM's own interp.zbd makes the
// game refuse that archive; a new name is fine. The .gs's `set DB <world>`
// becomes `set DB <new>`, which also names support\<new>.gw and the world's
// folder (zbd\<new>\gamez.zbd); nothing else changes. Every other script it
// sources comes from PM's own interp.zbd.
#include "zipkit.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>

#include "gamez/zinterp/zinterp.h"

namespace fs = std::filesystem;

namespace zipkit {
namespace {

// The script's lines as text, one command a line, tokens joined by a space
// (the text reader splits on spaces; no shipped token holds one).
bool decode(zint::Interp& in, const std::string& name, std::string* text, std::string* error) {
  zint::ScriptFile* f = zint::archive_find(in, name.c_str());
  if (!f) return *error = "MechWarrior 3's interp.zbd has no " + name + ".", false;
  in.binary = 1;
  char line[0x1000];
  while (zint::read_line(in, f, line)) {
    for (uint32_t i = 0; i < in.argc; ++i) {
      if (std::strpbrk(in.argv[i], " \t\r\n") || !*in.argv[i]) {
        std::free(in.line);
        return *error = name + " has a token the text form cannot hold.", false;
      }
      if (i) *text += ' ';
      *text += in.argv[i];
    }
    *text += "\r\n";
    std::free(in.line);
    in.line = nullptr;
  }
  return true;
}

bool write_text(const fs::path& path, const std::string& text, std::string* error) {
  std::error_code ec;
  fs::create_directories(path.parent_path(), ec);
  std::FILE* f = std::fopen(path.string().c_str(), "wb");
  bool ok = f && std::fwrite(text.data(), 1, text.size(), f) == text.size();
  if (f) ok = std::fclose(f) == 0 && ok;
  if (!ok) *error = "Could not write " + path.string() + ".";
  return ok;
}

}  // namespace

bool write_scripts(const std::string& interp, const std::string& world, const std::string& name,
                   const std::string& out, std::string* error) {
  zint::Interp in;
  zint::init(in, interp.c_str(), ".");
  if (!zint::open_archive(in, interp.c_str())) return *error = "Could not open " + interp + ".", false;
  std::string gs, gw;
  bool ok = decode(in, world + ".gs", &gs, error) && decode(in, "Support\\" + world + ".gw", &gw, error);
  zint::destroy(in);
  if (!ok) return false;
  const std::string from = "set DB " + world + "\r\n", to = "set DB " + name + "\r\n";
  const size_t at = gs.find(from);
  if (at == std::string::npos) return *error = world + ".gs has no `set DB " + world + "` line.", false;
  gs.replace(at, from.size(), to);
  return write_text(fs::path(out) / (name + ".gs"), gs, error) &&
         write_text(fs::path(out) / "support" / (name + ".gw"), gw, error);
}

}  // namespace zipkit
