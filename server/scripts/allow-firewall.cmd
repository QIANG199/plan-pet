@echo off
rem Optional port argument, defaults to 3737 (the PORT in .env).
setlocal
set PORT=%1
if "%PORT%"=="" set PORT=3737
netsh advfirewall firewall add rule name="desktop-pet-%PORT%" dir=in action=allow protocol=TCP localport=%PORT%
if errorlevel 1 (
  echo Failed. Run this file as Administrator.
  exit /b 1
)
echo Allowed inbound TCP %PORT%.
