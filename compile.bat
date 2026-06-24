@echo off
cd /d "%~dp0"

echo ===== Compiling Tank Defender =====
echo.

C:\msys64\mingw64\bin\g++.exe -std=c++17 -o TankDefender.exe ^
    src/main.cpp src/Game.cpp src/Player.cpp ^
    src/Enemy_Bullet_Turret.cpp ^
    src/Renderer.cpp ^
    -I"C:/msys64/mingw64/include" ^
    -I"C:/msys64/mingw64/include/GL" ^
    -L"C:/msys64/mingw64/lib" ^
    -lglew32 -lglfw3 -lopengl32 -lgdi32 -lm

echo.
echo ===== COMPILE SELESAI! =====
echo File: TankDefender.exe
pause