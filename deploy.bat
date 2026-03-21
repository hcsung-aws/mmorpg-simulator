@echo off
set DEPLOY_DIR=deploy

echo Packaging to %DEPLOY_DIR%\...

if exist %DEPLOY_DIR% rmdir /s /q %DEPLOY_DIR%
mkdir %DEPLOY_DIR%
mkdir %DEPLOY_DIR%\data
mkdir %DEPLOY_DIR%\scripts

copy build\GameServer.exe %DEPLOY_DIR%\ >nul
copy build\GameClient.exe %DEPLOY_DIR%\ >nul
xcopy data %DEPLOY_DIR%\data /s /q >nul
xcopy scripts %DEPLOY_DIR%\scripts /s /q >nul

echo Done.
echo   %DEPLOY_DIR%\GameServer.exe
echo   %DEPLOY_DIR%\GameClient.exe
echo   %DEPLOY_DIR%\data\
echo   %DEPLOY_DIR%\scripts\  (DB setup)
