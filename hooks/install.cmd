@echo off
rem Quick repair for PlanPet Cursor/ZCode hooks.
rem Cursor upgrades sometimes wipe ~/.cursor/hooks.json — double-click this, or:
rem   hooks\install.cmd
rem   hooks\install.cmd status
setlocal EnableExtensions
cd /d "%~dp0.."
where node >nul 2>&1
if errorlevel 1 (
  echo node.exe is not in PATH
  if "%~1"=="" pause
  exit /b 1
)
node hooks\install.js %*
set ERR=%ERRORLEVEL%
if "%~1"=="" pause
exit /b %ERR%
