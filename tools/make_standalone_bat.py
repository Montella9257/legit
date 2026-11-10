#!/usr/bin/env python3
import base64
import gzip
import hashlib
import os
import textwrap

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DLL_PATH = os.path.join(ROOT, "bin", "Meow.dll")
INJ_PATH = os.path.join(ROOT, "bin", "MeowInjector.exe")
OUT_BAT = os.path.join(ROOT, "inject.bat")


def b64_gz_lines(path: str) -> tuple[str, list[str]]:
    with open(path, "rb") as f:
        raw = f.read()
    sha = hashlib.sha256(raw).hexdigest().upper()
    gz = gzip.compress(raw, compresslevel=9, mtime=0)
    b64 = base64.b64encode(gz).decode("ascii")
    return sha, textwrap.wrap(b64, 120)


def main():
    inj_sha, inj_lines = b64_gz_lines(INJ_PATH)
    dll_sha, dll_lines = b64_gz_lines(DLL_PATH)

    ps_script = (
        "$ErrorActionPreference='Stop';"
        "$bat=$env:MEOW_BAT_SELF;"
        "$outDir=$env:MEOW_OUT_DIR;"
        "New-Item -ItemType Directory -Force -Path $outDir | Out-Null;"
        "$lines=[System.IO.File]::ReadAllLines($bat);"
        "function Unpack($s,$e,$p,$expected){"
        "if(Test-Path $p){"
        "$shaObj=[System.Security.Cryptography.SHA256]::Create();"
        "$cur=[BitConverter]::ToString($shaObj.ComputeHash([System.IO.File]::ReadAllBytes($p))).Replace('-','').ToUpper();"
        "$shaObj.Dispose();"
        "if($cur -eq $expected){return;}"
        "}"
        "$sb=New-Object System.Text.StringBuilder;"
        "$on=$false;"
        "foreach($l in $lines){"
        "if($l -eq $s){$on=$true;continue;}"
        "if($l -eq $e){break;}"
        "if($on){[void]$sb.Append($l.Trim());}"
        "}"
        "if($sb.Length -eq 0){throw ('Missing payload '+$s);}"
        "$gzb=[System.Convert]::FromBase64String($sb.ToString());"
        "$msIn=New-Object System.IO.MemoryStream(,$gzb);"
        "$gz=New-Object System.IO.Compression.GZipStream($msIn,[System.IO.Compression.CompressionMode]::Decompress);"
        "$msOut=New-Object System.IO.MemoryStream;"
        "$gz.CopyTo($msOut);"
        "$gz.Dispose();$msIn.Dispose();"
        "[System.IO.File]::WriteAllBytes($p,$msOut.ToArray());"
        "$msOut.Dispose();"
        "}"
        f"Unpack '===MEOW_INJECTOR_START===' '===MEOW_INJECTOR_END===' (Join-Path $outDir 'MeowInjector.exe') '{inj_sha}';"
        f"Unpack '===MEOW_DLL_START===' '===MEOW_DLL_END===' (Join-Path $outDir 'Meow.dll') '{dll_sha}';"
    )

    assert "%" not in ps_script and "!" not in ps_script and '"' not in ps_script

    header = f"""@echo off
setlocal EnableExtensions
chcp 65001 >nul
title MEOW.DLL // Minecraft 1.21.4 Fabric Legit Client

echo.
echo   ======================================================
echo     MEOW.DLL  ^|  Minecraft 1.21.4 Fabric Legit Client
echo   ======================================================
echo.

set "ROOT_DIR=%~dp0"
set "INJECTOR=%ROOT_DIR%bin\\MeowInjector.exe"
set "DLL_PATH=%ROOT_DIR%bin\\Meow.dll"

if exist "%INJECTOR%" if exist "%DLL_PATH%" goto :do_inject

if defined LOCALAPPDATA (
    set "CACHE_DIR=%LOCALAPPDATA%\\MeowClient\\bin"
) else (
    set "CACHE_DIR=%TEMP%\\MeowClient\\bin"
)
set "INJECTOR=%CACHE_DIR%\\MeowInjector.exe"
set "DLL_PATH=%CACHE_DIR%\\Meow.dll"

echo [*] Preparing standalone Meow.dll files...
set "MEOW_BAT_SELF=%~f0"
set "MEOW_OUT_DIR=%CACHE_DIR%"
powershell -NoProfile -ExecutionPolicy Bypass -Command "{ps_script}"
if errorlevel 1 (
    echo [!] Failed to unpack embedded payload.
    pause
    exit /b 1
)

:do_inject
"%INJECTOR%" "%DLL_PATH%"
set "RC=%ERRORLEVEL%"

echo.
if %RC% EQU 0 (
    echo   [OK] Menu key in Minecraft: R-SHIFT
    timeout /t 4 >nul
    exit /b 0
) else (
    echo   [!] Injection returned code %RC%.
    pause
    exit /b %RC%
)
goto :eof
"""

    lines = header.splitlines()
    lines.append("===MEOW_INJECTOR_START===")
    lines.extend(inj_lines)
    lines.append("===MEOW_INJECTOR_END===")
    lines.append("===MEOW_DLL_START===")
    lines.extend(dll_lines)
    lines.append("===MEOW_DLL_END===")

    content = "\r\n".join(lines) + "\r\n"
    with open(OUT_BAT, "wb") as f:
        f.write(content.encode("utf-8"))

    print(f"Generated standalone {OUT_BAT} ({len(content)/1024:.1f} KB)")
    print(f"  MeowInjector.exe SHA256: {inj_sha}")
    print(f"  Meow.dll         SHA256: {dll_sha}")


if __name__ == "__main__":
    main()
