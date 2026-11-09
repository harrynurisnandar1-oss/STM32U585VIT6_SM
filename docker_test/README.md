# Docker Host Test Harness (Single Unified Container: stm32u585vit6sm)

Direktori ini menyediakan script pendukung untuk pengujian container Docker tunggal (**stm32u585vit6sm**) yang didefinisikan pada [`Dockerfile`](../Dockerfile) dan diorkestrasi via [`compose.yaml`](../compose.yaml) di root repository. Seluruh pengujian logika host, 15 test suite CTest, dan uji end-to-end (E2E) DLMS/COSEM HDLC-over-TCP dijalankan dalam 1 container mandiri tanpa memerlukan toolchain embedded STM32 atau hardware fisik.

## Cakupan Pengujian

Container Docker dibangun menggunakan multi-stage build Alpine Linux yang aman, terisolasi, dan sangat ringan:
1. **15 CTest Unit Test Suites** (100% CTest suites dari `tests/CMakeLists.txt`):
   - `test_dlms_port`
   - `test_cosem`
   - `test_security`
   - `test_association`
   - `test_apdu`
   - `test_hdlc`
   - `test_auth_policy`
   - `test_sha256_mock`
   - `test_security_nsc_mock`
   - `test_dlms_pipeline`
   - `test_cosem_compliance`
   - `test_load_profile_capture`
   - `test_tamper_log`
   - `test_disconnect_control`
   - `test_ami_trustzone_boundary` (Validasi batas memori CMSE NSC & overflow pointer)
2. **E2E DLMS Simulator & Client Integration**:
   - `gurux_simulator` (HDLC-over-TCP listener port 4059).
   - `test_dlms_e2e_client.py` (SNRM/UA, AARQ/AARE, GET Request/Response).

## Cara Menjalankan

### 1. Menggunakan Docker Compose (Direkomendasikan)
Dari root repository:
```bash
docker compose up --build --abort-on-container-exit
```

### 2. Build & Run Langsung via Docker CLI
Dari root repository:
```bash
docker build -t stm32u585vit6sm .
docker run --rm -p 4059:4059 stm32u585vit6sm
```

### 3. Menggunakan Script Helper Windows
```powershell
.\docker_test\run_docker_tests.bat
```

## Batas Tanggung Jawab

Container ini memverifikasi logika protokol DLMS/COSEM, state machine asosiasi, parsing APDU, capture buffer load profile, tamper events, serta simulasi HDLC-over-TCP.
Pengujian akurasi metrologi energi nyata, interrupt hardware, driver UART fisik, dan Secure Engine HW (TrustZone / SAU) diverifikasi langsung pada hardware unit STM32U585.
