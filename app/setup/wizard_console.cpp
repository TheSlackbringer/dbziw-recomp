#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdio>

void attachParentConsole()
{
    if (AttachConsole(ATTACH_PARENT_PROCESS))
    {
        std::freopen("CONOUT$", "w", stdout);
        std::freopen("CONOUT$", "w", stderr);
    }
}
#else
void attachParentConsole() {}
#endif
