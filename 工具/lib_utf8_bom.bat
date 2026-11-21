@echo off
rem Add a UTF-8 BOM to the @response file before invoking MSVC lib.exe,
rem otherwise non-ASCII object names are misread under the ANSI code page.
setlocal enabledelayedexpansion
for %%A in (%*) do (
    set "ARG=%%~A"
    if "!ARG:~0,1!"=="@" (
        set "BYJY_RSP=!ARG:~1!"
        powershell -NoProfile -ExecutionPolicy Bypass -Command "[IO.File]::WriteAllText($env:BYJY_RSP, [IO.File]::ReadAllText($env:BYJY_RSP,[Text.Encoding]::UTF8), (New-Object Text.UTF8Encoding($true)))"
    )
)
"D:\ViusalStudioCommunity2026\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64\lib.exe" %*
exit /b %ERRORLEVEL%
