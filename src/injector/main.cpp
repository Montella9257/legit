#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <psapi.h>

#include <cstdio>
#include <cwchar>
#include <string>
#include <vector>
#include <algorithm>

namespace {

struct CandidateProcess {
    DWORD pid = 0;
    int score = 0;
    std::wstring title;
    std::wstring exeName;
};

std::wstring ToLower(std::wstring s) {
    for (auto& ch : s) {
        if (ch >= L'A' && ch <= L'Z') ch = static_cast<wchar_t>(ch - L'A' + L'a');
    }
    return s;
}

bool ProcessHasModule(DWORD pid, const wchar_t* targetModLower, bool matchFullName = false) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) return false;

    MODULEENTRY32W me{};
    me.dwSize = sizeof(me);
    bool found = false;
    if (Module32FirstW(snap, &me)) {
        do {
            std::wstring mod = ToLower(me.szModule);
            if (matchFullName) {
                if (mod == targetModLower) { found = true; break; }
            } else {
                if (mod == targetModLower) { found = true; break; }
            }
        } while (Module32NextW(snap, &me));
    }
    CloseHandle(snap);
    return found;
}

std::wstring GetExeName(DWORD pid) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return {};
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    std::wstring name;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (pe.th32ProcessID == pid) {
                name = pe.szExeFile;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return name;
}

BOOL CALLBACK EnumMcCallback(HWND hwnd, LPARAM lParam) {
    auto* list = reinterpret_cast<std::vector<CandidateProcess>*>(lParam);
    if (!IsWindowVisible(hwnd)) return TRUE;

    wchar_t cls[64]{};
    GetClassNameW(hwnd, cls, 64);

    wchar_t titleBuf[256]{};
    GetWindowTextW(hwnd, titleBuf, 256);
    std::wstring title(titleBuf);
    std::wstring titleLow = ToLower(title);

    bool isGlfw = (wcscmp(cls, L"GLFW30") == 0);
    bool hasMcTitle = (titleLow.find(L"minecraft") != std::wstring::npos);
    if (!isGlfw && !hasMcTitle) return TRUE;

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) return TRUE;

    std::wstring exe = ToLower(GetExeName(pid));
    bool isJava = (exe == L"javaw.exe" || exe == L"java.exe");
    if (!isJava && !ProcessHasModule(pid, L"jvm.dll")) return TRUE;

    int score = 0;
    if (isGlfw) score += 20;
    if (hasMcTitle) score += 30;
    if (titleLow.find(L"1.21.4") != std::wstring::npos) score += 40;
    if (titleLow.find(L"fabric") != std::wstring::npos) score += 15;
    if (isJava) score += 10;

    list->push_back({ pid, score, title, exe });
    return TRUE;
}

std::vector<CandidateProcess> FindMinecraftProcesses() {
    std::vector<CandidateProcess> candidates;
    EnumWindows(EnumMcCallback, reinterpret_cast<LPARAM>(&candidates));

    if (candidates.empty()) {
        // Fallback: scan javaw.exe / java.exe with both jvm.dll and opengl32.dll loaded
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W pe{};
            pe.dwSize = sizeof(pe);
            if (Process32FirstW(snap, &pe)) {
                do {
                    std::wstring exe = ToLower(pe.szExeFile);
                    if (exe == L"javaw.exe" || exe == L"java.exe") {
                        if (ProcessHasModule(pe.th32ProcessID, L"jvm.dll") &&
                            ProcessHasModule(pe.th32ProcessID, L"opengl32.dll")) {
                            candidates.push_back({ pe.th32ProcessID, 15, L"(Java OpenGL window)", exe });
                        }
                    }
                } while (Process32NextW(snap, &pe));
            }
            CloseHandle(snap);
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const CandidateProcess& a, const CandidateProcess& b) {
        return a.score > b.score;
    });
    return candidates;
}

bool InjectDllW(DWORD pid, const std::wstring& fullDllPath, std::string& errOut) {
    HANDLE hProc = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
        PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
        FALSE, pid);

    if (!hProc) {
        DWORD e = GetLastError();
        errOut = "OpenProcess failed (error " + std::to_string(e) + "). Try running as Administrator if Minecraft is elevated.";
        return false;
    }

    BOOL isWow64 = FALSE;
    if (IsWow64Process(hProc, &isWow64) && isWow64) {
        CloseHandle(hProc);
        errOut = "Target process is 32-bit, but Meow.dll is 64-bit.";
        return false;
    }

    size_t bytes = (fullDllPath.size() + 1) * sizeof(wchar_t);
    LPVOID remoteMem = VirtualAllocEx(hProc, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteMem) {
        DWORD e = GetLastError();
        CloseHandle(hProc);
        errOut = "VirtualAllocEx failed (error " + std::to_string(e) + ").";
        return false;
    }

    if (!WriteProcessMemory(hProc, remoteMem, fullDllPath.c_str(), bytes, nullptr)) {
        DWORD e = GetLastError();
        VirtualFreeEx(hProc, remoteMem, 0, MEM_RELEASE);
        CloseHandle(hProc);
        errOut = "WriteProcessMemory failed (error " + std::to_string(e) + ").";
        return false;
    }

    HMODULE hKernel = GetModuleHandleW(L"kernel32.dll");
    FARPROC pLoadLib = hKernel ? GetProcAddress(hKernel, "LoadLibraryW") : nullptr;
    if (!pLoadLib) {
        VirtualFreeEx(hProc, remoteMem, 0, MEM_RELEASE);
        CloseHandle(hProc);
        errOut = "Failed to resolve kernel32!LoadLibraryW.";
        return false;
    }

    HANDLE hThread = CreateRemoteThread(
        hProc, nullptr, 0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(pLoadLib),
        remoteMem, 0, nullptr);

    if (!hThread) {
        DWORD e = GetLastError();
        VirtualFreeEx(hProc, remoteMem, 0, MEM_RELEASE);
        CloseHandle(hProc);
        errOut = "CreateRemoteThread failed (error " + std::to_string(e) + ").";
        return false;
    }

    WaitForSingleObject(hThread, 10000);
    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);

    CloseHandle(hThread);
    VirtualFreeEx(hProc, remoteMem, 0, MEM_RELEASE);
    CloseHandle(hProc);

    if (exitCode == 0) {
        errOut = "Remote LoadLibraryW returned NULL (failed to load Meow.dll inside target process).";
        return false;
    }
    return true;
}

std::wstring GetDefaultDllPath() {
    wchar_t exePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring s(exePath);
    size_t pos = s.find_last_of(L"\\/");
    std::wstring dir = (pos != std::wstring::npos) ? s.substr(0, pos + 1) : L"";
    return dir + L"Meow.dll";
}

} // namespace

int main() {
    SetConsoleOutputCP(CP_UTF8);

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::wstring rawPath = (argv && argc >= 2 && argv[1] && argv[1][0] != L'\0')
        ? std::wstring(argv[1])
        : GetDefaultDllPath();
    if (argv) LocalFree(argv);

    wchar_t fullBuf[MAX_PATH]{};
    if (GetFullPathNameW(rawPath.c_str(), MAX_PATH, fullBuf, nullptr) == 0) {
        std::printf("[!] Invalid DLL path.\n");
        return 1;
    }
    std::wstring fullPath(fullBuf);

    DWORD attr = GetFileAttributesW(fullPath.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        std::printf("[!] Meow.dll not found at: %ls\n", fullPath.c_str());
        return 1;
    }

    std::printf("[*] Looking for running Minecraft 1.21.4 Fabric...\n");
    auto procs = FindMinecraftProcesses();
    if (procs.empty()) {
        std::printf("[!] Minecraft 1.21.4 Fabric was not found.\n");
        std::printf("    Start Minecraft 1.21.4 Fabric first, wait until the window opens, then run inject.bat!\n");
        return 2;
    }

    const auto& target = procs.front();
    std::printf("[+] Found process PID=%lu (%ls) - %ls\n",
                target.pid, target.exeName.c_str(), target.title.c_str());

    if (ProcessHasModule(target.pid, L"meow.dll")) {
        std::printf("[i] Meow.dll is already injected in PID=%lu! Press R-Shift in Minecraft.\n", target.pid);
        return 0;
    }

    std::string err;
    if (!InjectDllW(target.pid, fullPath, err)) {
        std::printf("[!] Injection failed: %s\n", err.c_str());
        return 3;
    }

    std::printf("[+] Success! Meow.dll injected into Minecraft (PID=%lu).\n", target.pid);
    std::printf("[+] Switch to Minecraft and press RIGHT SHIFT (R-Shift) to open the menu.\n");
    return 0;
}
