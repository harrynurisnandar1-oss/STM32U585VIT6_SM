@echo off
setlocal
echo ========================================================
echo   Building and Running Ultra-light DLMS Simulator...
echo ========================================================

set REPO_ROOT=%~dp0..

echo [1/2] Building micro-container (< 10 MB)...
docker compose -f "%REPO_ROOT%\compose.yaml" build --pull
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Docker build failed!
    exit /b %ERRORLEVEL%
)

echo [2/2] Starting DLMS Simulator on port 4059...
docker compose -f "%REPO_ROOT%\compose.yaml" up -d
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Failed to start container!
    exit /b %ERRORLEVEL%
)

echo.
echo ========================================================
echo  [SUCCESS] Simulator aktif di background!
echo  Port TCP: 127.0.0.1:4059
echo.
echo  Anda dapat langsung menghubungkan GXDLMSDirector dari Windows:
echo   - Media: Network / TCP/IP
echo   - Host: 127.0.0.1
echo   - Port: 4059
echo   - Interface: HDLC
echo.
echo  Untuk menghentikan: docker compose down
echo ========================================================
