#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdio>

void attachParentConsole()
{
    // Output already goes to a file or pipe (a test, a redirect): keep it there.
    const HANDLE current = GetStdHandle(STD_OUTPUT_HANDLE);
    if (current && current != INVALID_HANDLE_VALUE && GetFileType(current) != FILE_TYPE_UNKNOWN)
        return;
    if (AttachConsole(ATTACH_PARENT_PROCESS))
    {
        std::freopen("CONOUT$", "w", stdout);
        std::freopen("CONOUT$", "w", stderr);
    }
}
#else
void attachParentConsole() {}
#endif
