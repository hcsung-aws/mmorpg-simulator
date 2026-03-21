@echo off
echo Building MMORPG Simulator...

if not exist "build" mkdir build

echo.
echo [1/2] Building GameServer...
"C:\Program Files\Microsoft Visual Studio\18\Community\SDK\ScopeCppSDK\vc15\VC\bin\cl.exe" /EHsc /std:c++17 /I. GameServer\main.cpp GameServer\TcpServer.cpp /Fe:build\GameServer.exe ws2_32.lib /Fo:build\
if %errorlevel% neq 0 (
    echo GameServer build failed!
    pause
    exit /b 1
)

echo.
echo [2/2] Building GameClient...
"C:\Program Files\Microsoft Visual Studio\18\Community\SDK\ScopeCppSDK\vc15\VC\bin\cl.exe" /EHsc /std:c++17 /I. GameClient\main.cpp GameClient\TcpClient.cpp /Fe:build\GameClient.exe ws2_32.lib /Fo:build\
if %errorlevel% neq 0 (
    echo GameClient build failed!
    pause
    exit /b 1
)

echo.
echo Build complete!
echo Run: build\GameServer.exe (first)
echo Run: build\GameClient.exe (second)
