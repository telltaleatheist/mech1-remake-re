#include "mw1_loading.h"

namespace mw1::loading {
namespace {
// Letters shown (a highlight's marks aren't).
size_t shown_size(const std::string& s) {
  size_t n = 0;
  for (char c : s) n += c != kHighlight;
  return n;
}
// `s` to its first `n` shown letters, a highlight it leaves open closed.
std::string cut(const std::string& s, size_t n) {
  std::string out;
  bool on = false;
  size_t k = 0;
  for (char c : s) {
    if (c == kHighlight) {
      on = !on, out += c;
      continue;
    }
    if (k++ == n) break;
    out += c;
  }
  if (on) out += kHighlight;
  return out;
}
}  // namespace

std::vector<std::string> terminal_lines(const std::vector<std::string>& lines) {
  std::vector<std::string> out;
  for (const std::string& l : lines) {
    const size_t before = out.size();
    std::string line;
    size_t len = 0, start = 0;  // the line's letters, and its indent's
    bool on = false;            // a highlight open so far
    auto flush = [&] {
      if (on) line += kHighlight;
      out.push_back(line);
      line = std::string(kLoadIndent, ' ') + (on ? std::string(1, kHighlight) : std::string());
      len = start = kLoadIndent;
    };
    size_t i = 0;
    while (i <= l.size()) {
      size_t e = l.find(' ', i);
      if (e == std::string::npos) e = l.size();
      const std::string w = l.substr(i, e - i);
      i = e + 1;
      if (w.empty()) continue;
      const size_t wl = shown_size(w);
      if (len > start && len + 1 + wl > kLoadColumns) flush();
      if (len > start) line += ' ', ++len;
      line += w, len += wl;
      for (char c : w) on = c == kHighlight ? !on : on;
    }
    if (on) line += kHighlight;
    if (len > start || out.size() == before) out.push_back(line);  // (an empty line kept)
  }
  for (std::string& l : out)
    if (shown_size(l) > kLoadColumns) l = cut(l, kLoadColumns);
  if (out.size() > kLoadLines) {
    out.resize(kLoadLines);
    out.back() = cut(out.back(), kLoadColumns - 3) + "...";
  }
  return out;
}

ScreenView loading_view(const LoadingPanel& p) {
  ScreenView v;
  v.backdrop = p.scene;
  v.blocks = {at_place("PLANET", {p.planet}), at_place("PLACE", {p.employer}), at_place("TITLE", {p.title}),
              at_place("LINES", terminal_lines(p.lines)), at_place("LOADING", {"LOADING"})};
  v.panels.assign(kLoadPanelCount, true);
  v.panels[kLoadEnemyFrame] = v.panels[kLoadEnemy] = !p.enemy.empty();
  v.pictures.assign(kLoadPanelCount, std::string());
  v.pictures[kLoadEnemy] = p.enemy;
  return v;
}

}  // namespace mw1::loading
