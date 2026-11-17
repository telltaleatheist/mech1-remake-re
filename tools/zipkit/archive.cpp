// zipkit: MW3's zArchive (version 1) re-tailed as Pirate's Moon's (version 2).
// The entries and their data are the same; MW3's tail {1, count} becomes
// PM's {2, count, crc}, the CRC (zutl_zar::crc32) over each entry's data in
// table order (pm::zar::archive_crc). Read and written as bytes, so neither
// game's table reader needs to be the running one.
#include "zipkit.h"

#include <cstdio>
#include <cstring>
#include <vector>

#include "gamez/zutil/zutl_zar.h"

namespace zipkit {
namespace {
bool slurp(const std::string& path, std::vector<uint8_t>* out) {
  std::FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return false;
  std::fseek(f, 0, SEEK_END);
  const long n = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  out->resize(n > 0 ? static_cast<size_t>(n) : 0);
  const bool ok = n >= 0 && (out->empty() || std::fread(out->data(), out->size(), 1, f) == 1);
  std::fclose(f);
  return ok;
}
}  // namespace

bool file_crc(const std::string& path, uint64_t* size, uint32_t* crc) {
  std::vector<uint8_t> d;
  if (!slurp(path, &d)) return false;
  *size = d.size();
  *crc = zutl_zar::crc32(0, d.data(), d.size());
  return true;
}

bool retail_archive(const std::string& src, const std::string& dst, std::string* error) {
  std::vector<uint8_t> d;
  if (!slurp(src, &d)) return *error = "Could not read " + src + ".", false;
  uint32_t version = 0, count = 0;
  if (d.size() >= 8) std::memcpy(&version, d.data() + d.size() - 8, 4), std::memcpy(&count, d.data() + d.size() - 4, 4);
  const uint64_t table = static_cast<uint64_t>(count) * sizeof(zutl_zar::Entry);
  if (d.size() < 8 || version != 1 || table > d.size() - 8)
    return *error = src + " is not a MechWarrior 3 archive (version 1).", false;
  const size_t at = d.size() - 8 - static_cast<size_t>(table);
  uint32_t crc = 0;
  for (uint32_t i = 0; i < count; ++i) {
    zutl_zar::Entry e;
    std::memcpy(&e, d.data() + at + i * sizeof e, sizeof e);
    if (e.start > at || e.length > at - e.start)
      return *error = src + " is damaged: entry " + std::to_string(i) + " points past its data.", false;
    crc = zutl_zar::crc32(crc, d.data() + e.start, e.length);
  }
  std::FILE* f = std::fopen(dst.c_str(), "wb");
  if (!f) return *error = "Could not write " + dst + ".", false;
  const uint32_t tail[3] = {2, count, crc};
  bool ok = std::fwrite(d.data(), d.size() - 8, 1, f) == 1 && std::fwrite(tail, 4, 3, f) == 3;
  ok = std::fclose(f) == 0 && ok;
  if (!ok) *error = "Could not write " + dst + ".";
  return ok;
}

}  // namespace zipkit
