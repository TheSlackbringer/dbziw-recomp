#pragma once
// DW5 025 v10 Step 5 (ported in DBZIW 011): a slow log destination must not stall the game. Game threads (device worker, GS worker,
// window, EE) write their log lines with fprintf/std::cerr; when stdout/stderr lead somewhere slow (a console, a pipe
// whose reader lags, PowerShell Start-Process's redirect), each write can block the writing thread for long enough to
// cost frames. The pump puts an in-memory pipe (1 MiB) in front of a file descriptor and copies it to the original
// destination on a background thread, so writers only ever wait when that buffer is full.
//
// Each descriptor's text keeps its order and reaches the destination in whole lines (DBZIW 011): text without its
// newline waits for it (or for 64 KB, or for drain()), so when stdout and stderr share a destination their lines
// interleave only at line boundaries. Nothing is lost on a normal exit (atexit) or on the explicit drain the
// _Exit/abort paths call. A hard crash can lose the last few milliseconds of output: PS2X_STDIO_DIRECT=1 (DBZIW-DEBUG)
// keeps the old direct writes for crash hunting.

#include <cstddef>
#include <cstdint>
#include <functional>

namespace ps2x::stdio_pump
{
    // DW5 023 (013) criterion 6: debug lines that are expensive to compute (PS2X_WEAVE_TRACE: row profiles and hashes of
    // every window picture, ~11 ms per window frame on a laptop at 4x) run on one helper thread, so the thread that posts
    // them (the window loop) does not spend its frame on them. Jobs run in the order they were posted; each one owns a
    // copy of the data it needs (`bytes`: its size). Above kDeferredBytes queued the poster waits until the helper has
    // caught up (nothing is ever dropped) and writes a [log:deferred-wait] line, so a disturbed measurement shows.
    // A job larger than the budget is accepted when nothing is queued. drain() runs every queued job first.
    inline constexpr size_t kDeferredBytes = size_t(64) << 20;
    void postDeferred(std::function<void()> job, size_t bytes);
    // Posts that had to wait for the helper (tests).
    uint64_t deferredWaitCount();

    // Puts a pump in front of stdout and stderr, one each; when both lead to the same destination they write it
    // through one shared descriptor and lock, whole lines at a time (installPair).
    // Does nothing when PS2X_STDIO_DIRECT=1, when a descriptor has no valid destination (e.g. a detached process),
    // or when called again. Returns true if at least one descriptor is pumped.
    bool installStdio();

    // Writes everything still queued, puts the original destinations back and stops the threads. Safe to call more
    // than once and from any thread; called by atexit and before every _Exit/abort in the runtime.
    void drain();

    // For tests: pump an arbitrary descriptor (its writes go to a pipe; a thread copies them to a duplicate of the
    // descriptor's original destination). pipeBytes is the buffer the writers see. Returns false on failure.
    bool installFd(int fd, size_t pipeBytes);

    // Pumps two descriptors (installStdio: stdout and stderr), one pump each. When both lead to the same destination
    // the two pumps write it through one descriptor and one lock, so their whole lines interleave only at line
    // boundaries (DBZIW 011; DW5 025 Step 7 shared one pipe instead, which kept the exact order across the two but
    // let one stream's write land inside the other's line). Returns true if at least one descriptor is pumped.
    bool installPair(int first, int second, size_t pipeBytes);

    // DW5 023 step 7T: std::cout, std::cerr and std::clog collect each thread's text and write every finished line
    // with one write, so a line written as a chain of << is never split by another thread's output. Does nothing
    // when PS2X_STDIO_DIRECT=1 or when called again. Returns true if the streams were set up.
    bool installLineStreams();

    // DBZIW 011: for a crash signal handler, where drain() is not safe (locks, a thread join). Writes the bytes with
    // one plain write to the original destination of fd (1 or 2) when it is pumped, otherwise to fd itself, so a crash
    // banner survives the _Exit that follows. Text still queued in the pipe at that moment can be lost.
    void writeDirect(int fd, const char *data, size_t size) noexcept;
}
