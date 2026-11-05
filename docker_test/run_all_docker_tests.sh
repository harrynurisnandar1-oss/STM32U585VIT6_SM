#!/usr/bin/env bash
set -e

echo "========================================================"
echo " [DOCKER] Smart Meter Single Unified Test Container"
echo "========================================================"

# Step 1: Execute all CTest unit test suites (14/14 suites)
echo ""
echo ">>> [1/2] Menjalankan Unit Test Suites (CTest)..."
ctest --test-dir build --output-on-failure --timeout 30

echo ""
echo ">>> Unit Test Berhasil: 100% CTest suites PASS!"

# Step 2: Launch DLMS Simulator in background and run E2E Python verification
echo ""
echo ">>> [2/2] Memulai Gurux DLMS Simulator & E2E Integration Test..."

# Check simulator binary location
if [ -f "./build/host-tools/gurux_simulator" ]; then
    SIM_BIN="./build/host-tools/gurux_simulator"
elif [ -f "./build/gurux_simulator" ]; then
    SIM_BIN="./build/gurux_simulator"
else
    echo "ERROR: gurux_simulator binary tidak ditemukan di ./build/host-tools atau ./build/!"
    exit 1
fi

$SIM_BIN 4059 &
SIM_PID=$!
echo "Simulator berjalan di background (PID: $SIM_PID, Port: 4059)"

# Berikan waktu 1-2 detik untuk socket TCP listening
sleep 2

# Cleanup handler ketika script exit
cleanup() {
    echo ""
    echo "Membersihkan background simulator (PID: $SIM_PID)..."
    kill $SIM_PID 2>/dev/null || true
}
trap cleanup EXIT

# Eksekusi DLMS E2E Python Client
python3 test_dlms_e2e_client.py

echo ""
echo "========================================================"
echo " [SUCCESS] Seluruh Unit Test & E2E Simulator Lulus!"
echo "========================================================"

