#pragma once
// mw3.log (ours, not original code; docs/program.md, The log): everything mw3
// prints, on stdout and stderr, copied to <write dir>/mw3.log, made new each
// start; the last start's is kept as mw3.old.log, so a crash's log survives
// the restart a player makes before reporting it.
#include <memory>
#include <string>

namespace sdlfe {

// Ours: waits (at most `ms`) until what this program has written to stdout and stderr so far is in the log: both pipes
// empty and neither copying thread between its read and its write to the file. The two pipes are passed on by their
// own threads, so a stdout line and a stderr line written close together can otherwise reach the log in either order
// (the crash test's line ahead of the start's banner, mw1banner). Returns at once with no log open. Not for a signal
// handler (it sleeps).
void run_log_drain(int ms = 2000);

class RunLog {
 public:
  // The log in `dir`; false (and no log) when it cannot be made. The console
  // keeps every line as before.
  RunLog();
  bool open(const std::string& dir, const std::string& name = "mw3");  // <name>.log, <name>.old.log
  ~RunLog();  // the last lines written, the console's own handles back
  const std::string& path() const { return path_; }
  friend void run_log_drain(int ms);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  std::string path_;
};

}  // namespace sdlfe
