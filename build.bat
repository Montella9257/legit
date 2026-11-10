@echo off
setlocal EnableExtensions
chcp 65001 >nul

set "ROOT=%~dp0"
if not exist "%ROOT%bin" mkdir "%ROOT%bin"
if not exist "%ROOT%build_tmp" mkdir "%ROOT%build_tmp"

where zig >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo [*] Building with Zig C++...
    zig cc -O2 -I"%ROOT%third_party\minhook\include" -I"%ROOT%third_party\minhook\src" -c "%ROOT%third_party\minhook\src\buffer.c" -o "%ROOT%build_tmp\buffer.o" || exit /b 1
    zig cc -O2 -I"%ROOT%third_party\minhook\include" -I"%ROOT%third_party\minhook\src" -c "%ROOT%third_party\minhook\src\hook.c" -o "%ROOT%build_tmp\hook.o" || exit /b 1
    zig cc -O2 -I"%ROOT%third_party\minhook\include" -I"%ROOT%third_party\minhook\src" -c "%ROOT%third_party\minhook\src\trampoline.c" -o "%ROOT%build_tmp\trampoline.o" || exit /b 1
    zig cc -O2 -I"%ROOT%third_party\minhook\include" -I"%ROOT%third_party\minhook\src" -c "%ROOT%third_party\minhook\src\hde\hde64.c" -o "%ROOT%build_tmp\hde64.o" || exit /b 1

    zig c++ -std=c++20 -O2 -shared -s -DWIN32_LEAN_AND_MEAN -DNOMINMAX -DIMGUI_IMPL_WIN32_DISABLE_GAMEPAD ^
        -I"%ROOT%third_party\imgui" -I"%ROOT%third_party\imgui\backends" ^
        -I"%ROOT%third_party\minhook\include" -I"%ROOT%third_party\jni" -I"%ROOT%third_party\json" ^
        "%ROOT%build_tmp\buffer.o" "%ROOT%build_tmp\hook.o" "%ROOT%build_tmp\trampoline.o" "%ROOT%build_tmp\hde64.o" ^
        "%ROOT%third_party\imgui\imgui.cpp" "%ROOT%third_party\imgui\imgui_draw.cpp" ^
        "%ROOT%third_party\imgui\imgui_tables.cpp" "%ROOT%third_party\imgui\imgui_widgets.cpp" ^
        "%ROOT%third_party\imgui\backends\imgui_impl_win32.cpp" "%ROOT%third_party\imgui\backends\imgui_impl_opengl3.cpp" ^
        "%ROOT%src\dll\embedded_fonts.cpp" "%ROOT%src\dll\jvm_bridge.cpp" ^
        "%ROOT%src\dll\modules.cpp" "%ROOT%src\dll\ui.cpp" ^
        "%ROOT%src\dll\hooks.cpp" "%ROOT%src\dll\main.cpp" ^
        -o "%ROOT%bin\Meow.dll" -lopengl32 -lgdi32 -luser32 -lkernel32 -ldwmapi || exit /b 1

    zig c++ -std=c++20 -O2 -static -s -DWIN32_LEAN_AND_MEAN -DNOMINMAX ^
        "%ROOT%src\injector\main.cpp" -o "%ROOT%bin\MeowInjector.exe" -luser32 -lkernel32 -lshell32 -lpsapi || exit /b 1
    echo [OK] Built bin\Meow.dll and bin\MeowInjector.exe
    exit /b 0
)

where g++ >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo [*] Building with MinGW g++...
    gcc -O2 -I"%ROOT%third_party\minhook\include" -I"%ROOT%third_party\minhook\src" -c "%ROOT%third_party\minhook\src\buffer.c" -o "%ROOT%build_tmp\buffer.o" || exit /b 1
    gcc -O2 -I"%ROOT%third_party\minhook\include" -I"%ROOT%third_party\minhook\src" -c "%ROOT%third_party\minhook\src\hook.c" -o "%ROOT%build_tmp\hook.o" || exit /b 1
    gcc -O2 -I"%ROOT%third_party\minhook\include" -I"%ROOT%third_party\minhook\src" -c "%ROOT%third_party\minhook\src\trampoline.c" -o "%ROOT%build_tmp\trampoline.o" || exit /b 1
    gcc -O2 -I"%ROOT%third_party\minhook\include" -I"%ROOT%third_party\minhook\src" -c "%ROOT%third_party\minhook\src\hde\hde64.c" -o "%ROOT%build_tmp\hde64.o" || exit /b 1

    g++ -std=c++20 -O2 -shared -static-libgcc -static-libstdc++ -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic -s -DWIN32_LEAN_AND_MEAN -DNOMINMAX -DIMGUI_IMPL_WIN32_DISABLE_GAMEPAD ^
        -I"%ROOT%third_party\imgui" -I"%ROOT%third_party\imgui\backends" ^
        -I"%ROOT%third_party\minhook\include" -I"%ROOT%third_party\jni" -I"%ROOT%third_party\json" ^
        "%ROOT%build_tmp\buffer.o" "%ROOT%build_tmp\hook.o" "%ROOT%build_tmp\trampoline.o" "%ROOT%build_tmp\hde64.o" ^
        "%ROOT%third_party\imgui\imgui.cpp" "%ROOT%third_party\imgui\imgui_draw.cpp" ^
        "%ROOT%third_party\imgui\imgui_tables.cpp" "%ROOT%third_party\imgui\imgui_widgets.cpp" ^
        "%ROOT%third_party\imgui\backends\imgui_impl_win32.cpp" "%ROOT%third_party\imgui\backends\imgui_impl_opengl3.cpp" ^
        "%ROOT%src\dll\embedded_fonts.cpp" "%ROOT%src\dll\jvm_bridge.cpp" ^
        "%ROOT%src\dll\modules.cpp" "%ROOT%src\dll\ui.cpp" ^
        "%ROOT%src\dll\hooks.cpp" "%ROOT%src\dll\main.cpp" ^
        -o "%ROOT%bin\Meow.dll" -lopengl32 -lgdi32 -luser32 -lkernel32 -ldwmapi || exit /b 1

    g++ -std=c++20 -O2 -static -s -DWIN32_LEAN_AND_MEAN -DNOMINMAX ^
        "%ROOT%src\injector\main.cpp" -o "%ROOT%bin\MeowInjector.exe" -luser32 -lkernel32 -lshell32 -lpsapi || exit /b 1
    echo [OK] Built bin\Meow.dll and bin\MeowInjector.exe
    exit /b 0
)

echo [!] Neither zig nor MinGW g++ found in PATH.
echo     Prebuilt binaries should already be in bin\Meow.dll and bin\MeowInjector.exe.
exit /b 1
