@echo off
setlocal
echo ========================================================
echo   Building and Running AMI Docker Test Environment...
echo ========================================================

set REPO_ROOT=%~dp0..
docker compose -f "%REPO_ROOT%\compose.yaml" up --build --abort-on-container-exit
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Docker test execution failed!
    exit /b %ERRORLEVEL%
)

echo [SUCCESS] Docker tests completed successfully!
