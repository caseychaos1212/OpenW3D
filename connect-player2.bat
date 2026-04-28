@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
set "BUILD_DIR=%SCRIPT_DIR%"
if not exist "%BUILD_DIR%renegade.exe" set "BUILD_DIR=%SCRIPT_DIR%build\win\Release\"
if not exist "%BUILD_DIR%renegade.exe" set "BUILD_DIR=C:\Users\admin\source\repos\OpenW3D\build\win\Release\"

set "RENEGADE_ROOT=C:\Program Files (x86)\Steam\steamapps\common\Command & Conquer Renegade"
set "CONFIG_PATH=%BUILD_DIR%openw3d.conf"
set "GAME_EXE=%BUILD_DIR%renegade.exe"
set "CONFIG_EXE=%BUILD_DIR%wwconfig.exe"

if not exist "%GAME_EXE%" goto missing_game
if not exist "%CONFIG_EXE%" goto missing_config
if not exist "%RENEGADE_ROOT%" goto missing_renegade

if not exist "%CONFIG_PATH%" (
	"%CONFIG_EXE%" --ini "%CONFIG_PATH%"
	if errorlevel 1 exit /b 1
)

pushd "%BUILD_DIR%" || exit /b 1
start "Renegade Player 2" "%GAME_EXE%" --gamedir "%RENEGADE_ROOT%" --ini "%CONFIG_PATH%" --multi --coop-connect 192.168.2.50 --gamespy-netplayername ChAoS2
popd
exit /b 0

:missing_game
echo Missing game executable:
echo "%GAME_EXE%"
exit /b 1

:missing_config
echo Missing config executable:
echo "%CONFIG_EXE%"
exit /b 1

:missing_renegade
echo Missing Renegade folder:
echo "%RENEGADE_ROOT%"
exit /b 1
