@echo off
echo ===============================================================
echo   Compiling NaviCore Navigation Engine (C++14 / MinGW GCC)
echo ===============================================================
cd /d "%~dp0"
g++ -std=c++14 -O2 -Iinclude src/Graph.cpp src/PathFinder.cpp src/NavigationEngine.cpp src/main.cpp -o nav_engine.exe
if %ERRORLEVEL% EQU 0 (
    echo [SUCCESS] Binary compiled cleanly: nav_engine.exe
    echo Exporting updated JSON data for web visualization...
    nav_engine.exe --map campus --export-graph > web\campus_graph.json
    nav_engine.exe --map city --export-graph > web\city_graph.json
    copy /Y web\campus_graph.json web\graph_data.json >nul
    echo [SUCCESS] Web graph assets synced!
) else (
    echo [ERROR] Compilation failed. Please verify MinGW g++ is in your PATH.
)
pause
