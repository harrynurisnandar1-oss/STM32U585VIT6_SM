# Host Test and Gurux Conformance Guide

This document explains what is tested on the host, how to run the suites, and how to interpret Gurux Conformance Test Tool (CTT) results. Host tests use synthetic/snapshot data and do not require physical target hardware.

## Running CTest

From the repository root in PowerShell:

```powershell
cmake -S tests -B tests/build -G Ninja
cmake --build tests/build --parallel
ctest --test-dir tests/build --output-on-failure
```

To run a single test suite:

```powershell
ctest --test-dir tests/build -R AmiDlmsApduTest --output-on-failure
```

The 16 test suites defined in `tests/CMakeLists.txt`:
1. `AmiCosemTest`: Validates the COSEM data model: 23-channel 15-minute Load Profile, 11-channel Instant Profile (`1.96.98.128.0.255`), standard Event Log #1 with 2 capture objects (`0.0.99.98.0.255`), Tamper Logbook with 3 capture objects (9 Tamper Modes), Power Quality Voltage Log Profiles R/S/T (`1.0.99.14.2..4.255`), Class 6/11/20/22 objects, Multi-Tariff, Power Quality, Limiter (Class 71), and Disconnect Control (Class 70).
2. `AmiSecurityTest`: Validates AES-128 NIST SP 800-38D KAT vectors, GMAC, constant-time comparison, and exhaustive 1-bit flip tampering tests (GMAC tag, AAD, IV).
3. `AmiDlmsAssociationTest`: ACSE association state machine (`AARQ`/`AARE`), conformance negotiation, ciphered initiate hooks, and disconnect handling.
4. `AmiFirmwareAuthPolicyTest`: Target firmware fail-closed authentication policy enforcement (blocks No-Auth/LLS, requires HLS5 with 16-byte challenge, verifies Secure Boot manifest checks).
5. `AmiDlmsApduTest`: Normal and ciphered xDLMS `GET`/`SET`/`ACTION` processing and readability verification across all 66 active objects in the Association View (including Instant Profile and PQ Voltage Log Profiles).
6. `AmiDlmsHdlcTest`: Zero-heap streaming HDLC parser, CRC-16 HCS/FCS validation, transparent `0x7E` flag resilience, and LLC framing.
7. `AmiDlmsPortTest`: Transport port layer, UART circular buffer, and cipher backend integration.
8. `AmiAde9000Test`: ADE9000 SPI metrology driver, register scaling, fail-closed SPI error handling, and snapshot feeder.
9. `AmiDlmsCipherTest`: Security Suite 0 (AES-128-GCM) ciphering engine, tag authentication, and replay/tampering rejection.
10. `AmiNvmCounterTest`: Wear-leveling NVM/TAMP anti-replay Invocation Counter and fail-closed `0xFFFFFFFF` saturation protection.
11. `AmiKeyTransferTest`: Atomic RFC 3394 AES Key Wrap remote key transfer in Secure World.
12. `AmiAntiBruteforceTest`: Brute-force mitigation per SPLN D5.002:2024 (progressive penalties of 5s, 10s, 20s, 40s, and permanent lockout after 5 failures).
13. `AmiBlockTransferTest`: General Block Transfer (GBT) xDLMS Datablock segmentation for large profile/buffer payloads.
14. `AmiDisconnectControlTest`: Disconnect Control Class 70 state machine (`Disconnected`, `Connected`, `Ready_for_Reconnection`) and Action methods.
15. `AmiTrustZoneBoundaryTest`: ARM Cortex-M33 CMSE NonSecure Callable (NSC) boundary validation, NULL pointer rejection, integer pointer overflow prevention (`uptr + size < uptr`), Secure World boundary-crossing checks, and NonSecure Flash write-permission rejection.
16. `AmiBootCryptoTest`: Cryptographic integrity validation for Pure C11 Secure Boot (`SHA-256` + `ECDSA P-256`) and key management on `Secure/Core/Src/secure_nsc.c`.

`AmiDlmsApduTest` also verifies that every object advertised in the Association View responds successfully to `GET` attribute 2 using test snapshots. The baseline objects and protocol profile under test are documented in [docs/security-rules/PROTOCOL_PROFILE.md](../docs/security-rules/PROTOCOL_PROFILE.md).

## Gurux Simulator

Build and run the simulator:

```powershell
cmake -S tests -B tests/build -G Ninja
cmake --build tests/build --target gurux_simulator
.\tests\build\host-tools\gurux_simulator.exe
```

The simulator accepts raw HDLC-over-TCP on `127.0.0.1:4059`. In Gurux DLMSDirector, use:

| Parameter | Simulator Setting |
|---|---|
| Media | Network / TCP |
| Host | `127.0.0.1` |
| Port | `4059` |
| Interface | HDLC (not Wrapper) |
| Referencing | Logical Name (LN) |
| Authentication | None (for development simulator profile only) |

Client/server addresses must match the simulator connection profile. If connection fails, inspect simulator logs and HDLC traces before changing other parameters.

**Important:** The host simulator's `No-Auth` mode differs from target firmware. Target firmware is configured to reject `No-Auth` and `LLS`. A passing CTT run with `Authentication level: None` against the simulator does **not** prove target firmware authentication policy. Firmware testing requires an authentication profile supported by the target integration.

The firmware policy test also verifies that only the HLS5-GMAC OID with a 16-byte client challenge is accepted; `No-Auth`, `LLS`, `HLS3/MD5`, `HLS4/SHA-1`, and challenges of any other length are rejected. This test verifies policy logic, not Secure-world cryptographic execution, key provisioning, or production security. Target profiles and gate statuses are recorded in [docs/security-rules/PROTOCOL_PROFILE.md](../docs/security-rules/PROTOCOL_PROFILE.md).

## OBIS: Catalog, Model, and Association View

Do not conflate these three distinct layers:

1. **OBIS Catalog** (`NonSecure/AMI/Inc/ami_obis_catalog.h`): Records OBIS codes referenced or planned by the project.
2. **COSEM Model**: Defines objects, attributes, and values that the implementation can process.
3. **Association View** (`association_view` in `ami_dlms_apdu.c`): Advertises available objects to DLMS clients.

An OBIS code present in the catalog is not necessarily exposed via association or readable via `GET`. When evaluating conformance, match the class ID, logical name/OBIS, class version, attribute, data type, scaler/unit, and access rights for every object promised by the product profile.

## Running Gurux Conformance Tests (CTT)

Start with a profile matching the endpoint under test:

- Select Logical Name referencing if that is the implemented and active connection mode.
- Configure authentication to match the endpoint: `No-Auth` is for the development simulator only; never weaken firmware security policy to clear a CTT failure.
- Ensure media/interface and addresses match the actual connection, and archive reports along with request/response traces.
- Run HDLC tests if the endpoint uses HDLC and the connection is established.
- Run application/`GET` tests for objects included in the Association View and product profile.

Setting `ExcludeBasicTests = True` skips Basic Tests; this is suitable for routine runs scoped to selected objects, but is **not** proof that Basic Tests pass. Setting it to `False` is useful for gap analysis. CTT may query standard classes/OBIS/attributes not provided by a scoped implementation; failures such as `UndefinedObject` indicate the requested object does not exist on the endpoint, not automatically a security bug. However, if that object is promised by the product profile, log it as an implementation gap and investigate.

Do not enable or interpret `SET`, `Action`, image transfer, `SN`, or other service tests as product requirements unless those features are part of the profile and implementation. Conversely, do not exclude tests covering features claimed as supported.

### Interpreting Results

- **Passed:** Tests reported as passing on that specific endpoint and configuration only.
- **Failed / Access Error / UndefinedObject:** Save request/response details, class, OBIS, attribute, and endpoint. Compare against advertised objects and expected access rights.
- **Ignored / Excluded:** Test was not executed; do not count as passing.
- **Authentication None:** Identify whether the endpoint is the simulator or target firmware before drawing security conclusions.

For example, a failing `Attribute0SupportedWithGet` check must be traced through the actual APDU request/response logs. A CTT summary alone is insufficient to pinpoint the object and correct fix.

## Testing Boundaries

- Snapshot/sample data is used to test value mapping and serialization. These tests do not compute or prove metrology accuracy.
- Host tests and the TCP simulator do not prove physical UART communication, hardware timing, FreeRTOS scheduling on silicon, or field durability.
- Passing CTT for a single configuration does not prove full product profile compliance or formal certification.
- Production HLS authentication testing requires approved cryptographic services, key provisioning, secure storage, and target configuration. Host tests do not replace Secure-world integration validation.

## Code Coverage Analysis (LCOV)

To measure and verify host code coverage:

```powershell
# Run via Docker / Linux
cmake -S tests -B tests/build_cov -G Ninja -DENABLE_COVERAGE=ON
cmake --build tests/build_cov
ctest --test-dir tests/build_cov
lcov --capture --directory tests/build_cov --output-file tests/build_cov/coverage_raw.info --ignore-errors inconsistent,gcov
lcov --extract tests/build_cov/coverage_raw.info '*/NonSecure/AMI/*' '*/Secure/*' --output-file tests/build_cov/coverage_filtered.info --ignore-errors unused,inconsistent
lcov --list tests/build_cov/coverage_filtered.info
python3 tests/scripts/lcov_to_markdown.py tests/build_cov/coverage_filtered.info docs/COVERAGE.md
```

Current firmware coverage results are saved in [docs/COVERAGE.md](../docs/COVERAGE.md):
- **Total Line Coverage**: **79.2%** (2,600 / 3,281 lines).
- **Total Function Coverage**: **95.7%** (154 / 161 functions).
- Critical security modules (`ami_security.c`, `ami_dlms_hdlc.c`, `ami_dlms_cipher.c`) achieve **> 89%** line coverage.

## Docker Reference

Docker instructions and container simulator setup are available in [docker_test/README.md](../docker_test/README.md).
