# Sprint 1 — DLMS/COSEM read-only path

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

## [v0.0.0](https://github.com/harrynurisnandar1-oss/STM32U585VIT6_SM/releases/tag/v0.0.0) Added 08/10/2026 

- Added bounded BER-TLV parsing for AARQ and building of AARE using logical-name application context OID `2.16.756.5.8.1.1`.
- Added initial unauthenticated development association flow and association state tracking.
- Added bounded xDLMS `GET-Request-Normal` parsing without selective access and `GET-Response-Normal` encoding for supported COSEM values and access errors.
- Added read-only Class-8 Clock support at logical name `0.0.1.0.0.255`: attribute 2 (12-byte date-time), attribute 3 (time-zone minutes), and attribute 4 (clock status).
- Added CTest host suites for COSEM model, security facade, DLMS association, and DLMS APDU logic.

## [v1.0.0](https://github.com/harrynurisnandar1-oss/STM32U585VIT6_SM/releases/tag/v1.0.0) Added 09/10/2026 

### LLS and HLS association authentication

- Added LLS AARQ parsing for mechanism OID `2.16.756.5.8.2.1`, ACSE authentication requirement, and calling-authentication-value. Password verification is delegated to `verify_lls`; the password is not copied into the association context.
- Added configurable HLS mechanism OID and challenge exchange. The meter generates a 16-byte server challenge through the configured CSPRNG callback and returns it in AARE. Association stays `AUTH_PENDING` until `ami_dlms_process_hls_reply()` validates the client proof and generates the server proof for the client's challenge.
- Added bounded challenge storage, constant-time proof comparison, cleanup on reset/failure, and tests for successful/failed LLS plus the HLS challenge transition.
- Added callbacks for LLS verification, random challenge generation, and profile-specific HLS proof generation. Production callbacks are expected to bridge to protected credential/key storage and cryptographic services in the TrustZone Secure world.

### Security limitations

- HLS algorithm details vary by selected mechanism. The exact mechanism OID, key use, and proof calculation must be taken from the approved product profile. This code provides an integration hook; it does not claim a production HLS-GMAC implementation.
- LLS sends its password in the AARQ without cryptographic protection. Use LLS only on a physically trusted/protected link; use the product-approved HLS profile where stronger peer authentication is required.
- The legacy `ami_dlms_process_aarq()` function remains as a no-auth development entry point. Production code should call `ami_dlms_process_aarq_with_security()` and explicitly configure permitted authentication mechanisms.

## [v1.1.0](https://github.com/harrynurisnandar1-oss/STM32U585VIT6_SM/releases/tag/v1.1.0) Added 10/10/2026 

### Global-ciphered APDU after HLS

- HLS-associated sessions now reject plaintext GET requests. The new APDU entry point accepts only global-ciphered GET (`GLO-GET-REQUEST`, `0xC8`) and SET (`GLO-SET-REQUEST`, `0xC9`) requests; it authenticates/decrypts the inner APDU and emits global-ciphered GET/SET responses (`0xCC` / `0xCD`).
- Added a Secure-world SAL backend contract for AES-GCM processing. It receives the security-control byte and invocation counter and is responsible for deriving nonce/AAD from the approved DLMS profile, system titles, and Secure-held keys.
- Added fail-closed checks for the configured security-control value, authentication tag, and receive invocation counter. Receive counter acceptance must be atomic/persistent; transmit counters must be allocated durably before encryption to prevent reuse.
- Ciphered SET requests are decrypted and returned as encrypted `read-write-denied` responses because the meter's COSEM model is intentionally read-only.
- Added host tests for plaintext rejection after HLS, ciphered GET response generation, tampered-tag rejection, and ciphered read-only SET denial. These tests use a deliberately non-cryptographic fake backend to test framing/state only; they do not validate AES-GCM implementation.

### Integration requirements

- The Secure backend must implement the product-selected DLMS security suite, key selection, system-title handling, nonce/AAD construction, and 128-bit tag policy. The receive-counter callback must reject replay/rollback and commit only authenticated counters; the transmit callback must persist a fresh counter before use.
- No concrete STM32U585 TrustZone AES-GCM backend or provisioned keys are included here. Until that SAL callback is implemented and validated against approved DLMS test vectors, do not use the test backend or treat this feature as production-ready.

## Added 12/10/2026

### Profile Generic Class 7 for event and billing history

- Added two read-only Profile Generic objects to the COSEM model: the standard event-log logical name `0.0.99.98.0.255` and a billing-history profile configured as `1.0.98.1.0.255`. The billing OBIS instance is a product-profile choice and must be checked against the applicable PLN/SPLN meter specification before release.
- Added a bounded, fixed-capacity snapshot for up to eight event records and eight billing-period records. Event rows contain a 12-byte COSEM date-time and event code; billing rows contain a date-time plus active-import and active-export energy counters. The board/metrology layer supplies these rows and manages timestamp generation and rollover; no fake records are generated by the model.
- Implemented read-only Class 7 attributes 1–8: logical name, A-XDR buffer array, capture-object list, trigger-based capture period (`0`), FIFO sort method, timestamp sort object, entries in use, and profile capacity. Event capture objects describe Clock and event-code data; billing capture objects describe Clock and the active-import/export registers.
- Added A-XDR passthrough for already encoded array/structure values so Class 7 buffer and capture-object attributes can travel through the existing GET response path, including the global-ciphered path after HLS.
- The snapshot arrays are provided oldest first; producers must keep the counts at or below `AMI_COSEM_PROFILE_MAX_ENTRIES` and handle circular-buffer policy. Reads of buffer and entries-in-use return value-unavailable until their corresponding profile-validity bit is set.

### Scope notes

- This adds basic full-buffer reads only. Selective access by entry range or date range, profile capture/reset methods, persistent storage, and a FreeRTOS producer task are not implemented in this slice.
- Class 7 A-XDR encoding and the configured billing OBIS must be reviewed against the controlled IEC 62056 and product/SPLN profile editions. This change alone does not establish DLMS conformance.

## Added 13/10/2026

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
