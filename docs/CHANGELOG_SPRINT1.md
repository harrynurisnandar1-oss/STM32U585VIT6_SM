# Sprint 1 — DLMS/COSEM read-only path

## Scope and limitations

This is a first implementation slice, not a complete DLMS/COSEM stack. It does not parse HDLC frames, validate client/server addresses, negotiate InitiateRequest/InitiateResponse user information, implement authentication or ciphering, process selective access, or manage invocation counters. Pass an APDU here only after an HDLC layer validates framing and addressing. This code is not ready for network exposure or product deployment.

The AARQ parser accepts the configured logical-name application context and the minimal no-auth development profile. The AARE builder returns association result and diagnostic fields. GET responses cover the local Class-3 and Class-8 model encodings.

## Standards references

Implementation scope is intended for review against IEC 62056-5-3, IEC 62056-6-2, and SPLN D3.006:2021. Verify BER/A-XDR encoding, diagnostic values, conformance negotiation, OBIS mapping, and product security requirements against controlled editions before claiming compliance. No conformance certification is claimed.

## Run host tests

From the project root, configure/build the host test project and run CTest:

```powershell
cmake -S tests -B tests/build -G Ninja
cmake --build tests/build --parallel
ctest --test-dir tests/build --output-on-failure
```

These checks validate host logic only; they do not validate FreeRTOS scheduling, STM32U585 TrustZone behavior, or UART/HDLC operation. Docker and CMock are not part of this configuration.
## Added 08/10/2026

- A bounded BER-TLV AARQ parser and AARE builder for the logical-name application-context OID.
- Association states (`IDLE`, `ASSOCIATED`, `ERROR`) for the unauthenticated, unciphered development profile. Authentication and ciphering fields are rejected.
- A bounded xDLMS `GET-Request-Normal` parser (without selective access) and `GET-Response-Normal` encoder for supported COSEM values and access errors.
- Read-only Class-8 Clock support at logical name `0.0.1.0.0.255`: attribute 2 (12-byte date-time), attribute 3 (time-zone minutes), and attribute 4 (status).
- Host test targets for the existing security/model checks and the new association, APDU, and Clock checks. `tests/CMakeLists.txt` registers them with CTest.


