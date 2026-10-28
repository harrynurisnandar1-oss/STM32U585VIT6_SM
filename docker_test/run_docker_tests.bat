@echo off
setlocal
echo ========================================================
echo   Building and Running AMI Docker Test Environment...
echo ========================================================

cd ..
docker build -t ami-smart-meter-test -f docker_test/Dockerfile .
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Docker build failed!
    exit /b %ERRORLEVEL%
)

docker run --rm ami-smart-meter-test
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Docker test execution failed!
    exit /b %ERRORLEVEL%
)

echo [SUCCESS] Docker tests completed successfully!
cd docker_test

