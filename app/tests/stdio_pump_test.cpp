// CTest stdio_pump (DW5 025 v10 Step 5): a slow log destination must not stall the writing thread.
// Ported to DBZIW in issue 011 from DW5 dbb0454 (app/tests/stdio_pump_test.cpp).
// The destination is a small pipe that nobody reads for a while (like a lagging console or redirect). Writes through
// the pump must return at once; afterwards every byte arrives, in order; after drain() writes go straight through.
#include "runtime/ps2_stdio_pump.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#define T_PIPE(fds) _pipe(fds, 4096, _O_BINARY)
#define T_DUP _dup
#define T_READ _read
#define T_WRITE _write
#define T_CLOSE _close
#else
#include <unistd.h>
#define T_PIPE(fds) pipe(fds)
#define T_DUP dup
#define T_READ read
#define T_WRITE write
#define T_CLOSE close
#endif

namespace
{
    int g_failures = 0;
    void check(bool ok, const char *what)
    {
        std::printf("%s %s\n", ok ? "PASS" : "FAIL", what);
        if (!ok)
            ++g_failures;
    }

    std::string readExactly(int fd, size_t bytes)
    {
        std::string out;
        char buffer[4096];
        while (out.size() < bytes)
        {
            const auto n = T_READ(fd, buffer, static_cast<unsigned int>(sizeof(buffer)));
            if (n <= 0)
                break;
            out.append(buffer, static_cast<size_t>(n));
        }
        return out;
    }
}

int main()
{
    int dest[2];
    if (T_PIPE(dest) != 0)
    {
        std::printf("FAIL cannot create the destination pipe\n");
        return 1;
    }
    const int program = T_DUP(dest[1]); // the descriptor the "game" writes to
    T_CLOSE(dest[1]);
    check(ps2x::stdio_pump::installFd(program, 1u << 20), "pump installed in front of the descriptor");

    // 400 lines of 100 bytes = 40 KB, ten times what the unread destination pipe can hold.
    std::string expected;
    for (int i = 0; i < 400; ++i)
    {
        char line[101];
        std::snprintf(line, sizeof(line), "[line] %04d %s\n", i, std::string(86, 'a' + i % 26).c_str());
        expected += line;
    }
    const auto t0 = std::chrono::steady_clock::now();
    for (size_t at = 0; at < expected.size(); at += 100)
        T_WRITE(program, expected.data() + at, 100);
    const double writeMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    std::printf("      400 writes took %.2f ms with the destination unread\n", writeMs);
    check(writeMs < 200.0, "writes return while the destination is not being read");

    std::this_thread::sleep_for(std::chrono::milliseconds(300)); // the destination stays unread a while longer
    const std::string got = readExactly(dest[0], expected.size());
    check(got == expected, "every byte arrives, in order");

    // drain() while bytes are still queued: they arrive, and later writes go straight to the destination.
    T_WRITE(program, "queued\n", 7);
    std::string tail;
    std::thread reader([&] { tail = readExactly(dest[0], 7 + 7); });
    ps2x::stdio_pump::drain();
    T_WRITE(program, "direct\n", 7);
    reader.join();
    check(tail == "queued\ndirect\n", "drain() delivers what is queued, then writes go straight through");
    ps2x::stdio_pump::drain(); // a second drain does nothing

    T_CLOSE(program);
    T_CLOSE(dest[0]);

    // Same destination (stdout and stderr into one log). DW5 025 Step 7 kept exact program order across the two with
    // one shared pipe; DBZIW 011 (criterion 2 as changed 2026-11-03) gives each its own pump that writes whole lines
    // only, because the Windows C runtime writes an fprintf(stderr, ...) line in pieces and the other stream's write
    // could land between them. Checked: every line whole, each stream's lines in their own order, the two streams
    // interleaved only at line boundaries; and a line written in pieces with the other stream writing in between.
    {
        int log[2];
        if (T_PIPE(log) != 0)
        {
            std::printf("FAIL cannot create the shared destination pipe\n");
            return 1;
        }
        const int out = T_DUP(log[1]), err = T_DUP(log[1]);
        T_CLOSE(log[1]);
        check(ps2x::stdio_pump::installPair(out, err, 1u << 20), "pair installed (same destination)");
        std::string writtenOut, writtenErr;
        size_t total = 0;
        for (int i = 0; i < 1000; ++i)
        {
            char line[48];
            const bool toErr = (i % 2) != 0;
            const int n = std::snprintf(line, sizeof(line), "%s %04d ordered line\n", toErr ? "err" : "out", i);
            T_WRITE(toErr ? err : out, line, n);
            (toErr ? writtenErr : writtenOut).append(line, static_cast<size_t>(n));
            total += static_cast<size_t>(n);
        }
        // The torn-line pattern: half a line to err, a whole line to out, then the rest of err's line.
        for (int i = 0; i < 200; ++i)
        {
            char head[32], whole[48], rest[32];
            const int h = std::snprintf(head, sizeof(head), "err %04d first half ", i);
            const int w = std::snprintf(whole, sizeof(whole), "out %04d between the halves\n", i);
            const int r = std::snprintf(rest, sizeof(rest), "second half\n");
            T_WRITE(err, head, h);
            T_WRITE(out, whole, w);
            T_WRITE(err, rest, r);
            writtenErr.append(head, static_cast<size_t>(h)).append(rest, static_cast<size_t>(r));
            writtenOut.append(whole, static_cast<size_t>(w));
            total += static_cast<size_t>(h + w + r);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // the log is read only afterwards
        std::string got;
        std::thread reader([&] { got = readExactly(log[0], total); });
        ps2x::stdio_pump::drain();
        reader.join();
        std::string gotOut, gotErr;
        bool whole = got.size() == total && (got.empty() || got.back() == '\n');
        for (size_t at = 0; at < got.size();)
        {
            const size_t end = got.find('\n', at);
            const std::string line = got.substr(at, (end == std::string::npos ? got.size() : end + 1) - at);
            at += line.size();
            if (line.rfind("out ", 0) == 0)
                gotOut += line;
            else if (line.rfind("err ", 0) == 0)
                gotErr += line;
            else
                whole = false;
            if (line.find("out ", 1) != std::string::npos || line.find("err ", 1) != std::string::npos)
                whole = false; // another stream's text inside a line
        }
        check(whole, "same destination: every line arrives whole (no line holds the other stream's text)");
        check(gotOut == writtenOut && gotErr == writtenErr, "same destination: each stream's lines keep their order");
        T_CLOSE(out);
        T_CLOSE(err);
        T_CLOSE(log[0]);
    }

    // Different destinations stay separate: each descriptor's bytes reach its own destination, in order.
    {
        int a[2], b[2];
        if (T_PIPE(a) != 0 || T_PIPE(b) != 0)
        {
            std::printf("FAIL cannot create the two destination pipes\n");
            return 1;
        }
        const int out = T_DUP(a[1]), err = T_DUP(b[1]);
        T_CLOSE(a[1]);
        T_CLOSE(b[1]);
        check(ps2x::stdio_pump::installPair(out, err, 1u << 20), "pair installed (different destinations)");
        std::string toA, toB;
        for (int i = 0; i < 200; ++i)
        {
            char line[48];
            int n = std::snprintf(line, sizeof(line), "a %04d\n", i);
            T_WRITE(out, line, n);
            toA.append(line, static_cast<size_t>(n));
            n = std::snprintf(line, sizeof(line), "b %04d\n", i);
            T_WRITE(err, line, n);
            toB.append(line, static_cast<size_t>(n));
        }
        std::string gotA, gotB;
        std::thread readerA([&] { gotA = readExactly(a[0], toA.size()); });
        std::thread readerB([&] { gotB = readExactly(b[0], toB.size()); });
        ps2x::stdio_pump::drain();
        readerA.join();
        readerB.join();
        check(gotA == toA && gotB == toB, "different destinations: each stream reaches its own destination");
        T_CLOSE(out);
        T_CLOSE(err);
        T_CLOSE(a[0]);
        T_CLOSE(b[0]);
    }

    std::printf("%s\n", g_failures ? "stdio_pump: FAIL" : "stdio_pump: PASS");
    return g_failures ? 1 : 0;
}
