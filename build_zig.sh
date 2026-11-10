#!/usr/bin/env bash
set -euo pipefail

ZIG="${ZIG_BIN:-/tmp/zig-venv/bin/python-zig}"
if ! command -v "$ZIG" >/dev/null 2>&1; then
  if command -v zig >/dev/null 2>&1; then
    ZIG="zig"
  else
    echo "Error: Zig compiler not found (set ZIG_BIN or install zig)" >&2
    exit 1
  fi
fi

mkdir -p bin /tmp/meow-obj

echo "[1/3] Compiling MinHook (C, x86_64-windows-gnu)..."
MINHOOK_SRCS=(
  third_party/minhook/src/buffer.c
  third_party/minhook/src/hook.c
  third_party/minhook/src/trampoline.c
  third_party/minhook/src/hde/hde64.c
)
MINHOOK_OBJS=()
for src in "${MINHOOK_SRCS[@]}"; do
  obj="/tmp/meow-obj/$(basename "$src" .c).o"
  "$ZIG" cc -target x86_64-windows-gnu -O2 -Ithird_party/minhook/include -Ithird_party/minhook/src -c "$src" -o "$obj"
  MINHOOK_OBJS+=("$obj")
done

echo "[2/3] Compiling Meow.dll (C++20, static CRT/libcxx, x86_64-windows-gnu)..."
DLL_SRCS=(
  third_party/imgui/imgui.cpp
  third_party/imgui/imgui_draw.cpp
  third_party/imgui/imgui_tables.cpp
  third_party/imgui/imgui_widgets.cpp
  third_party/imgui/backends/imgui_impl_win32.cpp
  third_party/imgui/backends/imgui_impl_opengl3.cpp
  src/dll/embedded_fonts.cpp
  src/dll/jvm_bridge.cpp
  src/dll/modules.cpp
  src/dll/ui.cpp
  src/dll/hooks.cpp
  src/dll/main.cpp
)
DLL_OBJS=()
for src in "${DLL_SRCS[@]}"; do
  obj="/tmp/meow-obj/$(basename "$src" .cpp).o"
  "$ZIG" c++ -target x86_64-windows-gnu -std=c++20 -O2 \
    -DWIN32_LEAN_AND_MEAN -DNOMINMAX -DIMGUI_IMPL_WIN32_DISABLE_GAMEPAD \
    -Ithird_party/imgui -Ithird_party/imgui/backends \
    -Ithird_party/minhook/include -Ithird_party/jni -Ithird_party/json \
    -c "$src" -o "$obj"
  DLL_OBJS+=("$obj")
done

"$ZIG" c++ -target x86_64-windows-gnu -shared -s \
  -Wl,--out-implib,/tmp/meow-obj/Meow.lib \
  "${MINHOOK_OBJS[@]}" "${DLL_OBJS[@]}" \
  -o bin/Meow.dll \
  -lopengl32 -lgdi32 -luser32 -lkernel32 -ldwmapi

echo "[3/3] Compiling MeowInjector.exe (C++20, static CRT/libcxx, x86_64-windows-gnu)..."
"$ZIG" c++ -target x86_64-windows-gnu -std=c++20 -O2 -static -s \
  -DWIN32_LEAN_AND_MEAN -DNOMINMAX \
  src/injector/main.cpp \
  -o bin/MeowInjector.exe \
  -luser32 -lkernel32 -lshell32 -lpsapi

ls -lh bin/Meow.dll bin/MeowInjector.exe
echo "[4/4] Generating standalone inject.bat..."
python3 tools/make_standalone_bat.py
