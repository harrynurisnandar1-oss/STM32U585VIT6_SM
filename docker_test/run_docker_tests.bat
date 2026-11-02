@echo off
setlocal
echo ========================================================
echo   Building and Running AMI Docker Test Environment...
echo ========================================================

set SCRIPT_DIR=%~dp0
docker compose -f "%SCRIPT_DIR%docker-compose.yml" up --build --abort-on-container-exit
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Docker test execution failed!
    exit /b %ERRORLEVEL%
)

echo [SUCCESS] Docker tests completed successfully!
