# STM32U585VIT6 + ADE9000 3-Phase AMI Smart Meter

[![CTest](https://img.shields.io/badge/CTest%20Host-16%2F16%20Passing-brightgreen?style=flat-square&logo=cmake)](tests/)
[![Docker](https://img.shields.io/badge/Docker%20Simulator-Ready-blue?style=flat-square&logo=docker)](Dockerfile)
[![Coverage](https://img.shields.io/badge/Code%20Coverage-79.2%25%20(LCOV)-informational?style=flat-square&logo=codecov)](docs/COVERAGE.md)
[![Standards](https://img.shields.io/badge/Standards-IEC%2062056%20%7C%20SPLN%20D3.006%3A2021-orange?style=flat-square)](docs/security-rules/PROTOCOL_PROFILE.md)
[![Security](https://img.shields.io/badge/Security-Suite%200%20%7C%20HLS5%20GMAC-red?style=flat-square)](docs/security-rules/)
[![Architecture](https://img.shields.io/badge/Target-ARM%20Cortex--M33%20TrustZone-purple?style=flat-square)](Secure/)

Postpaid 3-phase Advanced Metering Infrastructure (AMI) smart meter firmware targeting the **STM32U585VIT6** microcontroller (ARM Cortex-M33 with TrustZone-M) and **ADE9000** polyphase metering analog front-end (AFE).

The codebase implements the **DLMS/COSEM** protocol stack (IEC 62056 / SPLN D3.006:2021), **DLMS Security Suite 0** with **HLS 5 (GMAC)** mutual authentication (SPLN D5.002:2024), TrustZone-isolated key management and Secure Boot verification, an ADE9000 SPI metrology driver, a 16-suite CTest host test harness, and an HDLC-over-TCP simulator for end-to-end verification.

---

## Overview

The E2 protocol and data-model layer is designed for deterministic interoperability with utility Head-End Systems (HES / MDMS) without dynamic heap allocation in the data-link and APDU paths:

- **Zero-heap streaming HDLC & xDLMS processing**: Fixed-buffer Type 3 HDLC framing (`0xA0`), dual-stage CRC-16 (HCS/FCS), LLC sublayer handling (`0xE6 0xE6 0x00` / `0xE6 0xE7 0x00`), and General Block Transfer (GBT) segmentation.
- **PLN COSEM Data Model**: 276 cataloged OBIS codes (`NonSecure/AMI/Inc/ami_obis_catalog.h`) and 66 active objects exposed in the Association View (`GET` with Selective Access by clock range or entry index).
- **Fail-closed TrustZone Security**: Cryptographic primitives, key wrapping, NVM anti-replay counters, and Secure Boot verification execute in the Secure world behind a Non-Secure Callable (NSC) gateway (`Secure/Core/Src/secure_nsc.c`).

---

## Architecture

### TrustZone Partitioning & Protocol Flow

<img width="743" height="667" alt="Workflow E3-TrustZone Partitioning   Protocol Flow drawio" src="https://github.com/user-attachments/assets/7b3eb689-ecae-4788-b342-8ae523ec353a" />

### Subsystems

- **Data Link (`ami_dlms_hdlc.c`)**: Zero-heap $O(1)$ streaming HDLC Type 3 parser supporting multi-octet addressing, HCS/FCS CRC-16 verification, back-to-back `0x7E` flag resilience, and LLC header wrapping/stripping.
- **Application & Association (`ami_dlms_association.c`, `ami_dlms_apdu.c`, `ami_dlms_block_transfer.c`)**:
  - ACSE `AARQ`/`AARE` state machine, Max PDU negotiation, conformance block negotiation (`Get`, `SelectiveAccess`), and ciphered association (`glo_initiateRequest` `0x21` / `glo_initiateResponse` `0x28`).
  - General Block Transfer (GBT) for segmented transfer of large profile buffers and Association View payloads.
- **COSEM Object Model (`ami_cosem_model.c`, `ami_obis_catalog.h`)**:
  - **Load Profile 15-min (`1.0.99.1.0.255`)**: 23 channels (active/reactive import/export energy, 4 quadrants Q1–Q4, per-phase voltage/current, neutral current, per-phase active power, total PF, frequency, voltage THD).
  - **Instant Profile (`1.96.98.128.0.255`)**: 11 instantaneous channels (Clock, 3-phase V/I, total P/Q, total PF, frequency).
  - **Power Quality Voltage Log Profiles (`1.0.99.14.2..4.255`)**: R/S/T phase voltage logs (Clock, Event Code, Phase Voltage).
  - **Event Log #1 (`0.0.99.98.0.255`)**: 2-column operational log (Clock, Event Code).
  - **Tamper Event Logbook (`1.97.99.98.x.255`)**: 3-column log (Clock, Event Code, Status) covering 9 PLN tamper modes (Cover Open, Terminal Open, Reverse Phase, Neutral Disturbance, Magnetic Tamper, Loss of Phase, Neutral Bypass, Reverse Power, Over Voltage/Current).
  - **Selective Access**: Selector 1 (Clock range) and Selector 2 (Entry index range).
  - **Schedule, Tariff & Load Control**: Register Activation (Class 6), Special Days Table (Class 11), Activity Calendar (Class 20), Single Action Schedule (Class 22), Multi-Tariff WBP/LWBP (Tariffs 1–4), Limiter (Class 71), and Disconnect Control (Class 70, `0.0.96.3.10.255`) with `Disconnected`, `Connected`, and `Ready_for_Reconnection` states.
- **Security & TrustZone (`ami_security.c`, `ami_dlms_cipher.c`, `ami_nvm_counter.c`, `ami_anti_bruteforce.c`, `secure_nsc.c`, `ami_boot_crypto.c`)**:
  - NIST SP 800-38D AES-128-GCM (`glo-get`, `glo-set`, `glo-action`) with 12-byte nonce and HLS 5 GMAC 4-pass mutual authentication (`2.16.756.5.8.2.5`) using peer system title verification.
  - Monotonic RX/TX Invocation Counter with wear-leveling NVM engine and fail-closed `0xFFFFFFFF` saturation handling.
  - Anti-brute-force progressive exponential backoff (`5s`, `10s`, `20s`, `40s`) and hard lockout after 5 consecutive failures (SPLN D5.002:2024).
  - Atomic RFC 3394 AES Key Wrap for batch updates (`GUEK`, `GBEK`, `AK`) persisted in AES-128-GCM encrypted Secure Flash (`FLASH_KEYSTORE` at `0x0C0FC000`, Page 126).
  - Pure C11 FIPS 180-4 / 186-4 Secure Boot (`SHA-256` + `ECDSA P-256`) with `PLN!` (`0x504C4E21`) manifest check and anti-rollback epoch tracking (`TAMP->BKP0R`).
- **Metrology Driver (`ami_ade9000.c`)**: Fail-closed SPI driver mapping calibrated 3-phase ADE9000 registers into COSEM snapshots.

---

## Tech Stack

| Layer | Technology / Component |
|---|---|
| **MCU** | STM32U585VIT6 (ARM Cortex-M33, TrustZone-M, SAU, GTZC, IWDG, Hardware TRNG) |
| **Metrology AFE** | Analog Devices ADE9000 (3-phase, SPI) |
| **RTOS / HAL** | FreeRTOS, STM32U5xx HAL Driver, CMSIS |
| **Language** | C11 (`NonSecure/AMI`, `Secure`, `tests`) |
| **Build System** | CMake (>= 3.20), Ninja |
| **Cross-Compilers** | `arm-none-eabi-gcc` (`gcc-arm-none-eabi.cmake`), `starm-clang` (`starm-clang.cmake`) |
| **Host Testing & Tooling** | CTest (16 suites), LCOV / gcov, Docker (`alpine:latest`), Python 3 (`docker_test/`), GXDLMSDirector |

---

## Prerequisites & Installation

### Host Testing & Simulator
- **CMake** >= 3.20
- **Ninja** build system
- **C11 compiler** (GCC, Clang, or MSVC)
- *(Optional)* **Docker** / **Docker Compose** for containerized builds and E2E testing
- *(Optional)* **lcov** and **gzip** (Linux/WSL or Docker) for code coverage extraction
- *(Optional)* **GXDLMSDirector** for interactive GUI testing against `STM32U585VIT6_SM.gxc`

### Target Firmware Cross-Compilation
- **ARM GNU Toolchain** (`arm-none-eabi-gcc`) or **STM32CubeCLT / STARM-Clang** (`starm-clang`)

---

## Configuration & Build Options

### Target Firmware Presets (`CMakePresets.json`)
The root `CMakePresets.json` configures the ARM Cortex-M33 build using `Ninja` and `gcc-arm-none-eabi.cmake`:

```powershell
# Configure and build Debug or Release firmware
cmake --preset Debug
cmake --build --preset Debug
```

Available presets: `Debug`, `Release`.

### Host Test & Security Compile Flags
- `ENABLE_COVERAGE=ON|OFF` (default `OFF`): Enables `--coverage -O0 -g` instrumentation in `tests/CMakeLists.txt` for GCC/Clang.
- `STM32U585xx`: Enforces target firmware authentication policy (blocks `No-Auth`, `LLS`, `HLS3`, `HLS4`, and non-16-byte challenges).
- `AMI_LITBANG_RDP_LEVEL1=1`: Opt-in build flag to enable RDP Level 1 provisioning during lab testing (defaults to RDP Level 0).

---

## Usage & Examples

### 1. Run the Host DLMS Simulator with GXDLMSDirector
Compile and start the HDLC-over-TCP simulator on port `4059`:

```powershell
cmake -S tests -B tests/build -G Ninja
cmake --build tests/build --target gurux_simulator
.\tests\build\host-tools\gurux_simulator.exe
```

1. Open **GXDLMSDirector**.
2. Load [`STM32U585VIT6_SM.gxc`](STM32U585VIT6_SM.gxc).
3. Click **Connect** to read COSEM objects over `127.0.0.1:4059` (HDLC, Logical Name referencing).

### 2. Run the Containerized Simulator via Docker
The multi-stage [`Dockerfile`](Dockerfile) builds and verifies all 16 CTest suites in Stage 1, then packages a stripped `/app/gurux_simulator` binary listening on port `4059` in Stage 2:

```powershell
# Using Docker Compose
docker compose up --build

# Or directly via Docker CLI
docker build -t stm32u585vit6sm:latest .
docker run --rm -p 4059:4059 stm32u585vit6sm:latest
```

### 3. Sign a NonSecure Firmware Binary (`ami_sign_tool`)
Sign `STM32U585VIT6_SM_NS.bin` with the ECDSA P-256 key and a monotonic version epoch before flashing:

```powershell
cmake --build tests/build --target ami_sign_tool

# Optional: regenerate the Secure Boot public key header
.\tests\build\host-tools\ami_sign_tool.exe --gen-header Secure/Core/Inc/ami_boot_pubkey.h

# Sign NonSecure binary with version epoch = 1
.\tests\build\host-tools\ami_sign_tool.exe --sign NonSecure/build/STM32U585VIT6_SM_NS.bin 1
```

---

## Project Structure

```text
├── .github/workflows/                # GitHub Actions CI (CTest, LCOV, ARM build, CodeQL)
├── NonSecure/                        # Cortex-M33 NonSecure application
│   ├── AMI/
│   │   ├── Inc/                      # Protocol, COSEM, OBIS catalog, and driver headers
│   │   └── Src/                      # HDLC, ACSE, APDU, GBT, Cipher, ADE9000, NVM Counter
│   ├── Core/                         # Startup, FreeRTOS tasks, HAL peripheral config
│   ├── CMakeLists.txt                # NonSecure target build configuration
│   └── STM32U585xx_FLASH_ns.ld       # NonSecure Flash linker script
├── Secure/                           # Cortex-M33 TrustZone Secure World
│   ├── Core/
│   │   ├── Inc/                      # Secure Boot pubkey & boot crypto headers
│   │   └── Src/                      # secure_nsc.c (NSC gateway), ami_boot_crypto.c, main.c
│   ├── CMakeLists.txt                # Secure target build configuration
│   └── STM32U585xx_FLASH_s.ld        # Secure Flash linker script (incl. FLASH_KEYSTORE)
├── Secure_nsclib/                    # Public NSC interface headers (secure_nsc.h)
├── Drivers/                          # STM32U5xx HAL & CMSIS
├── Middlewares/                      # FreeRTOS
├── tests/                            # 16 Host CTest suites & host tools
│   ├── CMakeLists.txt                # Host test & tool definitions
│   ├── ami_sign_tool.c               # ECDSA P-256 image signing CLI
│   ├── gurux_simulator.c             # HDLC-over-TCP simulator (port 4059)
│   ├── scripts/lcov_to_markdown.py   # LCOV .info to Markdown report generator
│   ├── test_ami_*.c                  # 16 unit/security/policy test suites
│   └── test_note.md                  # Host test & Gurux CTT guide
├── docker_test/                      # E2E Python client & Docker test harness scripts
├── docs/
│   ├── Architecture diagram for Project.md
│   ├── COVERAGE.md                   # LCOV coverage breakdown
│   └── security-rules/               # Protocol profile, architecture, checklist, threat model
├── CMakeLists.txt                    # Root STM32U585 CMake superbuild
├── CMakePresets.json                 # ARM Cortex-M33 CMake build presets (Debug/Release)
├── gcc-arm-none-eabi.cmake           # ARM GNU GCC toolchain file
├── starm-clang.cmake                 # STARM-Clang toolchain file
├── mx-generated.cmake                # STM32CubeMX generated CMake definitions
├── Dockerfile                        # Multi-stage Alpine simulator & CTest verification image
├── compose.yaml                      # Docker Compose service definition (port 4059)
├── CHANGELOG.md                      # Release history (Spin 1 & Spin 2)
├── CODE_OF_CONDUCT.md                # Contributor code of conduct
├── LICENSE                           # Project license
├── SECURITY.md                       # Vulnerability disclosure policy
├── SECURITY_REVIEW.md                # Static security audit & remediation status
├── STM32U585VIT6_SM.gxc              # GXDLMSDirector workspace profile
└── STM32U585VIT6_SM.ioc              # STM32CubeMX hardware configuration
```

---

## Testing

### Host Unit & Security Tests (16 CTest Suites)
```powershell
cmake -S tests -B tests/build -G Ninja
cmake --build tests/build --parallel
ctest --test-dir tests/build --output-on-failure
```

Suite details (including `AmiCosemTest`, `AmiSecurityTest`, `AmiDlmsAssociationTest`, `AmiFirmwareAuthPolicyTest`, `AmiDlmsApduTest`, `AmiDlmsHdlcTest`, `AmiDlmsPortTest`, `AmiAde9000Test`, `AmiDlmsCipherTest`, `AmiNvmCounterTest`, `AmiKeyTransferTest`, `AmiAntiBruteforceTest`, `AmiBlockTransferTest`, `AmiDisconnectControlTest`, `AmiTrustZoneBoundaryTest`, and `AmiBootCryptoTest`) are documented in [`tests/test_note.md`](tests/test_note.md).

### Code Coverage (LCOV)
On Linux/WSL (or inside an Alpine Docker container with `lcov` and `gzip`):

```bash
cmake -S tests -B tests/build_cov -G Ninja -DENABLE_COVERAGE=ON
cmake --build tests/build_cov
ctest --test-dir tests/build_cov --output-on-failure
lcov --capture --directory tests/build_cov --output-file tests/build_cov/coverage_raw.info --ignore-errors inconsistent,gcov
lcov --extract tests/build_cov/coverage_raw.info '*/NonSecure/AMI/*' '*/Secure/*' --output-file tests/build_cov/coverage_filtered.info --ignore-errors unused,inconsistent
lcov --list tests/build_cov/coverage_filtered.info
python3 tests/scripts/lcov_to_markdown.py tests/build_cov/coverage_filtered.info docs/COVERAGE.md
```

Latest measured baseline ([`docs/COVERAGE.md`](docs/COVERAGE.md)): **79.2% line coverage** (2,600 / 3,281 lines) and **95.7% function coverage** (154 / 161 functions), with core security modules (`ami_security.c`, `ami_dlms_hdlc.c`, `ami_dlms_cipher.c`) at **89.8%–91.9%** line coverage.

### Docker E2E Test Harness
Instructions for running the Python E2E client against `gurux_simulator` are documented in [`docker_test/README.md`](docker_test/README.md).

---

## Security Considerations & Trade-offs

- **Simulator vs. Target Auth Policy**: `gurux_simulator` permits `No-Auth` for local development and GXDLMSDirector browsing. Production firmware built with `STM32U585xx` enforces a fail-closed policy rejecting `No-Auth`, `LLS`, `HLS3` (MD5), `HLS4` (SHA-1), and any challenge length other than 16 bytes.
- **Anti-TOCTOU on NSC Calls**: `SECURE_DLMS_AES_GCM_Process()` and `SECURE_DLMS_TransferKeys_Atomic()` copy parameter descriptors into local Secure SRAM before performing `cmse_check_address_range` and integer-overflow checks (`uptr + size < uptr`), preventing asynchronous DMA modification between check and use.
- **Fail-Closed Crypto & NVM**: Hardware TRNG failures return an error and abort operations without falling back to a PRNG. If NVM Invocation Counter persistence fails or saturates at `0xFFFFFFFF`, ciphered sessions fail closed to prevent AES-GCM nonce reuse.
- **Zeroization vs. Compiler Optimization**: `secure_zeroize()` and `secure_memzero()` include compiler memory barriers (`__asm__ __volatile__("" : : "r"(ptr) : "memory")`) to prevent dead-store elimination under `-O2`/`-O3`.
- **Static Review Scope**: See [`SECURITY_REVIEW.md`](SECURITY_REVIEW.md) for hardware-dependent verification items remaining before production deployment (physical SRAM3 MPCBB lock verification, IWDG fault-reset validation, and RDP Level 1/2 mass-production lockdown).

---

## Status, Roadmap & Limitations

### Component Readiness

| Category | Component / Feature | Status | Notes |
|---|---|---|---|
| **Data Link** | HDLC Type 3, CRC-16 HCS/FCS, `0x7E` resilience, LLC | Complete | Verified in `ami_dlms_hdlc.c` & `AmiDlmsHdlcTest` |
| **Application** | ACSE (`AARQ`/`AARE`, ciphered `glo_initiate`), GBT | Complete | Verified in `ami_dlms_association.c`, `ami_dlms_block_transfer.c` |
| **Data Model** | 276 cataloged OBIS, 66 active objects, Class 7/6/11/20/22/70/71 | Complete | Verified in `ami_cosem_model.c` & `AmiCosemTest` |
| **Security** | AES-128-GCM Suite 0, HLS 5 GMAC, NVM Counter, Anti-Brute-Force | Complete | Verified against NIST SP 800-38D KAT & 1-bit flip tampering |
| **TrustZone & Boot** | NSC boundary checks, Flash Keystore, ECDSA P-256 Secure Boot, IWDG | Complete | Verified in `AmiTrustZoneBoundaryTest` & `AmiBootCryptoTest` |
| **Testing** | 16 Host CTest suites, LCOV coverage (79.2%), Docker E2E simulator | Complete | Automated on host & Alpine container |
| **Target Drivers** | UART DMA + IDLE ISR, ADE9000 SPI, HW TRNG, Dual-Image Flash | Code Complete | Pending physical STM32U585 + ADE9000 board & analog calibration |
| **Production & Lab** | RDP Level 1/2 lock, SWD disable, DLMS UA CTT, HES field interop | Pending | Requires mass-production flashing & accredited lab testing |

### 26-Week E2 Milestone Roadmap

| Phase | Target Hardware | Key Deliverables | Milestone Gate | Status |
|---|---|---|---|---|
| **Phase 1: Foundation** *(W1–W5)* | Host PC & STM32U575VGT6 Dev Board | Core DLMS/COSEM, ACSE, zero-heap HDLC, TrustZone SAU/GTZC, AES-128-GCM, HLS 5 GMAC, ADE9000 register mocks | **Gate G0 (W1)**, **Gate G1 (W5)** | Complete |
| **Phase 2: Spin 1 Bring-Up** *(W6–W10)* | Hardware Spin 1 *(Low Voltage)* | HAL UART/SPI port, 66 active COSEM objects, GBT, Anti-Brute-Force, Disconnect Control Class 70, Docker & CTest CI | **Gate G2 (W10)** | Complete |
| **Phase 3: Integration & Freeze** *(W11–W16)* | Hardware Spin 1 & Sensor Module | 9-mode Tamper Manager, ToU engine (8 tariffs, 8 schedules, 4 calendar types), NVM wear-leveling, DLMS Read-Set Freeze | **Gate G3 (W16)** | In Progress *(W10–W11)* |
| **Phase 4: Mains & Tape-Out** *(W17–W26)* | Hardware Spin 2 *(230/400V Mains)* | Safety Gate G-S, Class 0.5S/0.2S calibration (Fluke 6100B), HES/MDMS E2E, DLMS CTT v3.x, 24/7 Soak (-25°C to +70°C) | **G-S (W19)**, **G4 (W21)**, **G5 (W26)** | Pending Spin 2 |

### Testing Limitations
- Host tests and the TCP simulator use synthetic snapshots; they do **not** validate physical ADE9000 metrology accuracy, UART DMA/IDLE interrupt timing, FreeRTOS scheduling on silicon, or hardware TrustZone/SAU fault isolation.
- Passing `No-Auth` simulator tests does not represent production firmware authentication policy.

---

## Reference Documentation

- [`docs/Architecture diagram for Project.md`](docs/Architecture%20diagram%20for%20Project.md) — End-to-end system block, HLS5 sequence, CI/CD, and code dependency diagrams
- [`docs/security-rules/PROTOCOL_PROFILE.md`](docs/security-rules/PROTOCOL_PROFILE.md) — DLMS/COSEM protocol and association baseline
- [`docs/security-rules/SECURITY_ARCHITECTURE.md`](docs/security-rules/SECURITY_ARCHITECTURE.md) — TrustZone Secure/NonSecure isolation architecture
- [`docs/security-rules/SECURITY_CHECKLIST.md`](docs/security-rules/SECURITY_CHECKLIST.md) — Release security compliance checklist
- [`docs/security-rules/Threat_MODEL.md`](docs/security-rules/Threat_MODEL.md) — Threat mitigation model (T1–T12)
- [`docs/COVERAGE.md`](docs/COVERAGE.md) — Per-file LCOV line and function coverage report
- [`tests/test_note.md`](tests/test_note.md) — Host CTest suite descriptions and Gurux CTT guidance
- [`docker_test/README.md`](docker_test/README.md) — Docker test harness and Python E2E client
- [`SECURITY_REVIEW.md`](SECURITY_REVIEW.md) — Static security audit findings and remediation status
- [`SECURITY.md`](SECURITY.md) — Supported versions and vulnerability reporting policy
- [`CHANGELOG.md`](CHANGELOG.md) — Detailed release history
