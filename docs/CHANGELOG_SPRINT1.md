# Sprint 1 — DLMS/COSEM read-only path

## Scope and limitations

Sprint 1 is an initial protocol slice, not a complete DLMS/COSEM stack. It does not parse HDLC frames, validate client/server addresses, negotiate an InitiateRequest/InitiateResponse user-information payload, implement DLMS ciphering, process selective access, or manage invocation counters. Pass application APDUs here only after the HDLC layer validates framing and addressing. This code is not ready for network exposure or product deployment.

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
