@echo off
netsh advfirewall firewall add rule name="desktop-pet-3737" dir=in action=allow protocol=TCP localport=3737
if errorlevel 1 (
  echo Failed. Run this file as Administrator.
  exit /b 1
)
echo Allowed inbound TCP 3737.
