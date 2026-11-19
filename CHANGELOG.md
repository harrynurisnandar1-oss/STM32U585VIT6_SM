# 📋 Firmware Release Changelog: STM32U585VIT6 + ADE9000 Smart Meter

This document records the complete history of feature updates, bug fixes, security hardening, and verification results for the 3-Phase Smart Meter firmware.

---

## 📌 Development Phase Summary (Spin 1 vs. Spin 2)

* **🚀 [SPIN 1 (v1.0.0 to v2.12.0)](#-spin-1--protocol-foundation-trustzone-architecture--initial-integration)**:
  * DLMS/COSEM (IEC 62056) & ACSE Association protocol foundation.
  * Zero-heap HDLC Frame Format 3 streaming parser with CRC-16 (HCS/FCS) validation.
  * ARM Cortex-M33 TrustZone security architecture (Secure vs. NonSecure isolation).
  * DLMS Security Suite 0 (AES-128-GCM) & HLS 5 (GMAC) authentication.
  * Isolated key management (Master Key KEK & RFC 3394 Remote Key Wrap).
  * Anti-replay wear-leveling NVM Invocation Counter & TCP host simulator integration (port 4059).

* **🏭 [SPIN 2 (v2.13.0 to v2.22.0)](#-spin-2--production-features-mainboard-readiness--certification)**:
  * Anti-Brute-Force & progressive exponential lockout (SPLN D5.002:2024).
  * General Block Transfer (GBT) for large profile and object segmentation.
  * PLN COSEM Data Model expansion to **66 active objects** in the Association View.
  * Multi-mode Disconnect Control Breaker (Class 70) state machine.
  * Class 7 Historical Profiling: 15-min Load Profile (23-channel), monthly Billing, Tamper Logbook (9 modes), PQ Voltage Log (R/S/T), and Instant Profile (11-channel).
  * Target driver readiness (non-blocking HAL UART circular DMA & fail-closed ADE9000 SPI).
  * 4-Pillar security hardening: 79.2% Code Coverage (91.9% crypto), Exhaustive Bit-Flip Negative Testing, Compiler Memory Barrier Zeroization, and TrustZone Boundary Overflow Checks.
  * Hermetic Docker Alpine Linux verification (CTest Suites 100% PASS + Python E2E Client).

---

## 📑 Quick Jump Table

| Phase | Release | Highlights |
|---|---|---|
| **SPIN 2** | [v2.22.0](#v2220---2026-11-17) *(Latest)* | Ciphered Association (`glo_initiateRequest`/`Response`), HLS5 Peer System Title Verification, Gurux Simulator & `.gxc` Sync |
| | [v2.21.0](#v2210---2026-11-16) | F1/F2/C1/I5/A1/RDP Remediation (Pure C): ECDSA P-256 Secure Boot, Flash Keystore, Anti-TOCTOU, IWDG, 16/16 CTest |
| | [v2.20.0](#v2200---2026-11-09) | 4-Pillar Security Verification, 79.2% Code Coverage, 15/15 CTest PASS, TrustZone boundary checks |
| | [v2.19.0](#v2190---2026-11-06) | R/S/T Voltage Quality Profiles, 11-Channel Instant Profile, Calendar/Schedule Classes |
| | [v2.18.1](#v2181---2026-11-04) | SIGFPE division-by-zero remediation, Docker runner CMake fix, unit test resilience |
| | [v2.18.0](#v2180---2026-11-04) | 23-Channel 15-min Load Profile & 9-Mode PLN Tamper Logbook |
| | [v2.17.0](#v2170---2026-11-04) | UART DMA IDLE line driver, ADE9000 SPI return-code guard, HLS5 proof NSC |
| | [v2.16.0](#v2160---2026-11-03) | Disconnect Control Breaker Class 70 (Modes 1..6, remote/manual disconnect) |
| | [v2.15.0](#v2150---2026-11-03) | Association View expansion from 13 to 49 active PLN standard objects |
| | [v2.14.0](#v2140---2026-11-03) | xDLMS Datablock Transfer / General Block Transfer (GBT) for large payloads |
| | [v2.13.0](#v2130---2026-11-03) | Anti-Brute-Force Lockout SPLN D5.002:2024 (5s..40s penalty & 5x Hard Lockout) |
| **SPIN 1** | [v2.12.0](#v2120---2026-11-03) | CIA Triad Remediation: MPCBB SRAM3, anti-TOCTOU, fail-secure TRNG |
| | [v2.6.0 to v2.11.0](#v260---2026-11-02) | Suite 0 Ciphered APDU (AES-GCM), NVM Anti-Replay Counter, NSC Gateway |
| | [v2.0.0 to v2.5.2](#v200---2026-10-13) | LLC Sublayer wrapping, HDLC Parameter Handshake, Standalone TCP Simulator |
| | [v1.0.0 to v1.4.1](#v100-added-08102026) | BER-TLV AARQ/AARE foundation, HLS 5 Challenge, Class 64 Security Setup |

---

# 🚀 SPIN 1 — Protocol Foundation, TrustZone Architecture & Initial Integration

## Scope and limitations

Sprint 1 is an initial protocol slice, not a complete DLMS/COSEM stack. It does not parse HDLC frames, validate client/server addresses, negotiate an InitiateRequest/InitiateResponse user-information payload, implement selective column projection, or transfer large responses using block transfer. Ciphered APDUs and Class 7 selective access are present, but production security depends on the Secure-world SAL callbacks described below. Pass application APDUs here only after the HDLC layer validates framing and addressing. This code is not ready for network exposure or product deployment.

The HLS mechanism OID and cryptographic proof function are configured by the product security profile. This project does not claim a particular HLS algorithm until that profile and its keys are integrated through the TrustZone Secure service.

## Standards references

The implementation scope is intended for review against IEC 62056-5-3, IEC 62056-6-2, and SPLN D3.006:2021. Verify BER/A-XDR encoding, authentication mechanism details, diagnostic values, conformance negotiation, OBIS mappings, and product security requirements against controlled editions before claiming compliance. No conformance certification is claimed.

## Build and run host checks

Host tests are configured with CMake and run with CTest:

```powershell
cmake -S tests -B tests/build -G Ninja
cmake --build tests/build --parallel
ctest --test-dir tests/build --output-on-failure
```

These checks validate host logic only; they do not validate FreeRTOS scheduling, STM32U585 TrustZone behavior, or UART/HDLC operation. Docker and CMock are not part of this configuration.

## [v1.0.0] Added 08/10/2026 

- Added bounded BER-TLV parsing for AARQ and building of AARE using logical-name application context OID `2.16.756.5.8.1.1`.
- Added initial unauthenticated development association flow and association state tracking.
- Added bounded xDLMS `GET-Request-Normal` parsing without selective access and `GET-Response-Normal` encoding for supported COSEM values and access errors.
- Added read-only Class-8 Clock support at logical name `0.0.1.0.0.255`: attribute 2 (12-byte date-time), attribute 3 (time-zone minutes), and attribute 4 (clock status).
- Added CTest host suites for COSEM model, security facade, DLMS association, and DLMS APDU logic.

## [v1.1.0] Added 09/10/2026 

### LLS and HLS association authentication

- Added LLS AARQ parsing for mechanism OID `2.16.756.5.8.2.1`, ACSE authentication requirement, and calling-authentication-value. Password verification is delegated to `verify_lls`; the password is not copied into the association context.
- Added configurable HLS mechanism OID and challenge exchange. The meter generates a 16-byte server challenge through the configured CSPRNG callback and returns it in AARE. Association stays `AUTH_PENDING` until `ami_dlms_process_hls_reply()` validates the client proof and generates the server proof for the client's challenge.
- Added bounded challenge storage, constant-time proof comparison, cleanup on reset/failure, and tests for successful/failed LLS plus the HLS challenge transition.
- Added callbacks for LLS verification, random challenge generation, and profile-specific HLS proof generation. Production callbacks are expected to bridge to protected credential/key storage and cryptographic services in the TrustZone Secure world.

### Security limitations

- HLS algorithm details vary by selected mechanism. The exact mechanism OID, key use, and proof calculation must be taken from the approved product profile. This code provides an integration hook; it does not claim a production HLS-GMAC implementation.
- LLS sends its password in the AARQ without cryptographic protection. Use LLS only on a physically trusted/protected link; use the product-approved HLS profile where stronger peer authentication is required.
- The legacy `ami_dlms_process_aarq()` function remains as a no-auth development entry point. Production code should call `ami_dlms_process_aarq_with_security()` and explicitly configure permitted authentication mechanisms.

## [v1.2.0] Added 10/10/2026 

### Global-ciphered APDU after HLS

- HLS-associated sessions now reject plaintext GET requests. The new APDU entry point accepts only global-ciphered GET (`GLO-GET-REQUEST`, `0xC8`) and SET (`GLO-SET-REQUEST`, `0xC9`) requests; it authenticates/decrypts the inner APDU and emits global-ciphered GET/SET responses (`0xCC` / `0xCD`).
- Added a Secure-world SAL backend contract for AES-GCM processing. It receives the security-control byte and invocation counter and is responsible for deriving nonce/AAD from the approved DLMS profile, system titles, and Secure-held keys.
- Added fail-closed checks for the configured security-control value, authentication tag, and receive invocation counter. Receive counter acceptance must be atomic/persistent; transmit counters must be allocated durably before encryption to prevent reuse.
- Ciphered SET requests are decrypted and returned as encrypted `read-write-denied` responses because the meter's COSEM model is intentionally read-only.
- Added host tests for plaintext rejection after HLS, ciphered GET response generation, tampered-tag rejection, and ciphered read-only SET denial. These tests use a deliberately non-cryptographic fake backend to test framing/state only; they do not validate AES-GCM implementation.

### Integration requirements

- The Secure backend must implement the product-selected DLMS security suite, key selection, system-title handling, nonce/AAD construction, and 128-bit tag policy. The receive-counter callback must reject replay/rollback and commit only authenticated counters; the transmit callback must persist a fresh counter before use.
- No concrete STM32U585 TrustZone AES-GCM backend or provisioned keys are included here. Until that SAL callback is implemented and validated against approved DLMS test vectors, do not use the test backend or treat this feature as production-ready.

## [v1.3.0] Added 12/10/2026

### Profile Generic Class 7 for event and billing history

- Added two read-only Profile Generic objects to the COSEM model: the standard event-log logical name `0.0.99.98.0.255` and a billing-history profile configured as `1.0.98.1.0.255`. The billing OBIS instance is a product-profile choice and must be checked against the applicable PLN/SPLN meter specification before release.
- Added a bounded, fixed-capacity snapshot for up to eight event records and eight billing-period records. Event rows contain a 12-byte COSEM date-time and event code; billing rows contain a date-time plus active-import and active-export energy counters. The board/metrology layer supplies these rows and manages timestamp generation and rollover; no fake records are generated by the model.
- Implemented read-only Class 7 attributes 1–8: logical name, A-XDR buffer array, capture-object list, trigger-based capture period (`0`), FIFO sort method, timestamp sort object, entries in use, and profile capacity. Event capture objects describe Clock and event-code data; billing capture objects describe Clock and the active-import/export registers.
- Added A-XDR passthrough for already encoded array/structure values so Class 7 buffer and capture-object attributes can travel through the existing GET response path, including the global-ciphered path after HLS.
- The snapshot arrays are provided oldest first; producers must keep the counts at or below `AMI_COSEM_PROFILE_MAX_ENTRIES` and handle circular-buffer policy. Reads of buffer and entries-in-use return value-unavailable until their corresponding profile-validity bit is set.

### Scope notes

- This adds basic full-buffer reads only. Selective access by entry range or date range, profile capture/reset methods, persistent storage, and a FreeRTOS producer task are not implemented in this slice.
- Class 7 A-XDR encoding and the configured billing OBIS must be reviewed against the controlled IEC 62056 and product/SPLN profile editions. This change alone does not establish DLMS conformance.

## [v1.4.0] Added 12/10/2026

### SPLN D3.002:2021 OBIS review and date-range ordering

- Reviewed the provided `obis.md` inventory as the authoritative SPLN D3.002:2021 reference. Repeated logical names in the source table are recorded as source-profile entries and were not “corrected” based on neighboring OBIS patterns.
- Fixed Class 7 time-range ordering to compare calendar/time fields only (year through hundredths). Day-of-week, deviation, and clock status no longer affect range validation or row inclusion.
- The firmware COSEM model still implements only its explicitly listed register, Clock, Security Setup, and Profile Generic objects. The remaining SPLN OBIS objects require corresponding snapshot data, access semantics, and product mapping before they can be exposed; this review does not claim full SPLN object coverage.

### Three-phase measurement OBIS catalog

- Added the read-only Class-3 measurement OBIS entries from SPLN D3.002:2021 for neutral values, frequency, phase/total power factor, active/reactive/apparent power, and per-phase active/reactive/apparent energy.
- Added a bounded snapshot sample bank keyed by logical name. The COSEM model supplies the catalog-defined data type, scaler, and unit; metrology/board code supplies raw values and validity, so missing measurements return value-unavailable instead of a valid zero.
- The OBIS row for Q2 phase-S energy repeats `1.0.45.8.0.255` from Q1 phase-S in the supplied table. It is intentionally omitted from the additional catalog pending clarification against the controlled standard; no substitute OBIS is guessed.
- Demand registers, tariffs, power-quality metrics, event profiles, and control/configuration objects remain outside this measurement-register slice.

## [v1.4.1] Added 13/10/2026

### Security Setup Class 64 and wrapped key transfer

- Added Security Setup metadata object `0.0.43.0.0.255`, Class 64 version 1, with read-only attributes for logical name, security policy, security suite, client/server system titles, and an empty certificates array when no certificate store is configured.
- Corrected the requested class number: Class 15 is Association LN; Security Setup is Class 64. The Class 64 model does not expose plaintext keys as readable attributes.
- Added ciphered Action Request support for Security Setup method 2 (`key_transfer`) after HLS. The accepted Suite 0 key-type values are 0 (global unicast encryption key) and 2 (authentication key). Incoming 16-byte keys are represented as 24-byte AES-128 key-wrap values under the KEK.
- The Secure-world callback receives the wrapped batch and is responsible for RFC 3394 unwrap, KEK protection, integrity validation, atomic installation of all requested keys, and rejecting a failed/partial update. NonSecure code does not unwrap or retain plaintext keys. Host tests use a fake callback and do not verify key cryptography or hardware isolation.

### Profile Generic selective access

- Extended Get-Request-Normal parsing for Class 7 buffer attribute 2: selector 1 filters by Clock date-time range; selector 2 filters by entry range (one-based, inclusive; `to_entry = 0` means through the newest entry).
- Range descriptors must identify Clock `0.0.1.0.0.255`, attribute 2, and use complete date-time values. Entry descriptors must select all columns (`from_selected_value` and `to_selected_value` both zero). Unsupported selectors, range objects, and column projections fail with `scope-of-access-violated`; the parser never silently returns the full log for an unsupported selection.
- Selective access is applied before A-XDR encoding, so the response contains only matching rows. Large response block transfer and partial-column projection remain future work.

### VS Code CMake Tools for host tests

- Added a host-test CMake preset in `tests/CMakePresets.json` and workspace settings in `.vscode/settings.json` that point CMake Tools to `tests/`, separate from the CubeMX firmware CMake project at the repository root.
- Updated `tests/README.md` with the reason the extension previously selected the ARM firmware project and the steps to configure/build/run host CTest from VS Code.

### HDLC stream framing engine (ISO/IEC 13239 & IEC 62056-46)

- **Frame Format 3 Architecture (`ami_dlms_hdlc.h`, `ami_dlms_hdlc.c`)**:
  - Implemented compliant byte-stream framing based on the DLMS HDLC profile (Frame Format Type 3, `0xA0` prefix with 11-bit length encoding).
  - Built a zero-heap streaming state machine (`ami_dlms_hdlc_parse_byte`) with deterministic O(1) buffer bounds (`AMI_HDLC_MAX_FRAME_BYTES = 544u`), completely eliminating dynamic memory allocation and buffer overflow vectors.
  - Implemented variable multi-octet HDLC addressing (1 to 4 bytes per SAP) conforming to DLMS server/client logical device conventions (supporting up to 28-bit addresses with LSB extension-bit handling).
  - Implemented dual-stage CCITT CRC-16 (X.25 polynomial `0x8408` inverted) verification:
    - **HCS (Header Check Sequence)**: Validates destination, source, and control fields before payload processing.
    - **FCS (Frame Check Sequence)**: Validates complete frame integrity over all transmitted octets.
- **Control & Information Frame Dispatch (`ami_dlms_hdlc_build_frame`)**:
  - Added native response generator for Supervisory/Unnumbered frames: auto-responds to connection requests (`SNRM`, `0x93`) and disconnections (`DISC`, `0x53`) with Unnumbered Acknowledge (`UA`, `0x73`) while properly reversing source and destination SAPs.
  - Built bidirectional Information (`I`) frame transmission with 3-bit send/receive sequence counter rotation (`N(S)` and `N(R)`).
- **Target Integration & Testing**:
  - Registered `ami_dlms_hdlc.c` into the `NonSecure` ARM Cortex-M33 firmware build.
  - Added comprehensive CTest test suite `AmiDlmsHdlcTest` (`test_ami_dlms_hdlc.c`) validating standard CCITT CRC test vectors (`"123456789"` -> `0x906E`), variable address encoding/decoding boundaries, invalid address rejection, truncated inputs, HCS/FCS tampering detection, and I-frame APDU roundtrips.

### Threat-hunting hardening & memory safety

Conducted an in-depth security threat hunting audit across input parsers, cryptographic boundaries, and state machines, resolving the following vulnerabilities:

- **CWE-226: Sensitive Residual Key Material in RAM (`ami_dlms_apdu.c`)**:
  - *Vulnerability*: Post-execution of Security Setup Class 64 `key_transfer` Actions, `memset` only wiped `response_plain_length` (5 bytes), leaving raw 24-byte wrapped AES keys lingering in `workspace` memory.
  - *Fix*: Enforced full-capacity zeroization across the entire caller-owned `workspace` buffer on every path after decryption, ensuring all ephemeral keys and plaintext APDUs are thoroughly scrubbed.
- **CWE-20 / CWE-703: HDLC Stream Desynchronization & Flag Lockup (`ami_dlms_hdlc.c`)**:
  - *Vulnerability*: Injected headers with inflated length fields followed by premature `0x7E` delimiters caused the parser to interpret flags as payload data, locking up stream synchronization until buffer saturation.
  - *Fix*: Enforced strict delimiter synchronization: an incoming `0x7E` on frames shorter than expected immediately aborts the invalid frame and resynchronizes to the next delimiter.
- **CWE-384: Session Desynchronization on Reconnection (`ami_dlms_hdlc.c`)**:
  - *Vulnerability*: Subsequent `SNRM` or `DISC` frames generated `UA` acknowledgments without resetting sequence counters, causing subsequent `I`-frame sequence number mismatches with newly connected test clients.
  - *Fix*: Added atomic reset of sequence counters (`tx_sequence = 0u`, `rx_sequence = 0u`) inside `ami_dlms_hdlc_build_frame()` upon generating `UA` responses for `SNRM`/`DISC`.
- **CWE-284: Active Session Teardown DoS (`ami_dlms_association.c`)**:
  - *Vulnerability*: Stray or unauthenticated AARQ bytes received while a valid HLS session was active forced the state machine directly into `AMI_DLMS_ASSOC_ERROR`, severing active HES communications.
  - *Fix*: Protected active sessions by cleanly rejecting stray AARQ requests without corrupting active association state or invalidating active challenge/session keys.
- **CWE-703: Fail-Closed Transactional A-XDR Serialization (`ami_cosem_model.c`)**:
  - *Vulnerability*: Buffer exhaustion during Profile Generic (Class 7) multi-row encoding broke loop execution but still proceeded to finalize partial headers, emitting corrupted A-XDR streams to clients.
  - *Fix*: Made Profile Generic encoding transactional: any partial row encoding failure immediately aborts the response and returns `AMI_COSEM_INVALID_ARGUMENT`.
- **Regression Verification**:
  - Added strict assertion coverage across host unit tests confirming zeroization, session preservation under malformed inputs, framing recovery, and clean sequence rollover.

## [v2.0.0] - 2026-10-13

### Added
- **LLC Sublayer Auto-Wrapping & Stripping (`ami_dlms_hdlc.c`)**:
  - Added detection and stripping of the 3-byte LLC header (`0xE6 0xE6 0x00` / `0xE6 0xE7 0x00`) on incoming I-frame streams (RX).
  - Added automatic insertion of the response LLC header (`0xE6 0xE7 0x00`) when building outgoing frames (TX).
  - *Rationale*: Satisfies IEC 62056-46 compliance so third-party DLMS clients (e.g., Gurux DLMSDirector) recognize meter APDUs without flagging an *unsupported format*.
- **HDLC Parameter Negotiation on SNRM Handshake (`ami_dlms_hdlc.c`)**:
  - Added `make_ua_parameters` to extract client buffer proposals from `SNRM` payloads and emit negotiated `UA` parameters (`0x81 0x80 ...`), clamping TX/RX sizes safely (max 512 bytes, window size = 1).
  - *Rationale*: Prevents connection timeouts on compliant clients requiring frame-size confirmation before opening a data session.
- **Standalone Interactive TCP Host Simulator (`tests/gurux_simulator.c`)**:
  - Added WinSock2/POSIX TCP socket simulator on port `4059` bridging the HDLC stream parser with the APDU/COSEM dispatch engine.
  - *Rationale*: Enables interactive end-to-end *black-box* testing with GXDLMSDirector on host PCs prior to physical board availability.

### Changed
- **Simulator Live Logger Diagnostics (`tests/gurux_simulator.c`)**:
  - Updated terminal logging to print frame type, control byte, source/destination SAPs, payload size, and first-byte inspection in real time.
  - *Rationale*: Speeds up diagnosing addressing/SAP mismatches or transport protocol mismatches (WRAPPER vs. HDLC).

### Fixed
- **Gurux-Compliant Frame Format 3 Length Calculation (`ami_dlms_hdlc.c`)**:
  - Corrected the 11-bit length field in Frame Format Type 3 to span from the frame format byte through the trailing FCS byte.
  - *Rationale*: Fixed frame validation failures in Gurux caused by counting internal payload only.
- **HDLC CRC-16 HCS/FCS Coverage (`ami_dlms_hdlc.c`)**:
  - Aligned HCS and FCS CRC byte ranges to include Frame Format and length octets, and added a real Gurux SNRM test vector (`7E A0 08 02 03 2D 93 26 CE 7E`).
  - *Rationale*: Ensures full CRC checksum interoperability with Gurux reference implementations.

## [v2.1.0] - 2026-10-14

### Added
- **Dynamic DLMS Conformance Negotiation (IEC 62056-5-3)**:
  - Added `proposed_conformance` bitmask extraction from xDLMS `InitiateRequest` in `parse_aarq` (`ami_dlms_association.c`).
  - Implemented dynamic conformance negotiation via bitwise AND between client proposal and server-supported services (`Get` `0x10` and `SelectiveAccess` `0x04`).
  - Rejects association (`result = 1, diagnostic = 2`) when negotiated conformance yields an empty intersection.
- **CTT `T_APPL_IDLE_N1` & `T_APPL_OPEN_9` Compliance**:
  - Implemented *silent drop* for `GET` requests while unassociated (`T_APPL_IDLE_N1`), while responding to Short Name Read (`0x05`) with `ExceptionResponse` (*service not supported*) for disconnect verification.
  - Permanently rejects `AARQ` if client sends `dedicated_key` in `InitiateRequest` when ciphering/security suite is disabled (`T_APPL_OPEN_9`).
- **Profile Generic Entry Selector Column Tolerance (`ami_dlms_apdu.c`)**:
  - Updated `parse_profile_selection` to accept column index 0 or 1 in Gurux CTT `ReadRowsByEntry` tests.
  - Initialized baseline simulation snapshot rows (1 *Event Log* row and 1 *Billing History* row) in `tests/gurux_simulator.c`.

### Changed
- **Verified Server Service Mask Alignment (`ami_dlms_association.c`)**:
  - Changed default server conformance mask from `0x00 0x50 0x1F` to the factual Sprint 1 mask `0x00 0x00 0x14` (`Get` + `SelectiveAccess`) and aligned `test_ami_dlms_association.c`.
- **Max PDU Size Validation & Negotiation (`T_APPL_OPEN_14`)**:
  - Aligned `AARE` Max PDU negotiation to respect client limits under 500 bytes and reject malformed `AARQ` frames below minimum xDLMS size.

### Fixed
- **100% CTest Suite Verification**:
  - Updated all 5 host CTest suites (`AmiCosemTest`, `AmiSecurityTest`, `AmiDlmsAssociationTest`, `AmiDlmsApduTest`, `AmiDlmsHdlcTest`) with 100% pass rate.

### Scope & CTT Testing Notes
- **Read-Only Meter Profile**: Disable `Write` tests in CTT because Sprint 1 rejects all `SET`/write operations by design.
- **Profile Generic Range Filtering**: Partial column projection and deep *selective access by date range* deferred to later sprints.
- **Sprint 2 Handoff**: HDLC framing, `AARQ`/`AARE` state machine, conformance negotiation, and basic object reads (LDN, Firmware, Clock) completed and ready for Sprint 2 (LLS/HLS Authentication & Security Setup Class 64).

## [v2.1.1] - 2026-10-14

### Fixed
- **GitHub Actions CI Workflow Fixes (`.github/workflows/ci.yml`)**:
  - Fixed `actions/checkout@v7` typo to `actions/checkout@v4`.
  - Expanded CI matrix to dual-platform: **Ubuntu Linux** (`Ninja` + `GCC`) and **Windows** (CMake MSVC toolchain verifying WinSock2 `ws2_32` compatibility).
- **Linux Socket Compatibility Header (`tests/gurux_simulator.c`)**:
  - Added `#include <netinet/tcp.h>` on POSIX/Linux so `TCP_NODELAY` is defined on Ubuntu GCC runners.

### Added
- **C11 Standard Enforcement on Host Tests (`tests/CMakeLists.txt`)**:
  - Set `CMAKE_C_STANDARD 11` and `CMAKE_C_STANDARD_REQUIRED ON` across GCC, Clang, and MSVC (`/std:c11`).
- **`.gitignore` Enhancements**:
  - Ignored `.vscode/` and removed local `.vscode/settings.json` from Git tracking.
  - Added ignore rules for host build artifacts (`*.exe`, `*.dll`, `*.lib`, `*.so`), firmware outputs (`*.axf`, `*.srec`, `*.s19`), IDE directories (`Debug/`, `Release/`), logs/dumps (`*.log`, `*.dmp`), and editor swap files (`*.swp`, `*~`).

## [v2.1.2] - 2026-10-14

### Added
- **Modern CMake Workflow Automation (`tests/CMakePresets.json`)**:
  - Upgraded `CMakePresets.json` schema to version 6 (`cmakeMinimumRequired` 3.25.0) with `workflowPresets` (`host-tests`) chaining Configure (`Ninja`), Build (`Debug`), and CTest via `cmake --workflow --preset host-tests`.
- **VS Code DevTools & Debugger Integration (`.vscode/`)**:
  - **`launch.json`**: Added native GDB (`cppdbg`) one-click debugging for `gurux_simulator.exe` and individual unit tests.
  - **`tasks.json`**: Added automated tasks (`CMake: Build Host Tests`, `CMake: Configure Host Tests`, `CTest: Run All Tests`, `CMake: Run Full Workflow`).
  - **`settings.json`**: Configured deterministic host test source directory and preset for VS Code CMake Tools.

### Fixed
- **CMakePresets Version Compatibility**:
  - Resolved `should NOT have additional properties: workflowPresets` validation error in VS Code CMake Tools by upgrading to preset version 6.
- **Build Output Directory Sync (`binaryDir`)**:
  - Aligned `binaryDir` in `CMakePresets.json` to `${sourceDir}/build` matching `tasks.json` and `launch.json`.


## [v2.2.0] - 2026-10-26

### Added
- **Multi-Pipeline Continuous Integration (CI) Architecture (`.github/workflows/`)**:
  - **`windows-ci.yml`**: Dedicated pipeline for native Windows builds and testing (`runs-on: windows-latest`) using CMake and Ninja, executing all host CTest suites and `gurux_simulator.exe`.
  - **`stm32u585-freertos-ci.yml`**: Embedded cross-compilation pipeline targeting ARM Cortex-M33 (STM32U585) with FreeRTOS. Compiles multi-context TrustZone (`Secure` and `NonSecure`), checks memory footprint (`arm-none-eabi-size`), and uploads firmware binary artifacts (`.elf`, `.bin`, `.hex`, `.map`).
  - **`cpp-analysis.yml`**: Automated static analysis and security audit pipeline using GitHub CodeQL (`security-extended` & `security-and-quality`) to detect memory leaks, buffer overflows, boundary check errors, and logic vulnerabilities in DLMS/COSEM and HDLC framing.

### Fixed
- **Missing `math.h` Header Resolution in Embedded ARM Toolchain (`stm32u585-freertos-ci.yml`)**:
  - Fixed preprocessor failure `fatal error: math.h: No such file or directory` when compiling STM32 HAL drivers (`stm32u5xx_hal_def.h`).
  - Installed ARM Newlib C standard library packages (`libnewlib-arm-none-eabi` and `libstdc++-arm-none-eabi-newlib`) alongside `gcc-arm-none-eabi` on the Ubuntu runner so all standard C runtime headers and `libm.a` are deterministically available.
- **Multi-Context Artifact Path Correction**:
  - Updated CI runner artifact collection paths to pull build outputs from `Secure/build/` and `NonSecure/build/` matching STM32CubeMX `ExternalProject_Add` structure.


## [v2.2.1] - 2026-10-28

### Added
- **VS Code Workspace Configuration & CTest Explorer Integration (`.vscode/settings.json`, `.vscode/tasks.json`)**:
  - Set `cmake.sourceDirectory` to `${workspaceFolder}/tests` with `host-tests` preset so VS Code CMake Tools Test Explorer automatically discovers and runs all 5 host unit test suites interactively.
  - Added automated tasks in `tasks.json` for STM32 ARM firmware builds (`cmake --build --preset Debug`), host CTest execution, and launching `gurux_simulator.exe`.

### Fixed
- **Windows CI Toolchain Environment (`.github/workflows/windows-ci.yml`)**:
  - Added `ilammy/msvc-dev-cmd@v1` prior to CMake Configure on `windows-latest`, ensuring MSVC (`cl.exe`) is registered in `$PATH` when using `Ninja` and preventing `CMAKE_C_COMPILER not found`.
- **HDLC Compiler Warning Elimination & Refactoring (`NonSecure/AMI/Src/ami_dlms_hdlc.c`)**:
  - Refactored single-line compressed source blocks into standard C formatting, eliminating `-Wmisleading-indentation` GCC/Clang warnings.
  - Hardened frame control parsing (`decode_raw`) by masking P/F bit (`0x10`) when evaluating `SNRM`, `UA`, and `DISC` frame types.
  - Cleaned up duplicated code blocks that caused function redefinitions (`get_crc`, `put_be32`, `decode_raw`, etc.).
- **C99/C11 Designated Initializers (`tests/test_ami_dlms_apdu.c`)**:
  - Updated `AmiDlmsAssociation association` declarations in `test_voltage_get_request_response` and `test_unknown_obis_returns_access_error` to use `.state = AMI_DLMS_ASSOC_ASSOCIATED`, eliminating `-Wmissing-field-initializers` warnings.
- **HDLC Unit Test Output Alignment (`tests/test_ami_dlms_hdlc.c`)**:
  - Cleaned up test structure, removed stray code outside `main()`, and added explicit `DLMS HDLC tests passed.` output consistent with other CTest suites.
- **Guide & Hardcoded Path Corrections (`tests/test_note.md`)**:
  - Removed non-portable local hardcoded path (`D:\STM32U585VIT6_SM`) and replaced it with portable relative project instructions.
  - Corrected `gurux_simulator.exe` build/run path from `build/host-tools/` (STM32 firmware build dir) to `tests/build/host-tools/gurux_simulator.exe`.
- **Git Hygiene Cleanup (`.gitignore`)**:
  - Added `.settings/`, `.project`, and `.cproject` so local STM32CubeIDE/Eclipse metadata files remain untracked.
- **Root CMake Presets Restoration (`CMakePresets.json`)**:
  - Removed the `Host_testing` preset that had been mistakenly added to the root ARM firmware presets file.

## [v2.3.0] - 2026-10-28

### Added
- **Port Suite Integration into Host CTest (`tests/CMakeLists.txt`, `tests/test_ami_port.c`)**:
  - Added `ami_dlms_port.c` and new test executable `test_ami_port` to native host CTest targets for automated local and CI verification without Docker.
  - Fixed `AARQ` handling in `ami_dlms_port.c` so unsolicited `AARQ` does not reset/drop an active authenticated session.
  - Added `ami_dlms_port_set_security()` API on the port layer so authentication mode (`LLS`/`HLS`) can be explicitly configured instead of forcing `No-Auth`.
  - Handled APDU/`GET` parsing errors so failures do not transmit empty/corrupted frames over serial.
  - Updated weak `ami_dlms_hardware_uart_tx()` to return `0` (failure) before physical UART wiring, preventing unsent responses from being treated as sent.
  - Added return-value check for `ami_dlms_freertos_start_task()` in `MX_FREERTOS_Init()` with `Error_Handler()` fallback on RTOS allocation failure.
  - Enabled Git tracking for `docker_test/` (ignoring only `docker_test/build-docker/`) so the Docker test harness can be reviewed and reproduced in CI.
- **Phase 2 Hardware Porting Abstraction Layer (`NonSecure/AMI/Inc/ami_dlms_port.h`, `NonSecure/AMI/Src/ami_dlms_port.c`)**:
  - Implemented a Hardware Abstraction Layer decoupling the core DLMS/COSEM stack from physical UART/transceiver drivers.
  - Provided non-blocking byte-stream and buffer-stream parsers (`ami_dlms_port_process_byte()`, `ami_dlms_port_process_buffer()`) with zero-copy handling for MCU RAM efficiency.
  - Automated HDLC link-layer control: `SNRM -> UA`, `DISC -> UA`, `RR` keepalive, and LLC framing encapsulation (`0xE6 0xE6 0x00` / `0xE6 0xE7 0x00`) on I-Frame APDU exchanges (`AARQ`/`AARE`, `GET`, `SET`, `Action`).
  - Added fast ISR handlers (`ami_dlms_uart_rx_isr()`, `ami_dlms_uart_rx_buffer_isr()`) compatible with STM32 HAL UART Rx interrupts and circular DMA buffers.
  - Integrated high-priority FreeRTOS worker task (`ami_dlms_freertos_start_task()`) using `StreamBufferHandle_t` and CMSIS-RTOS v2 `osThreadNew()`, initialized automatically in `NonSecure/Core/Src/app_freertos.c` (`MX_FREERTOS_Init()`).
- **Dedicated Docker Test Suite & Security Hardening (`docker_test/`)**:
  - Placed in isolated `docker_test/` without modifying existing `tests/`.
  - Adopted Multi-Stage Alpine Linux build (`builder` & minimal `runner`) stripping all compilers, linkers (`binutils`), and dev packages from the runtime image.
  - Reduced image size from 407.13 MB to 23 MB (-94.3% footprint).
  - Eliminated vulnerabilities from 2,463 CVEs (16 Critical, 474 High) down to 0 Critical, 0 High, 0 Medium in Docker Scout scans.
  - Included `test_ami_port.c` CTest suite for testing `SNRM`, `DISC`, `AARQ`, and `GET` via port abstraction.
  - Included automated Python client `test_dlms_e2e_client.py` simulating a DLMS client (such as GXDLMSDirector) for full handshake verification: `SNRM -> UA`, `AARQ -> AARE`, `GET` Phase R Voltage (230 V), `GET` Meter Clock, and `DISC -> UA`.
  - Added `run_tests.sh`, `docker-compose.yml`, Windows runner (`run_docker_tests.bat`), and `docker_test/README.md`.
  - Verified 100% PASS across all 6 unit test suites and E2E integration tests.

## [v2.4.0] - 2026-10-28

### Added
- **Complete SPLN D3.006:2021 OBIS Code Catalog (`NonSecure/AMI/Inc/ami_obis_catalog.h`)**:
  - Integrated all 276 DLMS/COSEM OBIS codes from PLN technical specifications into a standard C header.
  - Defined each OBIS code as a 6-octet C macro `{A, B, C, D, E, F}` with object name metadata, COSEM class ID, and decimal OBIS notation (`A.B.C.D.E.F`).
  - Organized all 276 OBIS codes into 22 structured functional categories:
    1. Meter Identification & Device Information (Meter type, Meter ID, HW/SW version, Customer ID, PLN Unit ID, Export-import mode, LDN).
    2. Meter Constants & Auxiliary Power (Active/reactive constants, Battery capacity, Supercapacitor capacity).
    3. Measurement Algorithms & General Configuration (Active energy measurement algorithm).
    4. Script Tables & Control I/O (Meter reset, Clear all tamper, Trigger test, Control I/O state/script, Limiter script, Tariff script, Push script, Alarm push 1/2, Image activation).
    5. Electrical Instantaneous Measurements (Frequency, Current and Voltage for phases R, S, T, and Neutral).
    6. Power Factor & Phase Angles (Total & per-phase power factor, Voltage and current phase angles for R, S, T, and Neutral).
    7. Instantaneous Power Registers (Total & per-phase R, S, T import/export active, reactive, and apparent power).
    8. Cumulative Energy Registers (Total & per-phase import/export active, reactive, apparent energy, Q1–Q4 quadrant reactive energy per phase, Billing compensation, Total active energy).
    9. Tariff-Based Energy Registers (Import/export active, reactive, and apparent energy for Tariffs 1 to 5).
    10. Demand & Maximum Demand Registers (Import/export active, reactive, apparent demand, Total and per-Tariff 1–5 maximum demand).
    11. Power Quality (PQ), Harmonics & Voltage Profiles (Total and per-phase voltage/current THD/TDD, Low voltage limits/delay/count, PQ voltage registers R/S/T, PQ voltage log profiles R/S/T, Compensation threshold PF).
    12. Profiles (Billing profile, Billing scheduler, Load profile, Instant profile, Load profile descriptor & monitor).
    13. Power Limiter & Disconnect Control (Power limiter, Disconnect control / breaker).
    14. Alarm Registers, Status & Descriptors (Alarm registers 1 & 2, Status register, Alarm descriptors 1 & 2, Alarm filters 1 & 2, Alarm monitors 1 & 2).
    15. Alarm Log Profiles (Flash error, RAM error, Clock loss, Low battery, MCU fault, ADC sampling fault, Supercap fault, Relay fault).
    16. Event Log Profiles (Power supply fail/up, ToU config change, Non-ToU config change, Stop measurement, I/O status change, Relay state change, Meter reset, Time/date change, Asymmetric power, Ready remote reconnect, Pinhole button, Manufacturer-specific events, Total meter off count).
    17. Tamper Log Profiles (Meter/terminal cover open, Wrong phase sequence, Current/voltage phase mismatch, Neutral current open, Loss of phase voltage, Magnetic field, 3P/1P reverse power, Q4 low PF, Over limit, INSCEK, GNDCEK, 6-hour current imbalance, Terminal cover opened energized/de-energized, Total tamper count).
    18. Time, Clock & Calendar Management (Clock Class 8, Activity calendar, Tariff activation, Special days table).
    19. Push Setup & IP Registration (Push setup alarm 1/2, load profile, historical data, IP registration).
    20. Image Transfer / Firmware Upgrade Management (Image transfer 0/1/2, Activation schedulers 0/1/2, Image activation script, Active firmware identifiers & signatures 0/1/2).
    21. COSEM Associations & Security Setup (Association LN 1–6, Security setup LN 2–6, Unicast & broadcast frame counters).
    22. Communication Ports & HDLC Setup (Optical port setup, Optical HDLC setup, RS232/RS485 HDLC setup).
  - Maintained 100% backward compatibility with all 65 macros already used in `ami_cosem_model.c` and unit tests.
  - Corrected PLN document table row 39 anomaly (*Phase S Q2 reactive energy* where column C read 45 while OBIS code read `1.0.46.8.0.255`) to octet `46` per IEC 62056-6-2 / DLMS Blue Book with clear documentation comments.


## [v2.5.0] - 2026-10-29

### Added
- **HLS Mechanism 5 (GMAC) & AES-128-GCM Cryptographic Core (`NonSecure/AMI/Inc/ami_security.h`, `NonSecure/AMI/Src/ami_security.c`)**:
  - Implemented NIST FIPS 197 AES-128 cryptographic core (Forward cipher, S-Box substitution, ShiftRows, MixColumns, AddRoundKey, 10-round Key Expansion, Galois field `xtime`).
  - Implemented NIST SP 800-38D GHASH and GF(2^128) multiplication with reduction polynomial R = 0xE1 || 0^120.
  - Implemented NIST SP 800-38D Galois/Counter Mode (AES-128-GCM) authenticated encryption/decryption (`ami_sec_aes_128_gcm_encrypt()`, `ami_sec_aes_128_gcm_decrypt()`) with 96-bit (12-byte) IV/Nonce and 128-bit (16-byte) tag.
  - Implemented standalone AES-128-GMAC authentication mode (`ami_sec_aes_128_gmac()`).
  - Implemented DLMS/COSEM High Level Security (HLS) Mechanism 5 GMAC calculation and verification (`ami_sec_dlms_hls5_gmac_calculate()`, `ami_sec_dlms_hls5_gmac_verify()`):
    - HLS 5 Mechanism OID: `{2, 16, 756, 5, 8, 2, 5}` (`0x60, 0x85, 0x74, 0x05, 0x08, 0x02, 0x05`).
    - 96-bit IV format: System Title (8 bytes) || Invocation Counter IC (4 bytes Big-Endian).
    - AAD format: Security Control Byte (`0x10`, auth-only) || Authentication Key AK (16 bytes) || Challenge (up to 64 bytes).
    - Proof tag format: Security Control Byte (1 byte) || IC (4 bytes) || GMAC Tag (16 bytes) = 21 octets.
  - Constant-time verification (`ami_sec_constant_time_equals()`) to prevent side-channel timing attacks.
  - Volatile memory zeroization (`secure_zeroize()`) to clear round keys, hash subkey H, and sensitive buffers from RAM after execution.
  - Callback adapter `ami_sec_hls5_gmac_proof_callback()` for direct integration with the DLMS/COSEM association state machine (`AmiDlmsAssociation`).
- **NIST SP 800-38D KAT & HLS 5 Handshake Verification (`tests/test_ami_security.c`, `tests/test_ami_dlms_association.c`)**:
  - Added NIST SP 800-38D Known Answer Tests (KAT):
    - Test Case 1 (Empty Plaintext & AAD): Validated GMAC Tag `58e2fccefa7e3061367f1d57a4e7455a`.
    - Test Case 2 (16B Plaintext, Empty AAD): Validated Ciphertext and GMAC Tag `ab6e47d42cec13bdf53a67b21257bddf`.
    - Test Case 4 (60B Plaintext, 20B AAD): Validated Ciphertext and GMAC Tag `5bc94fbc3221a5db94fae95ae7121a47`.
  - Verified tamper resilience: immediate decryption rejection on 1-bit ciphertext or tag corruption.
  - Verified GMAC authentication-only mode.
  - Verified end-to-end HLS 5 GMAC challenge-response calculation & verification.
  - Verified `ami_sec_hls5_gmac_proof_callback` integration in 4-pass mutual authentication handshake in `test_ami_dlms_association.c`.
  - All 6 CTest suites passed (6/6) and Cortex-M33 firmware built with zero errors/warnings.

## [v2.5.1] - 2026-10-30

### Fixed
- **Transparent Flag (`0x7E`) Handling in HDLC Payload & FCS (`NonSecure/AMI/Src/ami_dlms_hdlc.c`)**:
  - Fixed DLMS/COSEM object read failures (e.g., `0.0.96.96.0.255` from Gurux DLMSDirector) when the CRC FCS checksum or I-frame payload contained a `0x7E` byte before the closing frame boundary.
  - Added Header Check Sequence (HCS) integrity verification: when `0x7E` is encountered before total expected frame length (`c->frame_length < expected`), the parser evaluates the HCS first. If HCS is valid, `0x7E` is treated as legitimate transparent payload/FCS data without breaking stream synchronization.
- **HDLC Transparent Flag Test Suite (`tests/test_ami_dlms_hdlc.c`)**:
  - Added `test_transparent_flag_in_payload_and_fcs()` verifying a real Gurux frame ending with checksum `0x7E 0x0A 0x7E`.

### Added
- **SPLN D3.006:2021 Object Catalog Synchronization in Gurux Profile (`STM32U585VIT6_SM.gxc`)**:
  - Mapped and synchronized 276 PLN postpaid OBIS objects into the Gurux DLMSDirector XML workspace file, covering meter identification, instantaneous measurements, billing profiles, event logs, tamper logs, and disconnect control (breaker).

## [v2.5.2] - 2026-10-31

### Protocol and data-model baseline
- **Defined E2 Baseline for Phase 1 (`docs/security-rules/PROTOCOL_PROFILE.md`)**:
  - Recorded the actual implemented host profile: Logical Name referencing, raw HDLC-over-TCP on simulator, `GET` and selective access subset, read-only policy, and simulator vs. firmware authentication differences.
  - Documented the 12 objects currently advertised in the Association View and distinguished them from the broader OBIS catalog.
  - Explicitly stated that this is a development baseline, not an approved production profile or full conformance claim.
- **Added Association View Consistency Regression (`tests/test_ami_dlms_apdu.c`)**:
  - Ensured all advertised objects respond to `GET` attribute 2 using test snapshots.
  - Clarified Phase 1 scope: limited host protocol/data-model baseline is documented and tested; product profile approval and target validation remain required.
- **Updated Phase 1 Guides (`README.md`, `tests/test_note.md`)**:
  - Linked protocol/data-model baseline and distinguished host results from target firmware, ADE9000 data, and metrology.
- **Recorded Selected E2 Target Profile (`docs/security-rules/PROTOCOL_PROFILE.md`, `README.md`)**:
  - LN referencing; client SAP 16 only with HLS5-GMAC and Suite 0; server physical/logical 1/1; implemented `GET` and selective access services.
  - `No-Auth`/`LLS` remain rejected on firmware. HLS5 on SAP 16 means no unauthenticated public association and requires formal product/security sign-off.
  - Remote key updates specified via RFC 3394 wrapped keys under a manufacturing-provisioned per-device KEK. Key-transfer requires additional peer/SAP authorization and is not exposed in the current Association View.
- **Restricted Firmware to HLS5 and Target HDLC Addresses (`ami_dlms_association.c`, `ami_dlms_port.c`)**:
  - Firmware rejects HLS mechanisms other than HLS5-GMAC, non-16-byte client challenges, and `No-Auth`/`LLS`.
  - Firmware rejects legacy HLS even if callback configuration requests those OIDs.
  - Firmware frames are processed only for server address `1` and client SAP `16`; host simulator behavior remains unchanged.
- **Added Security Profile and Peer Regressions (`tests/test_ami_firmware_auth_policy.c`, `tests/test_ami_port.c`)**:
  - Tested rejection of `No-Auth`, `LLS`, `HLS3/MD5`, `HLS4/SHA-1`, and invalid challenge sizes; HLS5 with 16-byte challenge transitions to `AUTH_PENDING` with test callbacks.
  - Tested peer address validation for the firmware profile.
- **Recorded Production Provisioning Gates**:
  - Secure-world does not yet provide production HLS5/Suite 0 services, validated CSPRNG, per-device KEK, atomic key storage/remote key transfer, peer authorization, or persistent invocation counters.
  - Because Phase 1 requires production backend and provisioning, Phase 1 remains incomplete until the gates in `docs/security-rules/PROTOCOL_PROFILE.md` are implemented and tested on target STM32U585 hardware.

### Security hardening
- **Restricted Raw-Key Cryptographic APIs to Host Tests Only (`NonSecure/AMI/Inc/ami_security.h`, `NonSecure/AMI/Src/ami_security.c`)**:
  - Raw-key AES-128-GCM/GMAC APIs, key-byte HLS 5 calculation/verification, HLS contexts storing authentication keys, and pointer-based software backends are available only on host builds and excluded from STM32 firmware builds (`STM32U585xx` or `USE_HAL_DRIVER`).
- **Retained Host Test APIs (`tests/CMakeLists.txt`)**:
  - No changes to test sources or host CMake config; test vectors and adapters continue to work unchanged.
- **Enforced Fail-Closed Authentication Default on Firmware (`ami_dlms_port.c`, `ami_dlms_association.c`)**:
  - Firmware `AARQ` is processed against configured policy; if unconfigured, zero-initialized policy rejects `No-Auth` and any unconfigured mechanism.
  - Legacy `No-Auth` helper `ami_dlms_process_aarq()` is compiled only in host/non-firmware builds.
- **Rejected `No-Auth` and `LLS` Across All Firmware Builds (`ami_dlms_association.c`, `ami_dlms_port.c`)**:
  - Firmware association paths reject `No-Auth` even if `allow_no_auth` is requested; host tests and simulator retain legacy behavior.
  - `ami_dlms_port_set_security()` returns `AMI_DLMS_PORT_SECURITY_POLICY_REJECTED` if firmware config allows `No-Auth` or attaches an `LLS` callback, leaving active policy unchanged.
  - `LLS` is disabled on all firmware links until an explicitly protected link exception is designed and approved.
- **Added Firmware Auth Policy Tests (`tests/test_ami_firmware_auth_policy.c`)**:
  - Compiled with STM32 firmware macros to verify `No-Auth` and `LLS` `AARQ`s are rejected even when callbacks are supplied.
- **Clarified Key Protection Status (`docs/security-rules/SECURITY_CHECKLIST.md`, `SECURITY_REVIEW.md`)**:
  - Checklist marks Secure-world production key provisioning/storage as open; the existence of `AmiKeyHandle` alone is not proof of secure storage.
  - Security review notes that restricting raw-key APIs reduces exposure in NonSecure firmware, while Secure NSC key storage/provisioning and handle-based HLS services remain pending.

### Verification
- NonSecure firmware build succeeded; object inspection confirmed raw-key AES/HLS APIs and software backends are excluded from the image.
- All CTest suites passed; firmware policy tests verified `No-Auth` and `LLS` rejection while existing host tests continued passing.
- NonSecure build succeeded after HLS5-only and peer address enforcement; CTest 7/7 passed.
- These changes are **not** a Secure-world key store implementation and do not yet prove hardware key isolation.

## [v2.6.0] - 2026-11-02

### Added
- **DLMS Security Suite 0 Ciphered APDU Engine (`NonSecure/AMI/Inc/ami_dlms_cipher.h`, `NonSecure/AMI/Src/ami_dlms_cipher.c`)**:
  - Implemented AES-128-GCM authenticated encryption and decryption per DLMS/COSEM Green Book (IEC 62056-5-3 / SPLN D5.002:2024).
  - Handled `glo-get-request` (`0xC8`) -> `glo-get-response` (`0xCC`), `glo-set-request` (`0xC9`) -> `glo-set-response` (`0xCD`), and `glo-action-request` (`0xCB`) -> `glo-action-response` (`0xCE` / `0xCF`).
  - Verified Security Control byte (`SC = 0x30`, Suite 0 Auth + Encrypt).
  - Constructed 12-byte Nonce/IV: `System Title (8 bytes) || Invocation Counter (4 bytes Big-Endian)`.
  - Constructed 17-byte AAD: `Security Control (1 byte) || Authentication Key (16 bytes)`.
  - Verified 16-byte GCM authentication tag using constant-time comparison (`constant_time_memcmp`) and cleared working memory (`secure_zeroize`) on auth failure.
- **Invocation Counter Persistence & Anti-Replay**:
  - Enforced strictly monotonic increases for received Invocation Counters (`rx_counter > last_rx_counter`).
  - Rejected duplicate, zero, or replayed counters with `AMI_DLMS_APDU_COUNTER_REJECTED`.
  - Monotonically allocated server transmission counters (`tx_counter`) per encrypted response.
- **Cipher Integration in DLMS Port (`NonSecure/AMI/Inc/ami_dlms_port.h`, `NonSecure/AMI/Src/ami_dlms_port.c`)**:
  - Added `ami_dlms_port_set_cipher_backend()` to attach external or TrustZone cipher backends to the HDLC pipeline.
  - Routed incoming `0xC8`, `0xC9`, and `0xCB` APDUs through `ami_dlms_process_ciphered_request()`.
- **DLMS Cipher CTest Suite (`tests/test_ami_dlms_cipher.c`)**:
  - `test_cipher_suite0_glo_get_roundtrip`: End-to-end encryption, transmission, and decryption of Phase A Voltage (`1.0.32.7.0.255`).
  - `test_cipher_suite0_anti_replay_invocation_counter`: Validated acceptance of fresh counters and rejection of replayed counters.
  - `test_cipher_suite0_tag_tampering`: Validated cryptographic rejection and buffer zeroization on 1-bit tag tampering.
  - `test_cipher_suite0_port_integration`: Validated HDLC framing carrying encrypted APDU payloads.
- **ARM Cortex-M33 TrustZone NonSecure Callable (NSC) Gateway (`Secure_nsclib/secure_nsc.h`, `Secure/Core/Src/secure_nsc.c`)**:
  - Implemented `CMSE_NS_ENTRY` gateways: `SECURE_TRNG_GetBytes()`, `SECURE_DLMS_HLS5_Verify()`, and `SECURE_DLMS_AES_GCM_Process()`.
  - Passed parameters via struct pointers compliant with ARM AAPCS (`r0–r3`) register conventions.
- **ADE9000 AFE Metrology Driver & Snapshot Feeder (`NonSecure/AMI/Inc/ami_ade9000.h`, `NonSecure/AMI/Src/ami_ade9000.c`, `tests/test_ami_ade9000.c`)**:
  - Mapped 3-phase SPI registers (RMS Voltage/Current, Active/Reactive Power, Energy, Frequency/Period, Status).
  - Implemented snapshot feeder populating `AmiCosemRegisterSnapshot` for real-time DLMS/COSEM register reads.

### Removed
- **Prepaid STS Token Module (`ami_sts_token.h`, `ami_sts_token.c`, `test_ami_sts_token.c`, `SECURE_KEY_STS`)**:
  - Removed all prepaid token decoders, 20-digit STS parsers, credit deduction, and credit ledger logic.
  - Reaffirmed **Postpaid** 3-Phase AMI Smart Meter architecture per SPLN D3.006:2021 and E2 Protocol Data & Security scope.

## [v2.7.0] - 2026-11-02

### Added
- **Profile Generic Class 7 Buffers (15-min Load Profile `1.0.99.1.0.255` & Monthly Billing Freeze `0.0.98.1.0.255` / `1.0.98.1.0.255`)**:
  - Added `AmiCosemLoadProfileEntry` and `load_profile[]` buffer array in `AmiCosemRegisterSnapshot` with capacity up to `AMI_COSEM_PROFILE_MAX_ENTRIES` (8 rows).
  - Added validity flag `AMI_COSEM_VALID_LOAD_PROFILE` (`1u << 14`) and adjusted `AMI_COSEM_AXDR_ENCODED_MAX` to 1024 bytes.
  - Implemented 12 capture object columns for the 15-min Load Profile per SPLN D3.006:2021:
    1. Clock (`0.0.1.0.0.255`, Class 8)
    2. Status Register (`0.0.96.10.1.255`, Class 1)
    3. Active Import Wh (`1.0.1.8.0.255`, Class 3)
    4. Active Export Wh (`1.0.2.8.0.255`, Class 3)
    5. Reactive Import varh (`1.0.3.8.0.255`, Class 3)
    6. Reactive Export varh (`1.0.4.8.0.255`, Class 3)
    7. Phase A Voltage mV (`1.0.32.7.0.255`, Class 3)
    8. Phase B Voltage mV (`1.0.52.7.0.255`, Class 3)
    9. Phase C Voltage mV (`1.0.72.7.0.255`, Class 3)
    10. Phase A Current mA (`1.0.31.7.0.255`, Class 3)
    11. Phase B Current mA (`1.0.51.7.0.255`, Class 3)
    12. Phase C Current mA (`1.0.71.7.0.255`, Class 3)
  - Implemented 5 capture object columns for Monthly Billing Freeze: Clock, Active Import Wh, Active Export Wh, Reactive Import varh, Reactive Export varh.
  - Dynamic Profile Generic attribute handling:
    - Attribute 2 (`buffer`): A-XDR formatted array of structured rows.
    - Attribute 3 (`capture_objects`): Array of column descriptors (`class_id`, `logical_name`, `attribute_index`, `data_index`).
    - Attribute 4 (`capture_period`): Returns 900s (15 min) for Load Profile and 0s for trigger-based profiles (Billing Profile and Event Log).
    - Attribute 5 (`sort_method`): Returns 1 (FIFO).
    - Attribute 6 (`sort_object`): Returns Clock capture object descriptor.
    - Attribute 7 (`entries_in_use`): Returns current active entry count.
    - Attribute 8 (`profile_entries`): Returns maximum buffer capacity (8 entries).
- **Selective Access on Profile Generic**:
  - **Selector 1 (Range by DateTime)**: Filters buffer rows by timestamp range (`from_date` to `to_date`), returning `AMI_COSEM_SCOPE_VIOLATED` if `from_date > to_date`.
  - **Selector 2 (Entry by Index)**: Filters buffer rows by 1-based index (`from_entry` to `to_entry`), returning `AMI_COSEM_SCOPE_VIOLATED` if `from_entry == 0`, `from_entry > to_entry`, or `to_entry > count`.
  - Rejects parameterized selection on non-buffer attributes (attribute 1 or 3) with `AMI_COSEM_SCOPE_VIOLATED`.
- **DLMS Association View Integration (`NonSecure/AMI/Src/ami_dlms_apdu.c`)**:
  - Registered 15-min Load Profile `{7u, 0u, {1, 0, 99, 1, 0, 255}}` in `association_view[]`.
  - Expanded Association View to 13 objects (Association, Clock, 3x Voltage, 3x Current, 2x Active Energy, Event Log, Billing Profile, Load Profile).
- **Unit Tests & Verification (`tests/test_ami_cosem.c`, `tests/test_ami_dlms_apdu.c`)**:
  - `test_profile_generic_buffers`: Validated 12-element Load Profile, 5-element Billing Profile, 900s capture period, FIFO sort method, and Selective Access Selector 1/2 including scope violations.
  - `test_load_profile_apdu_selective_access`: Validated `GET-Request-Normal` with selective access against Load Profile through the DLMS association pipeline.
  - CTest verification: **9/9 tests PASS (100%)**.
  - ARM Cortex-M33 TrustZone cross-compilation: **0 errors, 0 warnings**.

## [v2.8.0] - 2026-11-02

### Added
- **STM32U585 Hardware Security Attribution Unit (SAU) Configuration (`Secure/Core/Inc/partition_stm32u585xx.h`)**:
  - Enabled SAU (`SAU_INIT_CTRL_ENABLE = 1`) with *Secure-by-Default* policy (`SAU_INIT_CTRL_ALLNS = 0`).
  - Configured physical SAU memory partitions:
    - Region 0: Non-Secure Callable (NSC) Veneers (`0x0C0FE000` - `0x0C0FFFFF`, `NSC = 1`).
    - Region 1: Non-Secure Flash Bank 2 (`0x08100000` - `0x081FFFFF`, `NSC = 0`).
    - Region 2: Non-Secure SRAM3 (`0x20040000` - `0x200BFFFF`, 512 KB, `NSC = 0`).
    - Region 3: Non-Secure Peripherals (`0x40000000` - `0x4FFFFFFF`, `NSC = 0`).
    - Region 4: Non-Secure External Memory / OctoSPI (`0x60000000` - `0x9FFFFFFF`, `NSC = 0`).
    - Region 5: System Memory Bootloader ROM (`0x0BF90000` - `0x0BFA8FFF`, `NSC = 0`).
  - Enforced System Control Block (SCB AIRCR) isolation:
    - `SYSRESETREQS = 1`: System reset restricted to Secure World.
    - `PRIS = 1`: Prioritized Secure exceptions over Non-Secure exceptions.
    - `BFHFNMINS = 0`: HardFault, NMI, and BusFault handled directly in Secure domain.
  - Configured NVIC ITNS (Interrupt Target Non-Secure):
    - Isolated critical hardware interrupts to Secure World: `GTZC_IRQn`, `SAES_IRQn`, `TAMP_IRQn`, `RTC_S_IRQn`, `FLASH_S_IRQn`, `RCC_S_IRQn`, `RNG_IRQn`, `HASH_IRQn`, `PKA_IRQn`.
    - Routed communication interrupts (USART, SPI, EXTI) to Non-Secure World.
- **Global TrustZone Controller (GTZC) Configuration (`Secure/Core/Src/main.c`)**:
  - Locked cryptographic peripherals to Secure domain via TZSC: `GTZC_PERIPH_SAES`, `GTZC_PERIPH_RNG`, `GTZC_PERIPH_TAMP`, `GTZC_PERIPH_RTC`, `GTZC_PERIPH_PKA`, `GTZC_PERIPH_HASH`.
  - Locked MPCBB3 on SRAM3 (`HAL_GTZC_MPCBB_Lock(SRAM3_BASE)`) to prevent runtime RAM attribute modification.
  - Enabled TrustZone Interrupt Controller (`HAL_GTZC_TZIC_EnableIT`) to trap illegal access events.
  - Locked TZSC1 configuration (`HAL_GTZC_TZSC_Lock(GTZC_TZSC1)`).
- **Hardware SecureFault & GTZC Tamper Exception Handling (`Secure/Core/Src/stm32u5xx_it.c`, `Secure/Core/Inc/stm32u5xx_it.h`)**:
  - Implemented `SecureFault_Handler()` to read/clear `SCB->SFSR`, perform fail-secure zeroization, invoke registered callbacks, and lock down.
  - Implemented `GTZC_IRQHandler()` and `HAL_GTZC_TZIC_Callback()` for real-time hardware illegal-access response.
- **Non-Secure Callable (NSC) Gateway Hardening (`Secure/Core/Src/secure_nsc.c`)**:
  - Validated NonSecure pointer bounds using ARM CMSE intrinsic `cmse_check_address_range()` across all NSC gateway APIs (`output`, `input`, `iv`, `aad`, `tag`), preventing *Confused Deputy* attacks.
  - Initialized hardware TRNG from peripheral registers with health-check timeout.
  - Added constant-time stack zeroization (`secure_memzero`).
- **Verification**:
  - Cortex-M33 TrustZone build (`cmake --build --preset Debug`): **0 errors, 0 warnings**:
    - Secure Image (`STM32U585VIT6_SM_S.elf`): RAM 11,984 B (4.57%), ROM 12,392 B (1.19%), ROM_NSC 96 B (1.17%).
    - NonSecure Image (`STM32U585VIT6_SM_NS.elf`): RAM 17,224 B (3.29%), ROM 43,568 B (4.15%).
  - CTest host suite: **9/9 tests PASS (100%)**.

## [v2.9.0] - 2026-11-02

### Added
- **Non-Volatile Memory (NVM) Persistence for Invocation Counter & Anti-Replay (`NonSecure/AMI/Inc/ami_nvm_counter.h`, `NonSecure/AMI/Src/ami_nvm_counter.c`)**:
  - Designed 24-byte `AmiNvmCounterRecord` with Magic `0x414D4943` ('AMIC'), `version` (1), `tx_invocation_counter` (lease bound), `rx_invocation_counter` (high-water mark), `reboot_count`, and IEEE 802.3 CRC-32 integrity.
  - **Nonce Uniqueness Guarantee per NIST SP 800-38D & IEC 62056-5-3**:
    - On boot/reboot or brown-out recovery, automatically jumps past the previous session lease bound (`persisted_tx_limit = old_persisted_limit + tx_window_step`) and commits the new record to NVM before transmitting the first APDU, preventing AES-128-GCM nonce collisions/reuse across unexpected power loss.
  - **Flash Wear Mitigation (Lease Window Allocation)**:
    - Uses configurable lease window (`tx_window_step`, default 100) so TX Invocation Counter increments in volatile memory without per-packet Flash writes.
    - Writes to NVM only upon reaching the allocated lease bound or explicit `ami_nvm_counter_flush()`.
  - **Strict Monotonic Anti-Replay Enforcement**:
    - Validates incoming APDUs (`in_rx_ic > last_rx_counter`) and rejects replayed, duplicate, backward, or zero counters.
    - Periodically persists RX high-water mark (`rx_window_step`) across reboots.
  - **Flexible Storage Driver Abstraction**:
    - `ami_nvm_create_ram_driver`: RAM-backed driver for host simulation and CI/CD pipelines.
    - `ami_nvm_create_tamp_backup_driver`: STM32 TAMP Backup Registers (`TAMP->BKP0R` to `BKP4R`) powered by VBAT with zero flash-wear endurance and single-cycle access.
- **DLMS Security Suite 0 Integration (`NonSecure/AMI/Inc/ami_dlms_cipher.h`, `NonSecure/AMI/Src/ami_dlms_cipher.c`)**:
  - Added `AmiNvmCounterContext *nvm_counter` pointer to `AmiDlmsSecuritySuite0Context`.
  - Added `ami_dlms_cipher_suite0_bind_nvm()` binding function.
  - Delegated `suite0_accept_rx_counter_cb` and `suite0_next_tx_counter_cb` directly to the NVM subsystem when bound.
- **Unit Tests & CTest Verification (`tests/test_ami_nvm_counter.c`, `tests/CMakeLists.txt`)**:
  - `test_nvm_counter_fresh_init`: Validated empty/corrupted memory initialization, record formatting, CRC-32, and initial lease commit.
  - `test_nvm_counter_monotonic_and_lease_window`: Validated strictly monotonic TX increments, zero storage writes within lease, and automatic lease expansion.
  - `test_nvm_counter_reboot_and_nonce_uniqueness`: Simulated sudden power loss and reboot, proving new TX counters strictly exceed all prior session counters.
  - `test_nvm_counter_anti_replay`: Validated rejection of duplicate, backward, or zero APDUs and RX commit thresholds.
  - `test_nvm_counter_reboot_preserves_anti_replay`: Validated anti-replay state persistence across reboots.
  - `test_nvm_suite0_cipher_binding`: Validated end-to-end Suite 0 cipher and NVM counter integration.
  - CTest result: **10/10 tests PASS (100%)**.
  - ARM Cortex-M33 TrustZone cross-compilation: **0 errors, 0 warnings** (`STM32U585VIT6_SM_NS.elf`: RAM 17,224 B / 3.29%, ROM 43,568 B / 4.15%).


## [v2.10.0] - 2026-11-02

### Added
- **Per-Device Factory Master Key (KEK) Provisioning in Secure Storage (`Secure_nsclib/secure_nsc.h`, `Secure/Core/Src/secure_nsc.c`)**:
  - **TrustZone Secure World Isolated Storage**: `SECURE_KeyStorageTypeDef` (`master_kek`, `guek`, `gbek`, `ak`, `is_provisioned`) permanently isolated in Secure World with no plaintext getter exposed to NonSecure.
  - **Automatic Hardware-Unique Master Key Derivation**: Prior to explicit factory provisioning (`!is_provisioned`), Secure World deterministically reads STM32U585 96-bit Unique Device ID (`UID_BASE` at `0x0BFA0700`), diversifies with `FACTORY_MASTER_SALT`, and derives a per-chip 128-bit Master KEK.
  - **Protected Factory Provisioning Gateway (`SECURE_ProvisionMasterKey`)**: Guarded by `SECURE_FACTORY_PROVISION_TOKEN` (`0x504C4E31` 'PLN1'), validates NonSecure pointers via CMSE (`validate_ns_buffer`), rejects all-zero weak keys, and marks provisioned state.
  - **Status & UID Query Interfaces**: Added `SECURE_IsMasterKeyProvisioned()` and `SECURE_GetDeviceUniqueId()` NSC APIs.
- **Atomic RFC 3394 AES Key Wrap Remote Key Transfer (`Secure/Core/Src/secure_nsc.c`, `NonSecure/AMI/Inc/ami_security.h`, `NonSecure/AMI/Src/ami_security.c`, `NonSecure/AMI/Src/ami_dlms_cipher.c`)**:
  - **RFC 3394 / NIST SP 800-38F Key Wrap & Unwrap Engine**:
    - Implemented standard AES-128 Inverse Cipher (`InvSubBytes`, `InvShiftRows`, `InvMixColumns`, `InvAddRoundKey`) in Secure World and NonSecure host tests.
    - Constant-time integrity verification against standard Initial Value (`0xA6A6A6A6A6A6A6A6`).
    - Passed 100% of NIST SP 800-38F Appendix 4.1 Known Answer Tests (KAT).
  - **Strict Atomic Key Transfer (`SECURE_DLMS_TransferKeys_Atomic`)**:
    - Accepts 24-byte wrapped key batches (`GUEK`, `GBEK`, `AK`, or new Master KEK) from HES/NonSecure.
    - Unwraps each key into Secure World staging buffers using the unique device KEK.
    - **All-or-Nothing Atomicity**: If any key in the batch fails RFC 3394 integrity verification, the entire transfer immediately rolls back, staging memory is zeroized, and active operational keys remain 100% untouched.
    - Operational keys commit to Secure Storage only if all keys in the batch verify cleanly; plaintext keys never cross into NonSecure.
- **Unit Tests & Holistic Verification (`tests/test_ami_key_transfer.c`, `tests/CMakeLists.txt`)**:
  - `test_rfc3394_kat`: Verified wrap/unwrap against NIST SP 800-38F Appendix 4.1 vectors.
  - `test_rfc3394_tamper_integrity`: Verified absolute rejection on any bit flip in 24-byte wrapped keys.
  - `test_suite0_atomic_key_transfer_success`: Verified successful batch update of GUEK and AK.
  - `test_suite0_atomic_key_transfer_rollback_on_failure`: Verified atomic rollback preserving old keys when one key in the batch fails.
  - Host CTest: **11/11 tests PASS (100%)**.
  - Cortex-M33 TrustZone build (`cmake --build --preset Debug`): **0 errors, 0 warnings** (`STM32U585VIT6_SM_S.elf`: RAM 12,064 B / 4.60%, ROM 17,296 B / 1.66%, ROM_NSC 128 B / 1.56%).


## [v2.11.0] - 2026-11-02

### Remediated & Security Hardened
- **Comprehensive Security Audit & Remediation (`SECURITY_REVIEW.md`)**:
  - Reviewed all 6 findings in `SECURITY_REVIEW.md`, confirmed technical validity across TrustZone and DLMS, and remediated critical gaps in Secure World and NonSecure layers.
- **Full NIST SP 800-38D AES-128-GCM & GMAC in Secure World (`Secure/Core/Src/secure_nsc.c`) [Finding 5]**:
  - Eliminated placeholder XOR and dummy tag routines in `SECURE_DLMS_AES_GCM_Process()`.
  - Implemented full AES-128 block cipher (10 rounds, key expansion), $GF(2^{128})$ multiplication with irreducible polynomial $R = 11100001 \parallel 0^{120}$ (`gf128_mul_sec`), `ghash_sec`, and 32-bit big-endian counter increment (`inc32_sec`).
  - On decryption (`is_encrypt == 0`), 16-byte authentication tag verification runs in constant time; on mismatch or tampering, plaintext buffers are immediately cleared (`secure_memzero`) and fail-secure error `-4` is returned.
  - In `SECURE_DLMS_HLS5_Verify()`, authentic GMAC verification uses Secure World's internal Authentication Key (AK) and activates `s_hls_authenticated_session = 1` on success.
- **Secure World Key-Management Lifecycle Hardening [Finding 6]**:
  - **One-Time Provisioning (OTP) Lock**: `SECURE_ProvisionMasterKey()` permanently rejects re-provisioning calls with error `-5` once `s_secure_keys.is_provisioned != 0`.
  - **Enforce HLS Session Auth for Remote Key Transfer**: `SECURE_DLMS_TransferKeys_Atomic()` requires `s_hls_authenticated_session == 1`, blocking NonSecure from triggering key transfers without completing HLS handshake.
- **Dynamic Hardware TRNG Fallback Entropy [Finding 2]**:
  - Removed static seed (`0xA5A55A5A`) in `SECURE_TRNG_GetBytes()`, mixing entropy from STM32 Unique ID (`UID_BASE`) and `SysTick->VAL`.
- **Verification & Build Status**:
  - Cortex-M33 TrustZone build (`cmake --build build/Debug`): **0 errors, 0 warnings** (`STM32U585VIT6_SM_S.elf`: RAM 12,072 B / 4.61%, ROM 20,512 B / 1.97%).
  - Host CTest Suite: **11/11 tests PASS (100%)** in 0.29s.
  - Updated `SECURITY_REVIEW.md` status for all findings.


## [v2.12.0] - 2026-11-03

### Hardened & Remediated (CIA Triad Findings)
- **TrustZone SRAM3 MPCBB Security Attribution (`Secure/Core/Src/main.c`) [Finding C1]**:
  - Fixed SRAM3 (`0x30000000`) MPCBB configuration: `MPCBB_SecConfig_array[0..31]` and `MPCBB_PrivConfig_array[0..31]` set to `0xFFFFFFFF` (Secure & Privileged) and locked via `HAL_GTZC_MPCBB_Lock(SRAM3_BASE)`, preventing Secure data/BSS/stack from being exposed as NonSecure.
- **Double-Fetch / TOCTOU Elimination at NSC Boundary (`Secure/Core/Src/secure_nsc.c`) [Finding I4]**:
  - `SECURE_DLMS_AES_GCM_Process()` and `SECURE_DLMS_TransferKeys_Atomic()` perform *atomic single-fetch copies* of parameter structs onto Secure stack before CMSE pointer validation.
- **Fail-Secure Hardware TRNG Error Handling (`Secure/Core/Src/secure_nsc.c`) [Finding I3]**:
  - On STM32 TRNG timeout or SECS/CECS flags, `SECURE_TRNG_GetBytes()` immediately zeroizes output buffers and returns error `-3` (fail-closed).
- **Protected Key-Transfer Lifecycle Authorization (`Secure/Core/Src/secure_nsc.c`) [Finding I1]**:
  - `SECURE_DLMS_TransferKeys_Atomic()` rejects transfers if unprovisioned (`is_provisioned == 0`) or unauthenticated (`s_hls_authenticated_session == 0`), and invalidates `s_hls_authenticated_session = 0` immediately after transfer completion.
- **Invocation Counter Overflow Protection & Fail-Closed NVM (`NonSecure/AMI/Src/ami_nvm_counter.c`) [Finding I2]**:
  - Added 32-bit counter guard (`>= 0xFFFFFFFEu`) preventing nonce exhaustion wrap-around per NIST SP 800-38D, and fail-closed enforcement on NVM high-water mark write failures.
- **DLMS Firmware Runtime Task Integration (`NonSecure/AMI/Src/ami_dlms_port.c`)**:
  - Integrated persistent NVM counter driver and Suite 0 cipher backend directly into FreeRTOS runtime task `AmiDlmsTaskEntry`.


---

# 🏭 SPIN 2 — Production Features, Mainboard Readiness & Certification
*(Reference: 26-Week Work Plan Rev D, Weeks 11–26: Disconnect Control Breaker Class 70, Anti-Brute-Force Lockout SPLN D5.002:2024, General Block Transfer GBT, PLN COSEM Objects, 23-Channel Load Profile, and Target Driver Readiness)*

## [v2.13.0] - 2026-11-03

### Added
- **Anti-Brute-Force: Progressive Exponential Backoff & Lockout per SPLN D5.002:2024 (`NonSecure/AMI/Inc/ami_anti_bruteforce.h`, `NonSecure/AMI/Src/ami_anti_bruteforce.c`)**:
  - **HLS5 Authentication Failure Tracking (`failed_attempts`)**:
    - Tracks consecutive failed DLMS authentication attempts.
    - Computes progressive exponential backoff delay ($delay = initial \times multiplier^{attempts - 1}$): 5s, 10s, 20s, 40s, capped at 60s.
  - **Permanent Hard Lockout After 5 Failures**:
    - Per SPLN D5.002:2024, after reaching 5 consecutive failures (`max_failed_attempts = 5`), permanently locks access (`AMI_ABF_STATE_LOCKED_OUT` / `0xFFFFFFFF`).
    - During lockout, fail-closed rejects all `AARQ` requests with `AARE` (*Association-Result: rejected-permanent, Diagnostic: authentication-failure*).
  - **Reset & Administrative Recovery**:
    - Successful HLS authentication (`ami_anti_bruteforce_record_success`) immediately resets the failure counter to 0; administrative unlock (`ami_anti_bruteforce_unlock`) is provided.
- **DLMS Port & Association LN Method 1 Integration (`NonSecure/AMI/Inc/ami_dlms_port.h`, `NonSecure/AMI/Src/ami_dlms_port.c`)**:
  - Added `AmiAntiBruteforceContext` pointer and timestamp tracker to `AmiDlmsPortContext`.
  - Automatic `AARQ` rejection during lockout using Green Book `encode_aare_lockout_rejection`.
  - Processed `Action-Request` (Class 15 Association LN, OBIS `0.0.40.0.0.255`, Method 1 `reply_to_HLS_authentication`), wiring `ami_dlms_process_hls_reply` directly to Anti-Brute-Force tracking.
- **Unit Tests & Host Verification (`tests/test_ami_anti_bruteforce.c`, `tests/CMakeLists.txt`)**:
  - `test_abf_default_initialization`, `test_abf_progressive_exponential_backoff`, `test_abf_administrative_unlock`, `test_abf_success_resets_backoff`, `test_abf_dlms_port_aarq_lockout_rejection`, `test_abf_dlms_port_action_hls_reply`.
  - CTest result: **12/12 tests PASS (100%)**.

## [v2.14.0] - 2026-11-03

### Added
- **Advanced Protocol: DLMS/COSEM xDLMS Block Transfer & General Block Transfer (GBT) (`NonSecure/AMI/Inc/ami_dlms_block_transfer.h`, `NonSecure/AMI/Src/ami_dlms_block_transfer.c`)**:
  - **Block Data Transfer per IEC 62056-5-3 / DLMS Green Book**:
    - Supports large payloads (15-min Load Profile, Event Log, Billing Profile, and Association View Object List) exceeding Max PDU / HDLC frame limits (512/1024 bytes).
    - Parses `GET-Request-For-Next-Datablock` (`0xC0 0x02 <invoke-id> <block-number: 4B BE>`).
    - Generates `GET-Response-With-Datablock` (`0xC4 0x02 <invoke-id> <last-block: 1B> <block-number: 4B BE> <result-choice: 0x00 raw-data> <A-XDR length> <payload chunk>`).
    - Generates `DataBlock-Result: data-access-result` (`0x01` choice, e.g., `temporary-failure = 2`) on errors.
    - Standard A-XDR length encoding helper (`ami_bt_encode_axdr_length`).
  - **Buffer Management & Chunking State Machine (`AmiBlockTransferContext`)**:
    - Caches response payloads up to 2048 bytes (`AMI_BT_MAX_CACHE_BUFFER_SIZE`), configurable `block_size` (default 128 bytes, clamped 32..512 bytes), states (`IDLE`, `TRANSFERRING`, `COMPLETED`, `ERROR`), identical-block retransmission (`requested_block == current_block`), and out-of-sequence error detection.
- **DLMS Port Integration & Automatic Block Transfer Transition (`NonSecure/AMI/Inc/ami_dlms_port.h`, `NonSecure/AMI/Src/ami_dlms_port.c`)**:
  - Added `block_transfer` context and `block_transfer_threshold` to `AmiDlmsPortContext` with `ami_dlms_port_configure_block_transfer(ctx, block_size, threshold)`.
  - In `dispatch_i_frame()`: handles `0xC0 0x02` next-block requests, automatically starts GBT (`ami_bt_start_get_transfer`) when `0xC0 0x01` responses exceed threshold, and resets GBT state (`ami_bt_reset`) upon `RLRQ` (`0x62`).
- **Gurux DLMS Simulator Integration (`tests/gurux_simulator.c`)**:
  - Integrated `AmiBlockTransferContext s_sim_bt` into the host TCP simulator (port 4059), automatically splitting responses exceeding 128 bytes into datablocks and serving `GET-Request-For-Next-Datablock` from Gurux DLMSDirector.
- **Unit Tests & Host Verification (`tests/test_ami_block_transfer.c`, `tests/CMakeLists.txt`)**:
  - `test_bt_init_and_reset`, `test_bt_start_transfer_and_chunking` (350B payload split into 128B + 128B + 94B with `last_block == 1` and bit-identical reassembly), `test_bt_retransmission_and_errors`, `test_bt_parse_get_next_block_request`, `test_bt_port_integration`.
  - CTest result: **13/13 tests PASS (100%)** in 0.30s.


## [v2.15.0] - 2026-11-03

### Added
- **Extended Data Model: Association View Object Expansion (PLN OBIS Catalog) (`NonSecure/AMI/Inc/ami_cosem_model.h`, `NonSecure/AMI/Src/ami_cosem_model.c`, `NonSecure/AMI/Src/ami_dlms_apdu.c`)**:
  - **Expanded Association View Catalog from 13 to 49 PLN Standard Objects** per SPLN D3.002:2021 and IEC 62056-6-2 for 3-Phase Postpaid Smart Meters:
    - **Management & Device Info (Class 1 & 15)**: Association LN (`0.0.40.0.0.255`), Clock (`0.0.1.0.0.255`), Logical Device Name (`0.0.42.0.0.255`), Firmware Version (`1.0.0.2.0.255`), Meter ID/Serial Number (`0.0.96.1.0.255`), Meter Type (`0.0.96.1.1.255`), Security Setup (`0.0.43.0.0.255`).
    - **Historical Profiles & Logs (Class 7)**: Event Log Profile (`0.0.99.98.0.255`), Monthly Billing Profile (`1.0.98.1.0.255`), 15-min Load Profile (`1.0.99.1.0.255`).
    - **Instantaneous Per-Phase Voltage & Current (Class 3)**: Voltage L1/L2/L3 (`1.0.32/52/72.7.0.255`), Current L1/L2/L3 (`1.0.31/51/71.7.0.255`).
    - **Total Cumulative Energy (Class 3)**: Active Import (+A, `1.0.1.8.0.255`), Active Export (-A, `1.0.2.8.0.255`), Reactive Import (+R, `1.0.3.8.0.255`), Reactive Export (-R, `1.0.4.8.0.255`).
    - **Multi-Tariff Registers T1 to T4 (Class 3)**: Active Import T1..4 (`1.0.1.8.1..4.255`), Active Export T1..4 (`1.0.2.8.1..4.255`), Reactive Import T1..4 (`1.0.3.8.1..4.255`).
    - **Power Quality (Class 3)**: Mains Frequency (`1.0.14.7.0.255`, Hz, scaler $10^{-3}$), Total & Per-Phase Power Factor (`1.0.13/33/53/73.7.0.255`, scaler $10^{-3}$), Voltage THD Total & L1/L2/L3 (`1.0.12/32/52/72.7.124.255`, scaler $10^{-1}$, %), Current THD Total & L1/L2/L3 (`1.0.11/31/51/71.7.124.255`, scaler $10^{-1}$, %).
    - **Instantaneous Power (Class 3)**: Total Active Power (+P, `1.0.1.7.0.255`), Total Reactive Power (+Q, `1.0.3.7.0.255`).
    - **Demand Limiter (Class 71)**: OBIS `0.0.17.0.3.255`, attributes 1–10 (Monitored Value linked to Active Power `1.0.1.7.0.255`, active/normal thresholds, min over/under durations, emergency profile, action script linkage).
    - **Disconnect Control (Class 70)**: OBIS `0.0.96.3.10.255`, attributes 1–4 (Output State, Control State, Control Mode).
  - **COSEM Snapshot & Parser Expansion (`ami_cosem_model.h`, `ami_cosem_model.c`)**:
    - Added Class IDs (`AMI_COSEM_DATA_CLASS_ID` 1u, `AMI_COSEM_DISCONNECT_CONTROL_CLASS_ID` 70u, `AMI_COSEM_LIMITER_CLASS_ID` 71u) and validity mask flags (`AMI_COSEM_VALID_TARIFF_REGISTERS`, `AMI_COSEM_VALID_POWER_QUALITY`, `AMI_COSEM_VALID_LIMITER`, `AMI_COSEM_VALID_DISCONNECT_CTRL`, `AMI_COSEM_VALID_DEVICE_INFO`).
    - Mapped snapshot arrays for multi-tariff, power quality, limiter parameters, disconnect control state, and device info strings.
  - **Full-Scale Transmission Buffers & Block Transfer Segmentation**:
    - 49-object Association View generates a 1,427-byte APDU, buffered in `AMI_BT_MAX_CACHE_BUFFER_SIZE` (2048 bytes) and automatically segmented by xDLMS DataBlock Transfer (`v2.14.0`).
  - **Gurux DLMS Simulation & Host Unit Tests (`tests/test_ami_cosem.c`, `tests/test_ami_dlms_apdu.c`, `tests/gurux_simulator.c`)**:
    - Added unit tests for multi-tariff, power quality, limiter, disconnect control, and 49-object readability; CTest Suite: **13/13 tests PASS (100%)**.


## [v2.16.0] - 2026-11-03

### Added
- **Load Control: Disconnect Control Breaker State Machine (Class 70) (`NonSecure/AMI/Inc/ami_disconnect_control.h`, `NonSecure/AMI/Src/ami_disconnect_control.c`)**:
  - **Deterministic State Machine per IEC 62056-6-2 / DLMS Blue Book & SPLN D3.002:2021**:
    - **Control States (Attr 3)**: `Disconnected (0)`, `Connected (1)`, `Ready_for_Reconnection (2)`.
    - **Output State (Attr 2)**: `0` (relay open / load disconnected), `1` (relay closed / load connected).
    - **Control Modes (Attr 4)**: Supports Modes 1–6; defaults to **Mode 2** (*Direct Remote Reconnect* for PLN AMI) and supports **Mode 1** (*Manual Consumer Confirmation*).
    - **Method 1 (`remote_disconnect`)**: Checks mode permissions (Modes 3 & 4 reject remote disconnect), triggers `AMI_BREAKER_CMD_OPEN`, transitions to `Disconnected (0)` and `output_state = 0`.
    - **Method 2 (`remote_reconnect`)**: Checks mode permissions (Mode 3 rejects remote reconnect); Modes 1 & 5 transition to `Ready_for_Reconnection (2)` keeping relay open until manual confirmation; Modes 2, 4, 6 transition directly to `Connected (1)` and trigger `AMI_BREAKER_CMD_CLOSE`.
    - **Manual Push Button Trigger (`ami_disconnect_control_manual_reconnect`)**: Closes relay (`CMD_CLOSE`) and transitions to `Connected (1)` when in `Ready_for_Reconnection` under Modes 1 & 5.
  - **Hardware Abstraction Layer (HAL Breaker Actuator)**:
    - Bistable latching relay callback interface (`AmiBreakerActuatorFn`) and reference STM32U585 100ms GPIO pulse driver (`ami_disconnect_hal_stm32_actuate()`).
    - Fail-closed handling: if actuator reports error/timeout, state remains unchanged and returns `AMI_DISCONNECT_ERR_ACTUATOR_FAILED`.
- **DLMS Port & APDU Action-Request Integration (`NonSecure/AMI/Inc/ami_dlms_port.h`, `NonSecure/AMI/Src/ami_dlms_port.c`, `NonSecure/AMI/Src/ami_dlms_apdu.c`)**:
  - Added `disconnect_control` pointer and `ami_dlms_port_set_disconnect_control()` to `AmiDlmsPortContext`.
  - Handled plain `Action-Request` (`0xC3`) and Suite 0 ciphered `GLO-ACTION-REQUEST` (`0xCB`) on Class 70 (`0.0.96.3.10.255`), returning `Action-Response-Normal` / `GLO-ACTION-RESPONSE` (`0xCE`) and synchronizing snapshot state.
- **Gurux DLMS Simulator & Host Unit Tests (`tests/gurux_simulator.c`, `tests/test_ami_disconnect_control.c`, `tests/CMakeLists.txt`)**:
  - Instantiated `s_sim_dc` with mock relay driver in simulator.
  - Added 7 unit tests (`test_dc_init_defaults`, `test_dc_mode_2_remote_disconnect_reconnect`, `test_dc_mode_1_manual_reconnect`, `test_dc_mode_3_remote_prohibition`, `test_dc_actuator_hardware_failure`, `test_dc_dlms_port_wire_action_request`, `test_dc_ciphered_action_request`).
  - CTest result: **14/14 tests PASS (100%)**.

## [v2.17.0] - 2026-11-04

### Changed & Fixed
- **Production Readiness Remediation for Hardware I/O, Metrology, and Cryptography**:
  - **HAL Hardware Driver Activation (`NonSecure/Core/Inc/stm32u5xx_hal_conf.h`)**: Enabled `HAL_UART_MODULE_ENABLED`, `HAL_USART_MODULE_ENABLED`, and `HAL_SPI_MODULE_ENABLED`.
  - **Non-Blocking UART/DMA Driver (`NonSecure/AMI/Inc/ami_bsp_uart.h`, `NonSecure/AMI/Src/ami_bsp_uart.c`)**: Implemented 512-byte circular DMA receive buffer with IDLE line interrupt (`HAL_UARTEx_ReceiveToIdle_DMA`) wired to FreeRTOS StreamBuffer via `HAL_UARTEx_RxEventCallback()`.
  - **Authentic HLS 5 Proof Calculation (`Secure/Core/Src/secure_nsc.c`, `Secure_nsclib/secure_nsc.h`, `NonSecure/AMI/Src/ami_dlms_port.c`)**: Added `SECURE_DLMS_HLS5_CalculateProof()` computing real 16-byte GMAC server proofs, and explicitly registered cipher backend in `ami_dlms_port.c`.
  - **Fail-Closed Key Management & TRNG (`Secure/Core/Src/secure_nsc.c`)**: Removed LCG PRNG fallback (TRNG failures return `-2`), removed public default keys (`DEFAULT_GUEK`, `DEFAULT_GBEK`, `DEFAULT_AK`; unprovisioned devices return `SECURE_ERROR_UNPROVISIONED` `-8`), and removed static provisioning token from `secure_nsc.h`.
  - **ADE9000 Metrology Reliability (`NonSecure/AMI/Src/ami_ade9000.c`, `tests/test_ami_ade9000.c`)**: Enforced strict SPI return-code checking without `(void)` casts; added `is_valid` and `last_error_code` flags so SPI failures prevent dirty snapshot population (`valid_mask = 0`).
  - **Secure Boot Pre-Jump Verification (`Secure/Core/Src/main.c`, `tests/test_ami_firmware_auth_policy.c`)**: Implemented `SECURE_VerifyNonSecureImage()` validating `0x504C4E21` ('PLN!') magic, image size, `MSP_NS` bounds, Thumb ResetHandler alignment, and anti-rollback counter.

## [v2.18.0] - 2026-11-04

### Added
- **23-Channel 15-Min Load Profile Buffer & Tamper Event Logbook (Class 7)**:
  - **23-Channel 15-Min Load Profile (`1.0.99.1.0.255`, Class 7) (`NonSecure/AMI/Inc/ami_cosem_model.h`, `NonSecure/AMI/Src/ami_cosem_model.c`)**:
    - Aligned with SPLN D3.006-2:2021 and 3-Phase Postpaid Smart Meter PRD.
    - **23 Capture Objects**: Clock, Status Register, +A, -A, +R, -R, Q1–Q4, L1/L2/L3 Voltage, L1/L2/L3 Current, Neutral Current, L1/L2/L3 Active Power, Total PF, Frequency, and L1 Voltage THD.
    - Increased `AMI_COSEM_AXDR_ENCODED_MAX` from 1024 to 2048 bytes and expanded `AmiCosemLoadProfileEntry` natively for 23 channels.
  - **Tamper Event Logbook (Class 7) (`NonSecure/AMI/Inc/ami_cosem_model.h`, `NonSecure/AMI/Src/ami_cosem_model.c`, `NonSecure/AMI/Src/ami_ade9000.c`)**:
    - **3 Standard Capture Objects**: Clock (`0.0.1.0.0.255`), Event Code (`0.0.96.11.0.255`), Status Register (`0.0.96.5.0.255`).
    - Supports general profiles (`0.0.99.98.0.255` & `0.0.99.98.1.255`) and per-tamper OBIS codes (`1.97.99.98.20..28.255`) covering 9 official PLN Tamper Modes (Cover Open, Terminal Cover Open, Phase Sequence Reverse, Neutral Disturbance, Magnetic Field, Loss of Phase, Neutral Bypass, Reverse Power Flow, Over Voltage/Current).
    - Wired automatic metrology tamper triggers (Reverse Power, Phase Sequence Reverse, Loss of Phase) from ADE9000 driver to event log.
  - **Unit Tests & Host Verification (`tests/test_ami_cosem.c`, `tests/test_ami_dlms_apdu.c`)**: CTest Suite: **14/14 tests PASS (100%)**.

## [v2.18.1] - 2026-11-04

### Changed & Fixed
- **Anti-Brute-Force & Lockout Resilience (SIGFPE Remediation)**:
  - **Division Arithmetic Sanitization (`NonSecure/AMI/Src/ami_anti_bruteforce.c`)**: Fixed potential integer division-by-zero (`SIGFPE`) on x86_64 Alpine Linux under `-O3 -DNDEBUG` by enforcing `backoff_multiplier >= 2u`, `initial_delay_sec > 0u`, and `safe_ceiling = max_delay / multiplier`.
  - **Resilience Unit Test (`tests/test_ami_anti_bruteforce.c`)**: Added `test_abf_zero_config_resilience()` for all-zero configuration parameters.
- **Docker Environment & Pipeline (`Dockerfile`)**: Added `cmake` to runtime container so `ctest` executes cleanly (**14/14 suites PASS** + E2E Python simulator pass).
- **GitHub Repository Cleanup**: Removed `.github/ISSUE_TEMPLATE` and `.github/PULL_REQUEST_TEMPLATE.md`.

## [v2.19.0] - 2026-11-06

### Added & Enhanced
- **Power Quality Voltage Log Profiles R/S/T (`1.0.99.14.2..4.255`)**:
  - Resolved *"Capture objects not read."* in Gurux DLMSDirector by implementing Class 7 profiles for Phase R (`1.0.99.14.2.255`), Phase S (`1.0.99.14.3.255`), and Phase T (`1.0.99.14.4.255`) with their status registers (`1.135.32/52/72.0.0.255`) in `association_view`.
- **Instant Profile Class 7 (`1.96.98.128.0.255`)**: Implemented 11 Capture Objects (Clock, 3-phase V/I, Total P/Q, Total PF, Mains Frequency) with range filtering in `read_profile()`.
- **New COSEM Interface Classes**: Class 6 (`0.0.14.0.0.255`), Class 11 (`0.0.11.0.0.255`), Class 20 (`0.0.13.0.0.255`), and Class 22 (`0.1.15.0.4.255`, `0.0.15.0.2.255`, `0.1.15.0.2.255`, `0.2.15.0.2.255`).
- **Extended Registers & Generic Data Fallback**: Added low voltage limit/delay, compensation threshold PF, total active energy, and generic Class 1 fallback.

### Changed & Fixed
- **Event Log #1 vs. Tamper Log Column Separation (`0.0.99.98.0.255`)**:
  - Fixed *"The number of columns does not match"* in Gurux DLMSDirector by configuring **Event Log #1 (`0.0.99.98.0.255`)** with **2 columns** (Clock & Event Code) while keeping **Tamper/Alarm Logbook (`1.97.99.98.x.255`)** at **3 columns**.
- **Simulator Clock & Range Alignment (`tests/gurux_simulator.c`)**: Updated simulator internal RTC to `2026-11-06 10:10:00` to pass Selective Access range queries.
- **HDLC Framing Fix in E2E Client (`docker_test/test_dlms_e2e_client.py`)**: Fixed `read_hdlc_frame()` for back-to-back `0x7E` flags and added automated E2E checks for Instant Profile, Event Log #1 (2 columns), and PQ Voltage Log Profile R.
- **Unit Test Verification**: Updated `tests/test_ami_cosem.c` assertions; CTest Suite: **14/14 tests PASS (100%)**.

## [v2.20.0] - 2026-11-09

### Added
- **Pillar 1: Code Coverage Metrics (`ENABLE_COVERAGE`) & CI Workflow**:
  - Added `-DENABLE_COVERAGE=ON` (`--coverage -O0 -g`) in `tests/CMakeLists.txt` and automated `lcov` reporting in `.github/workflows/ci.yml`.
  - Measured coverage: `ami_security.c` **91.9%** (431/469 lines), `ami_dlms_hdlc.c` **91.4%** (235/257 lines), `ami_dlms_cipher.c` **89.8%** (150/167 lines); total firmware source line coverage **79.2%** (2,600 / 3,281 lines).
- **Pillar 2: Negative Testing & Tamper Invalidation**:
  - Added `test_gmac_exhaustive_bit_flip_tampering` in `tests/test_ami_security.c` (exhaustive 1-bit flips across all 16 GMAC tag bytes, AAD, and IV/Nonce).
- **Pillar 3: Volatile Zeroization Hardening Against Compiler Optimization**:
  - Added inline assembly memory barriers `__asm__ __volatile__("" : : "r"(ptr) : "memory")` in `secure_zeroize()` (`ami_security.c`, `ami_dlms_apdu.c`) and `secure_memzero()` (`secure_nsc.c`) to prevent dead-store elimination under `-O2`/`-O3`.
- **Pillar 4: TrustZone CMSE Security Boundary Validation**:
  - Added 15th CTest suite `tests/test_ami_trustzone_boundary.c` (`AmiTrustZoneBoundaryTest`) validating NULL pointers, pointer integer overflow (`uptr + size < uptr`), Secure World boundary crossings, and NonSecure Flash write-permission rejection.

### Changed & Fixed
- Fixed Class 1 Data handling in `NonSecure/AMI/Src/ami_cosem_model.c` to consistently return `AMI_COSEM_OBJECT_NOT_FOUND` for unregistered objects.
- Aligned response buffer size and Event Log #1 2-column expectations in `tests/test_ami_dlms_apdu.c`.
- Verified **15/15 CTest suites PASS (100%)** and Python E2E simulator in hardened Docker container.

---

## [v2.21.0] - 2026-11-16

### Added & Security Hardening (Remediation F1/I2, F2/I6, C1, I5, A1 & Litbang RDP — 100% Pure C)
- **Cryptographic Secure Boot FIPS 180-4 SHA-256 + FIPS 186-4 ECDSA P-256 (`F2 / I6`)**:
  - Implemented zero-heap Pure C11 crypto in `Secure/Core/Inc/ami_boot_crypto.h` and `Secure/Core/Src/ami_boot_crypto.c`.
  - Added host signing CLI `tests/ami_sign_tool.c` to generate ECDSA P-256 keys, emit `Secure/Core/Inc/ami_boot_pubkey.h`, and sign `STM32U585VIT6_SM_NS.bin` at offset `0x400`.
  - `SECURE_VerifyNonSecureImage()` in `Secure/Core/Src/main.c` verifies canonical SHA-256, ECDSA P-256 signature, anti-rollback (`TAMP->BKP0R`), and `AMI_NS_SRAM_START/END` (`0x20040000U`–`0x200C0000U`).
- **Encrypted Key Persistence in Secure Flash (`C1`)**:
  - Allocated `FLASH_KEYSTORE` (8 KB, Page 126 at `0x0C0FC000`) in `Secure/STM32U585xx_FLASH_s.ld` (preserving `ROM_NSC` at `0x0C0FE000`).
  - `secure_flash_load_keys()` and `secure_flash_save_keys()` in `Secure/Core/Src/secure_nsc.c` encrypt `SECURE_KeyStorageTypeDef` with AES-128-GCM bound to `UID_BASE` and TRNG IV.
- **Key-Management Authorization (`F1 / I2`) & Anti-TOCTOU (`I5`)**:
  - Added `SECURE_DLMS_ResetSession()` and `SECURE_DLMS_SetSessionSap()` in `Secure_nsclib/secure_nsc.h` & `Secure/Core/Src/secure_nsc.c`, integrated with `NonSecure/AMI/Src/ami_dlms_port.c`.
  - Applied full **copy-then-validate** in `SECURE_DLMS_AES_GCM_Process()` and `SECURE_DLMS_TransferKeys_Atomic()` using local Secure SRAM buffers zeroized on every exit path.
- **Independent Watchdog (`A1`) & Opt-In Litbang RDP Level 1**:
  - Configured `MX_IWDG_Init()` (~6s, ST-LINK *debug-freeze*) and `SECURE_Watchdog_Kick()` in `NonSecure/Core/Src/app_freertos.c`; `Error_Handler()` logs `TAMP->BKP1R = 0xFA017001U` and triggers `NVIC_SystemReset()`.
  - `SECURE_CheckAndApplyLitbangRdp()` keeps RDP Level 0 by default with opt-in RDP Level 1 (`-DAMI_LITBANG_RDP_LEVEL1=1`).
- **Automated Testing (16/16 CTest PASS & ARM Cortex-M33 Build)**:
  - Added 16th CTest suite `tests/test_ami_boot_crypto.c` (`AmiBootCryptoTest`) and TOCTOU scenarios in `tests/test_ami_trustzone_boundary.c`.

## [v2.22.0] - 2026-11-17

### Added
- **Ciphered Association (`glo_initiateRequest` `0x21` & `glo_initiateResponse` `0x28`)**:
  - Added full encrypted association support in `NonSecure/AMI/Inc/ami_dlms_association.h` and `NonSecure/AMI/Src/ami_dlms_association.c`, including `client_system_title`, `dedicated_key`, `client_sap`, `server_sap`, and `decrypt_initiate`/`encrypt_initiate` hooks.
- **HLS5 Peer System Title Verification**:
  - Added `hls_verify` hook in `AmiDlmsAssociationSecurity` to verify HLS5 proofs using *peer system title* and *embedded Invocation Counter*.

### Changed & Fixed
- **DLMS Port, APDU, Cipher & TrustZone NSC Integration**:
  - Updated `NonSecure/AMI/Src/ami_dlms_port.c`, `ami_dlms_apdu.c`, `ami_dlms_cipher.c`, `ami_security.c` (and headers), plus `Secure/Core/Src/secure_nsc.c` and `Secure_nsclib/secure_nsc.h` for ciphered association and HLS5 verification.
- **Gurux Simulator & Unit Tests**:
  - Updated `tests/gurux_simulator.c` and `STM32U585VIT6_SM.gxc` for ciphered association and HLS5 testing; aligned `test_ami_dlms_association.c`, `test_ami_dlms_apdu.c`, `tests/test_ami_dlms_cipher.c`, `test_ami_security.c`, and `test_ami_disconnect_control.c`.
- **Documentation Sync**:
  - Updated `README.md`, `tests/test_note.md`, and `SECURITY.md` to reflect 16 CTest suites.
  - Verified `arm-none-eabi-gcc` cross-compilation for `STM32U585VIT6_SM_S` and `STM32U585VIT6_SM_NS` with **0 errors**.
