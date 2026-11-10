#include "hooks.hpp"
#include <windows.h>

namespace {

DWORD WINAPI InitThread(LPVOID param) {
    HMODULE hMod = static_cast<HMODULE>(param);
    meow::hooks::Install(hMod);
    return 0;
}

} // namespace

extern "C" __declspec(dllexport) const char* MeowVersion() {
    return "Meow.dll 1.21.4 Fabric";
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        HANDLE hThread = CreateThread(nullptr, 0, InitThread, hModule, 0, nullptr);
        if (hThread) {
            CloseHandle(hThread);
        }
    } else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        if (lpReserved == nullptr) {
            meow::hooks::Uninstall();
        }
    }
    return TRUE;
}
