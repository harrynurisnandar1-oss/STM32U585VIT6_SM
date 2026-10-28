#!/usr/bin/env bash
set -e

echo "=========================================================="
echo "      AMI SMART METER - DOCKER TEST SUITE RUNNER         "
echo "=========================================================="

BUILD_DIR="build-docker"
mkdir -p "$BUILD_DIR"

if command -v cmake &> /dev/null && [ ! -f "$BUILD_DIR/test_ami_port" ]; then
    echo "[1/3] Configuring & Building tests with CMake..."
    cmake -S . -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build "$BUILD_DIR" --parallel
else
    echo "[1/3] Using pre-built test binaries in $BUILD_DIR..."
fi

echo ""
echo "[2/3] Running unit test suites (Port, Cosem, Security, Association, APDU, HDLC)..."
TEST_SUITES=(
    "test_ami_port"
    "test_ami_cosem"
    "test_ami_security"
    "test_ami_dlms_association"
    "test_ami_dlms_apdu"
    "test_ami_dlms_hdlc"
)

for suite in "${TEST_SUITES[@]}"; do
    if [ -x "$BUILD_DIR/$suite" ]; then
        echo "  [RUN] $suite"
        "$BUILD_DIR/$suite"
        echo "  [PASS] $suite passed."
    else
        echo "  [ERROR] Executable $BUILD_DIR/$suite not found!"
        exit 1
    fi
done

echo ""
echo "[3/3] Running End-to-End DLMS/COSEM HDLC-over-TCP integration test..."
# Start simulator in background
"$BUILD_DIR/gurux_simulator" &
SIM_PID=$!
trap "kill -9 $SIM_PID 2>/dev/null || true" EXIT

# Give simulator a moment to bind port 4059
sleep 1

# Run automated DLMS client test
python3 test_dlms_e2e_client.py

# Terminate simulator cleanly
kill $SIM_PID 2>/dev/null || true
trap - EXIT

echo ""
echo "=========================================================="
echo "  >>> ALL DOCKER UNIT & E2E TESTS PASSED SUCCESSFULLY <<< "
echo "=========================================================="

