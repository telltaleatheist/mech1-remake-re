#include "mw1_text_names.h"

#include <cctype>

#include "mw1_market.h"
#include "mw1_pilots.h"

namespace mw1 {
namespace {

bool letter(char c) { return std::isalpha(static_cast<unsigned char>(c)) != 0; }
char up(char c) { return static_cast<char>(std::toupper(static_cast<unsigned char>(c))); }

// At `at` (a word's start), does `name` (its letters; its spaces optional) match? Its end in `*end`. Between the
// name's letters the text may put one space or hyphen.
bool match_at(const std::string& t, size_t at, const std::string& name, size_t* end) {
  size_t i = at;
  bool first = true;
  for (char n : name) {
    if (n == ' ' || n == '-') continue;
    if (!first && i < t.size() && (t[i] == ' ' || t[i] == '-') && i + 1 < t.size() && up(t[i + 1]) == up(n)) ++i;
    if (i >= t.size() || up(t[i]) != up(n)) return false;
    ++i, first = false;
  }
  *end = i;
  return true;
}

}  // namespace

std::vector<std::string> mw1_kind_names() {
  const std::vector<uint8_t>& d = mw_main_data_segment();
  std::vector<std::string> out;
  size_t p = kKindNames;
  for (int32_t k = 0; k < kMw1KindCount; ++k) {
    std::string n;
    while (p < d.size() && d[p] && n.size() < 16) n += static_cast<char>(d[p++]);
    if (p >= d.size() || d[p] || n.size() < 3) return {};
    for (char c : n)
      if (!(c >= 'A' && c <= 'Z') && c != ' ' && c != '-') return {};
    out.push_back(n), ++p;
  }
  return out;
}

std::string our_names(const std::string& text, const std::vector<std::string>& names) {
  if (names.size() != static_cast<size_t>(kMw1KindCount)) return text;
  std::string out;
  size_t i = 0;
  while (i < text.size()) {
    bool done = false;
    if (letter(text[i]) && (i == 0 || !letter(text[i - 1]))) {
      for (int32_t k = 0; k < kMw1KindCount && !done; ++k) {
        size_t end = 0;
        if (!match_at(text, i, names[static_cast<size_t>(k)], &end)) continue;
        // A plural or a possessive, then the word must end.
        bool plural = false, possessive = false;
        size_t past = end;
        if (past + 1 < text.size() && text[past] == '\'' && up(text[past + 1]) == 'S')
          possessive = true, past += 2;
        else if (past < text.size() && up(text[past]) == 'S')
          plural = true, past += 1;
        if (past < text.size() && letter(text[past])) continue;
        bool upper = true;
        for (size_t j = i; j < end; ++j) upper = upper && !(text[j] >= 'a' && text[j] <= 'z');
        std::string ours = mech_title(kOurChassis[k]);
        if (plural) ours += (!ours.empty() && (ours.back() == 's' || ours.back() == 'S')) ? "es" : "s";
        if (upper)
          for (char& c : ours) c = up(c);
        if (possessive) ours += std::string("'") + text[past - 1];
        out += ours;
        i = past, done = true;
      }
    }
    if (!done) out += text[i++];
  }
  return out;
}

std::string our_names(const std::string& text) { return our_names(text, mw1_kind_names()); }

}  // namespace mw1
