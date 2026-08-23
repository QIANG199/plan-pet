@echo off
setlocal EnableExtensions
cd /d "%~dp0.."
where node >nul 2>&1
if errorlevel 1 (
  echo node.exe is not in PATH
  exit /b 1
)
for /f "delims=" %%i in ('where node') do (
  set "NODE=%%i"
  goto :got
)
:got
powershell -NoProfile -Command ^
  "$node = $env:NODE; $wd = (Resolve-Path '.').Path; $s = [Environment]::GetFolderPath('Startup'); $lnk = Join-Path $s 'plan-pet-relay.lnk'; $w = New-Object -ComObject WScript.Shell; $l = $w.CreateShortcut($lnk); $l.TargetPath = $node; $l.Arguments = 'src/index.js'; $l.WorkingDirectory = $wd; $l.WindowStyle = 7; $l.Save(); Write-Host \"startup shortcut: $lnk\""
if errorlevel 1 exit /b 1
echo Relay will start minimized at Windows logon.
echo Remove the shortcut from the Startup folder to undo.
