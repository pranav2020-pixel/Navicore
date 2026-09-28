@echo off
echo ===============================================================
echo   Starting NaviCore Web Bridge & Interactive UI
echo ===============================================================
cd /d "%~dp0"
start http://localhost:8000
python server.py
pause
