#pragma once
// The company's log (FIXES.md items 62 and 63, the owner 2026-11-18): everything that happened to the company, newest
// first, dated, saved: the news, the story's events, contracts, missions, payday, the crew's fates, buys and sells,
// hires, travel. An entry keeps numbers and ids only (a pilot's id, a mech's, a news story's record), never words: its
// words are built when it is shown (log_lines), from the game's own tables and the script, so the save holds none of
// MW1's text.
//
// The hub's MESSAGES are the entries not yet heard there (the box's lines, until OK); the LOG button pages through all
// of them; a news entry's story opens in the news's pages (READ), and reading it marks it read. At most kLogCap
// entries: past it the oldest goes, but never a news story not yet read (the oldest of the rest instead).
#include <cstdint>
#include <string>
#include <vector>

#include "mw1_campaign.h"

namespace mw1::log_book {

// LogKind, LogEntry and kLogCap: mw1_campaign.h (the company holds them).

// A new entry at the company's date, newest first; `heard` for what the hub doesn't announce. Past kLogCap, the
// oldest entry that isn't an unread news story goes.
LogEntry& log_add(Company& c, LogKind kind, bool heard, int32_t a = 0, int32_t b = 0, int32_t cc = 0, int32_t d = 0,
                  int64_t n = 0, int64_t m = 0);
// Whether the hub announces this kind (the rest are heard as they're made).
bool log_announced(LogKind k);

// The entry's words, built now: its lines (most say one; a contract three).
std::vector<std::string> log_lines(const Company& c, const LogEntry& e);
// "14 APR 3025": the entry's date, short.
std::string log_date(const Date& d);

// The hub's MESSAGES: the unheard entries' lines, oldest first (as they happened). News stories that share a title
// (MW1's round-ups, item 67) are one line, "NEWS: <title> (3)"; each still opens under READ.
std::vector<std::string> unheard_lines(const Company& c);
// The unheard news entries' indices in c.log, oldest first (READ's stories).
std::vector<size_t> unheard_news(const Company& c);
// OK at the hub: everything heard.
void hear_all(Company& c);
// A news story read: every entry of that record.
void mark_read(Company& c, int32_t record);

// The log as pages for the LOG screen: each entry's date and lines, `lines` a page at the most (an entry never split
// across pages), and each page's news entries' indices in c.log.
struct LogPage {
  std::vector<std::string> lines;
  std::vector<size_t> news;
};
std::vector<LogPage> log_pages(const Company& c, size_t lines, size_t columns);

}  // namespace mw1::log_book

namespace mw1 {
using namespace log_book;
}  // namespace mw1
