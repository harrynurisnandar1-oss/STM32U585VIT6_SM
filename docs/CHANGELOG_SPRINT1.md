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

## Added 08/10/2026 

- Added bounded BER-TLV parsing for AARQ and building of AARE using logical-name application context OID `2.16.756.5.8.1.1`.
- Added initial unauthenticated development association flow and association state tracking.
- Added bounded xDLMS `GET-Request-Normal` parsing without selective access and `GET-Response-Normal` encoding for supported COSEM values and access errors.
- Added read-only Class-8 Clock support at logical name `0.0.1.0.0.255`: attribute 2 (12-byte date-time), attribute 3 (time-zone minutes), and attribute 4 (clock status).
- Added CTest host suites for COSEM model, security facade, DLMS association, and DLMS APDU logic.

## Added 09/10/2026 

### LLS and HLS association authentication

- Added LLS AARQ parsing for mechanism OID `2.16.756.5.8.2.1`, ACSE authentication requirement, and calling-authentication-value. Password verification is delegated to `verify_lls`; the password is not copied into the association context.
- Added configurable HLS mechanism OID and challenge exchange. The meter generates a 16-byte server challenge through the configured CSPRNG callback and returns it in AARE. Association stays `AUTH_PENDING` until `ami_dlms_process_hls_reply()` validates the client proof and generates the server proof for the client's challenge.
- Added bounded challenge storage, constant-time proof comparison, cleanup on reset/failure, and tests for successful/failed LLS plus the HLS challenge transition.
- Added callbacks for LLS verification, random challenge generation, and profile-specific HLS proof generation. Production callbacks are expected to bridge to protected credential/key storage and cryptographic services in the TrustZone Secure world.

### Security limitations

- HLS algorithm details vary by selected mechanism. The exact mechanism OID, key use, and proof calculation must be taken from the approved product profile. This code provides an integration hook; it does not claim a production HLS-GMAC implementation.
- LLS sends its password in the AARQ without cryptographic protection. Use LLS only on a physically trusted/protected link; use the product-approved HLS profile where stronger peer authentication is required.
- The legacy `ami_dlms_process_aarq()` function remains as a no-auth development entry point. Production code should call `ami_dlms_process_aarq_with_security()` and explicitly configure permitted authentication mechanisms.

## Added 10/10/2026 

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

## Added 12/10/2026

### SPLN D3.002:2021 OBIS review and date-range ordering

- Reviewed the provided `obis.md` inventory as the authoritative SPLN D3.002:2021 reference. Repeated logical names in the source table are recorded as source-profile entries and were not “corrected” based on neighboring OBIS patterns.
- Fixed Class 7 time-range ordering to compare calendar/time fields only (year through hundredths). Day-of-week, deviation, and clock status no longer affect range validation or row inclusion.
- The firmware COSEM model still implements only its explicitly listed register, Clock, Security Setup, and Profile Generic objects. The remaining SPLN OBIS objects require corresponding snapshot data, access semantics, and product mapping before they can be exposed; this review does not claim full SPLN object coverage.

### Three-phase measurement OBIS catalog

- Added the read-only Class-3 measurement OBIS entries from SPLN D3.002:2021 for neutral values, frequency, phase/total power factor, active/reactive/apparent power, and per-phase active/reactive/apparent energy.
- Added a bounded snapshot sample bank keyed by logical name. The COSEM model supplies the catalog-defined data type, scaler, and unit; metrology/board code supplies raw values and validity, so missing measurements return value-unavailable instead of a valid zero.
- The OBIS row for Q2 phase-S energy repeats `1.0.45.8.0.255` from Q1 phase-S in the supplied table. It is intentionally omitted from the additional catalog pending clarification against the controlled standard; no substitute OBIS is guessed.
- Demand registers, tariffs, power-quality metrics, event profiles, and control/configuration objects remain outside this measurement-register slice.

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
