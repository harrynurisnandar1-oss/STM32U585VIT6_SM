# Threat Model

## System under review

The target is a three-phase postpaid smart meter using STM32U585VIT6, Secure and NonSecure firmware images, a DLMS/COSEM application stack, HDLC framing, metrology inputs, and billing/event snapshots. A host-only Gurux-compatible simulator is also present. The current firmware starts the NonSecure image and initializes FreeRTOS, but its default task only delays; the repository's protocol modules are not yet connected to a device UART/transport task.

The model describes both the current repository and the intended integrated meter. A host simulator finding does not, by itself, prove the same path is reachable on deployed hardware.

## Assets

| Asset | Security property |
|---|---|
| Meter readings and billing history | Confidentiality and integrity |
| Event log and clock | Integrity, ordering, and availability |
| DLMS credentials, keys, KEK, system titles | Confidentiality and controlled lifecycle |
| Invocation counters and session challenges | Replay resistance and freshness |
| Secure boot image and TrustZone configuration | Authenticity and isolation |
| Meter service availability | Bounded, resilient communications and measurement |
| Provisioning and firmware-update process | Authenticity, authorization, and rollback resistance |

## Trust boundaries and entry points

1. **External client to HDLC transport:** serial/optical or other product link; the target transport is not yet integrated. Inputs include flags, variable-length addresses, control fields, CRCs, LLC, and APDUs.
2. **Protocol parser to COSEM model:** untrusted request fields select objects, attributes, methods, and profile ranges.
3. **NonSecure to Secure/NSC boundary:** cryptographic calls, RNG, key installation, pointer/length parameters, and key handles. The product backend is not yet present.
4. **Metrology/board layer to COSEM snapshot:** measurement values, validity flags, timestamps, and bounded event/billing arrays.
5. **Build/provisioning/update boundary:** source, build configuration, signing keys, image delivery, and debug access.
6. **Host simulator boundary:** local TCP listener at 127.0.0.1:4059; it uses a no-auth association helper and development data.

## Adversaries and assumptions

- A nearby or remote party able to access the meter's communication port can send arbitrary bytes, malformed frames, repeated requests, or replayed traffic.
- A legitimate but compromised or malicious DLMS client can issue validly framed requests outside the intended access profile.
- An attacker with physical access may probe debug interfaces, reset/power-cycle the device, or interrupt writes unless lifecycle controls prevent it.
- A compromised NonSecure application may attempt to misuse Secure services or pass crafted pointers and sizes.
- A build or provisioning compromise may replace firmware or embed test credentials.
- The STM32 hardware attribution, trusted provisioning process, and final product network isolation are assumptions that require verification on the actual board and deployment.

## Threats, risk, and attack vectors

Risk ratings are initial engineering priorities, not measured exploitability. Severity should be revisited after transport integration and product-profile selection.

| ID | Threat / attack vector | Impact | Initial risk | Existing evidence / required response |
|---|---|---|---|---|
| T1 | No-auth association via the legacy `ami_dlms_process_aarq()` helper or simulator | Unauthorized reading of meter/billing data | High | Helper sets `allow_no_auth = 1`; simulator calls it. Remove production use and make auth policy explicit. |
| T2 | LLS password sent in an unprotected AARQ | Credential disclosure and reuse | High when link is observable | Changelog documents cleartext LLS. Disable on exposed links or use approved HLS/security suite. |
| T3 | Missing Secure crypto backend, key provisioning, and durable counters | Forgery, nonce reuse, replay, key compromise | Critical before production | APDU relies on callbacks for decrypt, counter acceptance, TX counter, and transfer; implement and validate in Secure world. |
| T4 | Forged, replayed, or misaddressed HDLC frames | Unauthorized service invocation, session confusion, denial of service | High after transport integration | Parser checks frame integrity, but integration must enforce peer SAPs and sequence/session policy before APDU dispatch. |
| T5 | Malformed BER/A-XDR/HDLC input or oversized profile selection | Parser failure, resource exhaustion, memory corruption | High | Bounded parsers exist; still require fuzzing and target-specific checks across every entry point. |
| T6 | AARQ resets an active association in the simulator | Session interruption / local denial of service | Medium, simulator scope | `gurux_simulator.c` resets association on each AARQ. Use a session policy that rejects or safely replaces state. |
| T7 | Compromised NonSecure caller abuses NSC APIs | Secure data disclosure or unauthorized key operation | Critical if NSC crypto is added | Validate address ranges, sizes, permissions, operation state, and key handles at every NSC call. |
| T8 | Power loss or reset rolls back counters or interrupts key installation | Replay acceptance, nonce reuse, inconsistent keys | High | Counter persistence and atomic key batch installation are explicit callback requirements but are not implemented. |
| T9 | DMA/task race changes snapshots or parser buffers during processing | Inconsistent billing data, integrity failure, crashes | Medium | Define ownership, locking, and snapshot-copy rules when device transport and metrology tasks are integrated. |
| T10 | Debug interface or unsigned/rollback firmware update is abused | Persistent compromise or extraction | High | Secure boot/update and production lifecycle controls are not established by current protocol tests; verify platform configuration and boot chain. |
| T11 | Diagnostic logging exposes protocol data or saturates CPU/storage | Secret disclosure or denial of service | Medium | Simulator logs frame/APDU bytes; keep it development-only and ensure production logging is bounded and redacted. |
| T12 | Small task stack or unbounded communication work exhausts RTOS resources | Reset, service loss, or corrupted state | Medium | Default FreeRTOS task stack is 512 bytes and currently idle. Measure high-water mark after real handlers are added. |

## Attack scenarios to validate

### Unauthenticated read in the host simulator

1. Start the host simulator on the local machine.
2. Connect with a DLMS client configured for no authentication.
3. Send the project's no-auth AARQ fixture, then a GET request for an exposed register.
4. Confirm whether an AARE and meter value are returned.

The AARQ fixture is in `tests/test_ami_dlms_association.c`; the simulator's association path is in `tests/gurux_simulator.c`. This scenario demonstrates simulator behavior only and must not be presented as proof that production hardware currently exposes a listener.

### Counter rollback/reuse

With a backend test harness, authenticate a ciphered request, reset or power-cycle the target, then replay the same invocation counter and capture the next encrypted response. The expected result is rejection of the replay and a TX counter that cannot repeat under the same key. Repeat with power interruption during counter reservation and key installation.

### HDLC address and sequence enforcement

Send otherwise valid frames with an unexpected destination SAP, unexpected source SAP, repeated `N(S)`, stale `N(R)`, and a valid FCS. Confirm the device rejects them before any APDU callback and does not corrupt the established session.

## Mitigation priorities

1. Define and approve the production DLMS authentication/security profile and disable no-auth/LLS where not permitted.
2. Implement Secure-world crypto, protected keys, persistent anti-replay counters, and atomic wrapped-key installation.
3. Integrate HDLC transport with strict address, sequence, negotiated-size, timeout, and per-session checks.
4. Verify TrustZone attribution, Secure boot, signed update, rollback protection, debug lifecycle, and key provisioning on the actual MCU.
5. Add parser fuzzing, negative protocol tests, power-failure tests, concurrency checks, and target hardware verification before release.

