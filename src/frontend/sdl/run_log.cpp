#include "frontend/sdl/run_log.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <system_error>
#include <thread>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace sdlfe {

namespace {
#if defined(_WIN32)
int fd_dup(int fd) { return _dup(fd); }
int fd_dup2(int from, int to) { return _dup2(from, to); }
int fd_pipe(int p[2]) { return _pipe(p, 1 << 16, _O_BINARY); }
void fd_private(int) {}  // _dup2 onto 1 and 2 also sets the std handles a child inherits
long fd_read(int fd, char* b, unsigned n) { return _read(fd, b, n); }
void fd_write(int fd, const char* b, long n) { (void)_write(fd, b, static_cast<unsigned>(n)); }
void fd_close(int fd) { _close(fd); }
// Bytes in the pipe not yet read; -1 when it can't be told.
long fd_pending(int fd) {
  DWORD avail = 0;
  const HANDLE h = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
  return PeekNamedPipe(h, nullptr, 0, nullptr, &avail, nullptr) ? static_cast<long>(avail) : -1;
}
#else
int fd_dup(int fd) { return dup(fd); }
int fd_dup2(int from, int to) { return dup2(from, to); }
int fd_pipe(int p[2]) { return pipe(p); }
long fd_read(int fd, char* b, unsigned n) { return static_cast<long>(read(fd, b, n)); }
void fd_write(int fd, const char* b, long n) {
  while (n > 0) {
    const ssize_t w = write(fd, b, static_cast<size_t>(n));
    if (w <= 0) return;
    b += w, n -= w;
  }
}
void fd_close(int fd) { close(fd); }
long fd_pending(int fd) {
  int n = 0;
  return ioctl(fd, FIONREAD, &n) == 0 ? n : -1;
}
void fd_private(int fd) { fcntl(fd, F_SETFD, FD_CLOEXEC); }  // not in the programs mw3 starts
#endif
}  // namespace

// stdout and stderr each through a pipe; a thread per pipe passes what comes
// to the console's handle and to the log.
struct RunLog::Impl {
  std::FILE* file = nullptr;
  std::mutex lock;
  struct Tee {
    int fd = -1, console = -1, read = -1;
    std::thread thread;
    std::atomic<bool> busy{false};  // between a read and its write to the file (run_log_drain)
  } tees[2];

  void pass(Tee& t) {
    char buf[4096];
    for (;;) {
      const long n = fd_read(t.read, buf, sizeof buf);
      if (n <= 0) return;
      t.busy = true;
      if (t.console >= 0) fd_write(t.console, buf, n);
      std::lock_guard<std::mutex> g(lock);
      std::fwrite(buf, 1, static_cast<size_t>(n), file);
      std::fflush(file);
      t.busy = false;
    }
  }
};

namespace {
std::atomic<void*> g_open{nullptr};  // the log open now, its RunLog::Impl (run_log_drain)
}  // namespace

bool RunLog::open(const std::string& dir, const std::string& name) {
  namespace fs = std::filesystem;
  std::error_code ec;
  const fs::path log = fs::path(dir) / (name + ".log"), old = fs::path(dir) / (name + ".old.log");
  if (fs::exists(log, ec)) {
    fs::remove(old, ec);
    fs::rename(log, old, ec);
  }
  auto impl = std::make_unique<Impl>();
  impl->file = std::fopen(log.string().c_str(), "wb");
  if (!impl->file) return false;
  std::fflush(stdout);
  std::fflush(stderr);
  for (int i = 0; i < 2; ++i) {
    Impl::Tee& t = impl->tees[i];
    int p[2];
    if (fd_pipe(p) != 0) continue;
    t.fd = i + 1;
    t.console = fd_dup(t.fd);  // -1 without a console: the log only
    t.read = p[0];
    fd_private(t.read);
    if (t.console >= 0) fd_private(t.console);
    fd_dup2(p[1], t.fd);
    fd_close(p[1]);
    Impl* self = impl.get();
    t.thread = std::thread([self, &t] { self->pass(t); });
  }
  impl_ = std::move(impl);
  g_open = impl_.get();
  path_ = log.string();
  return true;
}

RunLog::RunLog() = default;

RunLog::~RunLog() {
  if (!impl_) return;
  if (g_open == impl_.get()) g_open = nullptr;
  std::fflush(stdout);
  std::fflush(stderr);
  for (Impl::Tee& t : impl_->tees) {
    if (t.fd < 0) continue;
    // The console's handle back closes the pipe's last writer: the thread
    // reads what is left, then the end.
    if (t.console >= 0) fd_dup2(t.console, t.fd);
    else fd_close(t.fd);
    if (t.thread.joinable()) t.thread.join();
    fd_close(t.read);
    if (t.console >= 0) fd_close(t.console);
  }
  std::fclose(impl_->file);
}

void run_log_drain(int ms) {
  std::fflush(stdout);
  std::fflush(stderr);
  RunLog::Impl* const impl = static_cast<RunLog::Impl*>(g_open.load());
  if (!impl) return;
  auto idle = [impl] {
    for (RunLog::Impl::Tee& t : impl->tees)
      if (t.fd >= 0 && (fd_pending(t.read) != 0 || t.busy)) return false;
    return true;
  };
  // Twice idle 2 ms apart: a thread whose read has just returned has set `busy` by the second look.
  const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
  while (std::chrono::steady_clock::now() < end) {
    if (idle()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
      if (idle()) return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

}  // namespace sdlfe
