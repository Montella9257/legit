#include "hooks.hpp"
#include "config.hpp"
#include "jvm_bridge.hpp"
#include "modules.hpp"
#include "ui.hpp"

#include "../../third_party/imgui/imgui.h"
#include "../../third_party/imgui/backends/imgui_impl_win32.h"
#include "../../third_party/imgui/backends/imgui_impl_opengl3.h"
#include "../../third_party/minhook/include/MinHook.h"

#include <GL/gl.h>
#include <atomic>
#include <mutex>
#include <vector>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace meow::hooks {

namespace {

using FnWglSwapBuffers = BOOL(WINAPI*)(HDC);
using FnSetCursorPos   = BOOL(WINAPI*)(int, int);
using FnClipCursor     = BOOL(WINAPI*)(const RECT*);

FnWglSwapBuffers oWglSwapBuffers = nullptr;
FnSetCursorPos   oSetCursorPos   = nullptr;
FnClipCursor     oClipCursor     = nullptr;
WNDPROC          oWndProc        = nullptr;

HWND    g_Hwnd         = nullptr;
HMODULE g_SelfModule   = nullptr;
bool    g_ImGuiInit    = false;
bool    g_PrevMenuOpen = false;
RECT    g_SavedClip{};
bool    g_HasSavedClip = false;

std::atomic<bool> g_MenuOpen{false};
std::atomic<bool> g_InMcScreen{false};
std::atomic<bool> g_ShuttingDown{false};
std::atomic<int>  g_PendingBindKey{0};

std::mutex        g_KeyQueueMtx;
std::vector<int>  g_KeyQueue;

ClientConfig  g_Config;
JvmBridge     g_Bridge;
ModuleManager g_Modules;
MeowUI        g_UI;

struct FindWinCtx {
    DWORD pid = 0;
    HWND  best = nullptr;
    HWND  fallback = nullptr;
};

BOOL CALLBACK EnumMcWindowsProc(HWND hwnd, LPARAM lParam) {
    auto* ctx = reinterpret_cast<FindWinCtx*>(lParam);
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != ctx->pid) return TRUE;
    if (!IsWindowVisible(hwnd)) return TRUE;

    wchar_t cls[64]{};
    GetClassNameW(hwnd, cls, 64);
    if (wcscmp(cls, L"GLFW30") == 0) {
        ctx->best = hwnd;
        return FALSE;
    }

    RECT rc{};
    GetClientRect(hwnd, &rc);
    if ((rc.right - rc.left) > 300 && (rc.bottom - rc.top) > 200) {
        ctx->fallback = hwnd;
    }
    return TRUE;
}

HWND FindOwnMinecraftWindow() {
    FindWinCtx ctx;
    ctx.pid = GetCurrentProcessId();
    EnumWindows(EnumMcWindowsProc, reinterpret_cast<LPARAM>(&ctx));
    return ctx.best ? ctx.best : ctx.fallback;
}

int TranslateVirtualKeyEx(WPARAM wParam, LPARAM lParam) {
    if (wParam == VK_SHIFT) {
        UINT sc = static_cast<UINT>((lParam >> 16) & 0xFF);
        UINT mapped = MapVirtualKeyW(sc, MAPVK_VSC_TO_VK_EX);
        return mapped ? static_cast<int>(mapped) : VK_LSHIFT;
    }
    if (wParam == VK_CONTROL) {
        return (lParam & (1LL << 24)) ? VK_RCONTROL : VK_LCONTROL;
    }
    if (wParam == VK_MENU) {
        return (lParam & (1LL << 24)) ? VK_RMENU : VK_LMENU;
    }
    return static_cast<int>(wParam);
}

void ProcessPendingHotkeys() {
    int bindVk = g_PendingBindKey.exchange(0);
    if (bindVk != 0) {
        g_UI.HandleKeyCapture(bindVk);
        SaveConfigToFile(g_Config);
    }

    std::vector<int> keys;
    {
        std::lock_guard<std::mutex> lock(g_KeyQueueMtx);
        keys.swap(g_KeyQueue);
    }
    if (keys.empty()) return;

    bool changed = false;
    auto checkToggle = [&](int bind, bool& enabled) {
        if (bind > 0) {
            for (int vk : keys) {
                if (vk == bind) {
                    enabled = !enabled;
                    changed = true;
                }
            }
        }
    };

    checkToggle(g_Config.espBind, g_Config.espEnabled);
    checkToggle(g_Config.tracersBind, g_Config.tracersEnabled);
    checkToggle(g_Config.targetHudBind, g_Config.targetHudEnabled);
    checkToggle(g_Config.crosshairRingBind, g_Config.crosshairRingEnabled);
    checkToggle(g_Config.fullbrightBind, g_Config.fullbrightEnabled);
    checkToggle(g_Config.mobTriggerBind, g_Config.mobTriggerEnabled);
    checkToggle(g_Config.autoFishBind, g_Config.autoFishEnabled);
    checkToggle(g_Config.autoSprintBind, g_Config.autoSprintEnabled);
    checkToggle(g_Config.autoEatBind, g_Config.autoEatEnabled);
    checkToggle(g_Config.triggerBotBind, g_Config.triggerBotEnabled);
    checkToggle(g_Config.aimAssistBind, g_Config.aimAssistEnabled);
    checkToggle(g_Config.sprintResetBind, g_Config.sprintResetEnabled);
    checkToggle(g_Config.jumpResetBind, g_Config.jumpResetEnabled);

    if (changed) {
        SaveConfigToFile(g_Config);
    }
}

LRESULT CALLBACK hkWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (g_ShuttingDown.load()) {
        return CallWindowProcW(oWndProc, hwnd, msg, wParam, lParam);
    }

    // Handle R-Shift & module hotkeys / keybind capture
    if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
        bool wasDown = (lParam & (1LL << 30)) != 0;
        if (!wasDown) {
            int vk = TranslateVirtualKeyEx(wParam, lParam);

            if (g_MenuOpen.load() && g_UI.IsCapturingBind()) {
                g_PendingBindKey.store(vk);
                return 0;
            }

            if (vk == VK_RSHIFT) {
                bool next = !g_MenuOpen.load();
                g_MenuOpen.store(next);
                return 0;
            }

            if (g_MenuOpen.load() && vk == VK_ESCAPE) {
                g_MenuOpen.store(false);
                return 0;
            }

            if (!g_MenuOpen.load() && !g_InMcScreen.load()) {
                std::lock_guard<std::mutex> lock(g_KeyQueueMtx);
                g_KeyQueue.push_back(vk);
            }
        }
    } else if (msg == WM_XBUTTONDOWN) {
        int xbtn = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? VK_XBUTTON1 : VK_XBUTTON2;
        if (g_MenuOpen.load() && g_UI.IsCapturingBind()) {
            g_PendingBindKey.store(xbtn);
            return 0;
        }
        if (!g_MenuOpen.load() && !g_InMcScreen.load()) {
            std::lock_guard<std::mutex> lock(g_KeyQueueMtx);
            g_KeyQueue.push_back(xbtn);
        }
    }

    bool open = g_MenuOpen.load();
    if (open && g_ImGuiInit) {
        ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);

        if (msg == WM_SETCURSOR) {
            SetCursor(nullptr);
            return TRUE;
        }

        switch (msg) {
            case WM_INPUT:
            case WM_MOUSEMOVE:
            case WM_LBUTTONDOWN:
            case WM_LBUTTONDBLCLK:
            case WM_RBUTTONDOWN:
            case WM_RBUTTONDBLCLK:
            case WM_MBUTTONDOWN:
            case WM_XBUTTONDOWN:
            case WM_MOUSEWHEEL:
            case WM_MOUSEHWHEEL:
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
            case WM_CHAR:
                return 0;
            default:
                break;
        }
    }

    return CallWindowProcW(oWndProc, hwnd, msg, wParam, lParam);
}

BOOL WINAPI hkSetCursorPos(int X, int Y) {
    if (g_MenuOpen.load()) {
        return TRUE;
    }
    return oSetCursorPos(X, Y);
}

BOOL WINAPI hkClipCursor(const RECT* lpRect) {
    if (lpRect) {
        g_SavedClip = *lpRect;
        g_HasSavedClip = true;
    } else {
        g_HasSavedClip = false;
    }
    if (g_MenuOpen.load()) {
        return oClipCursor(nullptr);
    }
    return oClipCursor(lpRect);
}

BOOL WINAPI hkWglSwapBuffers(HDC hdc) {
    if (g_ShuttingDown.load()) {
        return oWglSwapBuffers(hdc);
    }

    HWND dcWin = WindowFromDC(hdc);
    if (dcWin && !g_ImGuiInit) {
        if (!g_Hwnd) g_Hwnd = dcWin;
        LoadConfigFromFile(g_Config);

        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;

        g_UI.InitFonts(io);
        ImGui_ImplWin32_InitForOpenGL(g_Hwnd);
        ImGui_ImplOpenGL3_Init("#version 150");

        if (!oWndProc) {
            oWndProc = reinterpret_cast<WNDPROC>(
                SetWindowLongPtrW(g_Hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(hkWndProc)));
        }

        g_Bridge.Init();
        g_ImGuiInit = true;
    }

    if (g_ImGuiInit) {
        ProcessPendingHotkeys();

        bool menuOpen = g_MenuOpen.load();
        bool requestSave = false;

        g_Bridge.WithFrame([&](const FrameSnapshot& snap, const FrameHandles& handles) {
            g_InMcScreen.store(snap.inScreen);

            // Sync cursor lock cleanly when toggling menu
            if (menuOpen != g_PrevMenuOpen) {
                if (menuOpen) {
                    if (oClipCursor) oClipCursor(nullptr);
                    if (snap.ready && snap.isCursorLocked) {
                        g_Bridge.UnlockCursor();
                    }
                    // Place cursor at window center on first open
                    RECT rc{};
                    if (GetClientRect(g_Hwnd, &rc)) {
                        POINT pt{ (rc.right - rc.left) / 2, (rc.bottom - rc.top) / 2 };
                        ClientToScreen(g_Hwnd, &pt);
                        oSetCursorPos(pt.x, pt.y);
                    }
                } else {
                    if (snap.ready && snap.inWorld && !snap.inScreen) {
                        g_Bridge.LockCursor();
                    }
                }
                g_PrevMenuOpen = menuOpen;
            }

            // Ensure OS clip stays cleared while menu is open
            if (menuOpen && oClipCursor) {
                oClipCursor(nullptr);
            }

            // Disable sRGB framebuffer conversion during ImGui pass so dark colors match accurately
            constexpr GLenum kGlFramebufferSrgb = 0x8DB9;
            GLboolean srgbEnabled = glIsEnabled(kGlFramebufferSrgb);
            if (srgbEnabled) glDisable(kGlFramebufferSrgb);

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            ImGuiIO& io = ImGui::GetIO();
            io.MouseDrawCursor = menuOpen;

            g_Modules.Tick(g_Bridge, g_Config, snap, handles, menuOpen, io.DeltaTime);
            g_UI.Render(g_Config, snap, g_Modules, g_Bridge.StatusText(), menuOpen, requestSave);

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            if (srgbEnabled) glEnable(kGlFramebufferSrgb);
        });

        if (requestSave) {
            SaveConfigToFile(g_Config);
        }
    }

    return oWglSwapBuffers(hdc);
}

} // namespace

bool Install(HMODULE hSelfModule) {
    g_SelfModule = hSelfModule;

    // Wait briefly for GLFW window & opengl32.dll if injected right after startup
    for (int i = 0; i < 40 && !g_Hwnd; ++i) {
        g_Hwnd = FindOwnMinecraftWindow();
        if (!g_Hwnd) Sleep(100);
    }

    HMODULE hGl = nullptr;
    for (int i = 0; i < 40 && !hGl; ++i) {
        hGl = GetModuleHandleW(L"opengl32.dll");
        if (!hGl) Sleep(100);
    }
    if (!hGl) return false;

    void* pSwap = reinterpret_cast<void*>(GetProcAddress(hGl, "wglSwapBuffers"));
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    void* pSetCursorPos = hUser32 ? reinterpret_cast<void*>(GetProcAddress(hUser32, "SetCursorPos")) : nullptr;
    void* pClipCursor   = hUser32 ? reinterpret_cast<void*>(GetProcAddress(hUser32, "ClipCursor")) : nullptr;

    if (!pSwap) return false;

    if (MH_Initialize() != MH_OK) return false;

    if (MH_CreateHook(pSwap, reinterpret_cast<void*>(hkWglSwapBuffers),
                      reinterpret_cast<void**>(&oWglSwapBuffers)) != MH_OK) {
        return false;
    }
    if (pSetCursorPos) {
        MH_CreateHook(pSetCursorPos, reinterpret_cast<void*>(hkSetCursorPos),
                      reinterpret_cast<void**>(&oSetCursorPos));
    }
    if (pClipCursor) {
        MH_CreateHook(pClipCursor, reinterpret_cast<void*>(hkClipCursor),
                      reinterpret_cast<void**>(&oClipCursor));
    }

    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        return false;
    }

    return true;
}

void Uninstall() {
    g_ShuttingDown.store(true);
    Sleep(50);

    if (g_Hwnd && oWndProc) {
        SetWindowLongPtrW(g_Hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(oWndProc));
        oWndProc = nullptr;
    }

    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();

    g_Bridge.Shutdown();

    if (g_ImGuiInit) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        g_ImGuiInit = false;
    }
}

} // namespace meow::hooks
