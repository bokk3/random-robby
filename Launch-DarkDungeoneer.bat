@echo off
setlocal
set "PROJECT_ROOT=%~dp0"
set "STAGED_EXE=%PROJECT_ROOT%Saved\StagedBuilds\Windows\DarkDungeoneer.exe"
set "STANDALONE_EXE=%PROJECT_ROOT%Binaries\Win64\DarkDungeoneer.exe"
set "COOKED_DIR=%PROJECT_ROOT%Saved\Cooked\Windows"
set "UE_EDITOR=D:\epuc\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"

if exist "%STAGED_EXE%" (
    start "" "%STAGED_EXE%" -Windowed -ResX=1280 -ResY=720 %*
    exit /b 0
)

if exist "%STANDALONE_EXE%" if exist "%COOKED_DIR%" (
    start "" "%STANDALONE_EXE%" -Windowed -ResX=1280 -ResY=720 %*
    exit /b 0
)

if exist "%UE_EDITOR%" (
    start "" "%UE_EDITOR%" "%PROJECT_ROOT%DarkDungeoneer.uproject" -game -Windowed -ResX=1280 -ResY=720 %*
    exit /b 0
)

echo [DarkDungeoneer] Unable to locate UE 5.8 UnrealEditor.exe at "%UE_EDITOR%".
exit /b 1
