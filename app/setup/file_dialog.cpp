#include "file_dialog.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>
#include <shobjidl.h>
#else
#include <cstdio>
#include <cstdlib>
#endif

namespace dbziw::setup {
namespace fs = std::filesystem;

#ifdef _WIN32

bool fileDialogAvailable() { return true; }

fs::path pickDiscImage(const std::string &title)
{
    wchar_t file[4096] = L"";
    const int n = MultiByteToWideChar(CP_UTF8, 0, title.c_str(), -1, nullptr, 0);
    std::wstring wtitle(size_t(n > 0 ? n : 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, title.c_str(), -1, wtitle.data(), n);
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = L"Disc images (*.iso)\0*.iso\0All files\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = DWORD(std::size(file));
    ofn.lpstrTitle = wtitle.c_str();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&ofn))
        return {};
    return fs::path(file);
}

fs::path pickFolder(const std::string &title)
{
    const int n = MultiByteToWideChar(CP_UTF8, 0, title.c_str(), -1, nullptr, 0);
    std::wstring wtitle(size_t(n > 0 ? n : 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, title.c_str(), -1, wtitle.data(), n);
    const HRESULT started = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    fs::path chosen;
    IFileOpenDialog *dialog = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dialog))))
    {
        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR);
        dialog->SetTitle(wtitle.c_str());
        IShellItem *item = nullptr;
        if (SUCCEEDED(dialog->Show(nullptr)) && SUCCEEDED(dialog->GetResult(&item)))
        {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)))
            {
                chosen = fs::path(path);
                CoTaskMemFree(path);
            }
            item->Release();
        }
        dialog->Release();
    }
    if (SUCCEEDED(started))
        CoUninitialize();
    return chosen;
}

#else

namespace {
// The first picker found on PATH ("zenity" / "kdialog"), or empty.
std::string picker()
{
    for (const char *name : {"zenity", "kdialog"})
    {
        const std::string probe = std::string("command -v ") + name + " >/dev/null 2>&1";
        if (std::system(probe.c_str()) == 0)
            return name;
    }
    return {};
}

std::string shellQuote(const std::string &text)
{
    std::string out = "'";
    for (char c : text)
        out += c == '\'' ? std::string("'\\''") : std::string(1, c);
    return out + "'";
}
} // namespace

bool fileDialogAvailable()
{
    static const bool available = !picker().empty();
    return available;
}

fs::path pickDiscImage(const std::string &title)
{
    const std::string tool = picker();
    std::string command;
    if (tool == "zenity")
        command = "zenity --file-selection --title=" + shellQuote(title) +
                  " --file-filter='Disc images (*.iso) | *.iso *.ISO' --file-filter='All files | *' 2>/dev/null";
    else if (tool == "kdialog")
        command = "kdialog --title " + shellQuote(title) + " --getopenfilename . 'Disc images (*.iso *.ISO)' 2>/dev/null";
    else
        return {};
    FILE *pipe = popen(command.c_str(), "r");
    if (!pipe)
        return {};
    std::string out;
    char buffer[1024];
    while (size_t n = std::fread(buffer, 1, sizeof(buffer), pipe))
        out.append(buffer, n);
    pclose(pipe);
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
        out.pop_back();
    return out.empty() ? fs::path() : fs::path(out);
}

fs::path pickFolder(const std::string &title)
{
    const std::string tool = picker();
    std::string command;
    if (tool == "zenity")
        command = "zenity --file-selection --directory --title=" + shellQuote(title) + " 2>/dev/null";
    else if (tool == "kdialog")
        command = "kdialog --title " + shellQuote(title) + " --getexistingdirectory . 2>/dev/null";
    else
        return {};
    FILE *pipe = popen(command.c_str(), "r");
    if (!pipe)
        return {};
    std::string out;
    char buffer[1024];
    while (size_t n = std::fread(buffer, 1, sizeof(buffer), pipe))
        out.append(buffer, n);
    pclose(pipe);
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
        out.pop_back();
    return out.empty() ? fs::path() : fs::path(out);
}

#endif

} // namespace dbziw::setup
