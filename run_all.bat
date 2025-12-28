@echo off
setlocal

REM ==========================================
REM CONFIGURATION
REM ==========================================
set "RACE_SRC=10936972_Park_Tasks1to3_v4.cpp"
set "RACE_EXE=RaceApp.exe"
set "MONITOR_SRC=10936972_Park_Task4_v3.cpp"
set "MONITOR_EXE=MonitorApp.exe"

REM Add Compiler to Path (Vital for your setup)
set PATH=C:\msys64\ucrt64\bin;%PATH%

echo ========================================================
echo  STEP 0: Cleanup Old Processes
echo ========================================================
REM Kill any old stuck versions to prevent "Bind" errors
taskkill /F /IM %RACE_EXE% >nul 2>&1
taskkill /F /IM %MONITOR_EXE% >nul 2>&1

echo ========================================================
echo  STEP 1: Compiling Monitor (Task 4)...
echo ========================================================
if not exist %MONITOR_SRC% (
    echo [ERROR] Could not find %MONITOR_SRC%
    pause
    exit /b
)

g++ -static -std=c++20 -DASIO_STANDALONE -D_WIN32_WINNT=0x0A00 -I. %MONITOR_SRC% -o %MONITOR_EXE% -lws2_32 -lmswsock

if not exist %MONITOR_EXE% (
    echo [ERROR] Monitor Compilation Failed!
    pause
    exit /b
)
echo [OK] Monitor Compiled.

echo.
echo ========================================================
echo  STEP 2: Compiling Race (Tasks 1-3)...
echo ========================================================
if not exist %RACE_SRC% (
    echo [ERROR] Could not find %RACE_SRC%
    pause
    exit /b
)

g++ -static -std=c++20 -DASIO_STANDALONE -D_WIN32_WINNT=0x0A00 -I. %RACE_SRC% -o %RACE_EXE% -lws2_32 -lmswsock

if not exist %RACE_EXE% (
    echo [ERROR] Race Compilation Failed!
    pause
    exit /b
)
echo [OK] Race Compiled.

echo.
echo ========================================================
echo  STEP 3: Running Simulation
echo ========================================================

REM 1. Start the Monitor in a NEW separate window
echo Launching Monitor...
start "Race Monitor Dashboard" %MONITOR_EXE%

REM 2. Wait 2 seconds to ensure Monitor is ready to listen
timeout /t 2 /nobreak >nul

REM 3. Run the Race in THIS window
echo Launching Race...
echo.
%RACE_EXE%

echo.
echo ========================================================
echo  STEP 4: Cleanup
echo ========================================================
echo Deleting executable files...
del %RACE_EXE%
del %MONITOR_EXE%

echo.
echo [DONE] Simulation finished and files cleaned up.
pause