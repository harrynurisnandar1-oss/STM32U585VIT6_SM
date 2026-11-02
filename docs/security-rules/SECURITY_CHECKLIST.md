# Security Checklist (DLMS/COSEM IEC 62056 & STM32U5 TrustZone AMI Smart Meter)

Use this checklist for architecture design reviews, implementation verification, and release candidates. Record evidence, owner, and review date for each item. `Open` is the initial status for this repository; the existence of a code path or host test is not sufficient evidence of target security.

---

## 1. Product Profile, Conformance & Threat Model

- [ ] Product/security owner approves the DLMS/COSEM and SPLN/utility profile, controlled standards editions, cipher suite, system-title rules, OBIS map, and conformance settings. *(E2-selected draft is recorded in `docs/security-rules/PROTOCOL_PROFILE.md`: LN, client SAP 16 with HLS5-GMAC/Suite 0, server 1/1, and the 12-object host view. Formal approval remains open.)*
- [ ] Threat model risks have owners, mitigations, residual-risk approval, and documented deployment assumptions.
- [ ] Production communication interfaces, client identities, physical access assumptions, and service/maintenance modes are documented.
- [x] COSEM access-control matrix covers every exposed object, attribute, and method. *(Implemented via `association_view[]` in `ami_dlms_apdu.c`)*
- [ ] Public Client (SAP 16) is strictly read-only and restricted to basic device identity, logical name, clock, and conformance block; no sensitive configuration or control is exposed without authentication.
- [ ] Conformance block negotiation in AARQ/AARE validates requested services against association capabilities; unauthorized services are rejected.

---

## 2. Authentication, Session Management & Anti-Abuse

- [ ] Production uses an explicit authentication allow-list per client SAP; no-auth is strictly forbidden on Management Association (SAP 1) and maintenance profiles.
- [ ] Selected E2 firmware profile admits only HLS5-GMAC on client SAP 16 and validates the expected server address; legacy HLS and unauthenticated/LLS associations are rejected.
- [x] Firmware DLMS port rejects associations when no authentication policy is configured; the legacy no-auth `ami_dlms_process_aarq()` entry point is host-only.
- [ ] Legacy/insecure authentication remains disabled by explicit production policy for all restricted client SAPs.
- [x] Firmware association code rejects no-auth and LLS regardless of the configured callbacks; host-only builds retain these modes for tests/simulator.
- [x] LLS (Low-Level Security) is disabled on all firmware links until a formally approved protected-link exception is implemented.
- [ ] HLS mechanism, proof, challenge size, CSPRNG, and test vectors match the approved product profile. *(Firmware policy now admits only HLS5 with an exactly 16-byte client challenge; constant-time proof comparison exists, but production HLS crypto/key service is not integrated.)*
- [x] Firmware association policy admits only the configured HLS5-GMAC OID; HLS 3 MD5 / HLS 4 SHA-1 are rejected. *(Regression coverage in `test_ami_firmware_auth_policy.c`; Secure-world HLS5 implementation remains open.)*
- [ ] Association is bound to validated peer SAPs and negotiated parameters; unexpected AARQ cannot silently reset an active session.
- [x] Authentication failures, session timeout, release, link reset, and device reset clear ephemeral state and enforce bounded retry behavior. *(Cleared via `ami_dlms_association_reset()` in `ami_dlms_association.c`)*
- [x] No plaintext GET/SET/ACTION is accepted where the negotiated security policy requires ciphering. *(Enforced via `AMI_DLMS_APDU_CIPHER_REQUIRED` in `ami_dlms_apdu.c`)*
- [ ] Progressive exponential backoff or temporary lockout enforced after N consecutive failed AARQ / HLS attempts to prevent brute-force attacks.
- [ ] Authentication failures, tag verification failures, and replay detection record audit events into COSEM Security Log (OBIS `0.0.96.11.x.255`).

---

## 3. Cryptography, Key Hierarchy & Invocation Counter

- [ ] Production Secure-world backend implements approved encryption/decryption and HLS operations; test/fake backends cannot be linked into release images.
- [ ] Manufacturing provisions a unique per-device KEK into protected Secure-world storage; remote wrapped-key installation unwraps and atomically persists keys without exposing plaintext to NonSecure code. *(Remote key transfer is selected in `docs/security-rules/PROTOCOL_PROFILE.md`; production provisioning/storage is not implemented.)*
- [ ] Key-transfer authorization verifies both authenticated peer identity and the approved SAP/authorization policy; successful HLS5 authentication alone does not permit key replacement.
- [ ] Distinct key hierarchy enforced: Master Key / Key Encrypting Key (KEK), Global Unicast Encryption Key (GUEK / DEK), and Authentication Key (AK).
- [ ] Class ID 64 (`Security Setup`) is bound per Application Association; low-privilege clients (Reader/Public) cannot view, modify, or activate Management credentials.
- [ ] Wrapped key transfer (method 1 of Security Setup) validates all entries, unwraps only in Secure world per NIST SP 800-38F (AES Key Wrap), and installs the batch atomically.
- [ ] Hardware TRNG is qualified for target (NIST SP 800-90B / AIS-31); startup health tests and runtime failure abort security-sensitive operations.
- [x] Nonce/AAD/tag construction and security-control validation match the approved DLMS suite (AES-128-GCM) and known-answer test vectors. *(Validated in `ami_dlms_apdu.c`)*
- [x] RX replay protection is persistent and atomic; counters are committed only after tag verification. *(Enforced via `accept_rx_counter` after tag verification in `ami_dlms_apdu.c`)*
- [ ] TX Invocation Counter allocation is durably reserved before encryption and cannot repeat after reset, rollback, or storage failure.
- [ ] Non-volatile Invocation Counter storage uses window reservation / wear-leveling to protect flash endurance while preventing replay rollbacks on power loss.
- [ ] Invocation counter reaching `0xFFFFFFFF` triggers mandatory key rotation and halts encryption fail-closed (prevents AES-GCM nonce reuse).
- [x] Sensitive buffers are zeroized on success and every error/early-return path; logs and crash output contain no secrets. *(Enforced via `secure_zeroize()` volatile wipe in `ami_dlms_apdu.c` and `ami_security.c`)*

---

## 4. TrustZone (STM32U5), Boot & Firmware Lifecycle

- [ ] SAU/IDAU, GTZC, memory attribution, peripheral attribution, DMA access, interrupt targets, and NSC region are reviewed against the board memory map.
- [ ] All NonSecure Callable (NSC) APIs validate NonSecure pointers, lengths, alignment, allowed ranges, and operation permissions.
- [ ] Secure boot (SBSFU / MCUboot) authenticates both Secure and NonSecure images with asymmetric signatures before execution.
- [ ] Firmware update signatures, product identity, version checks, rollback counter prevention, interrupted-update recovery, and key rotation are verified via COSEM Image Transfer (Class ID 18).
- [ ] Production debug/readout protection (RDP Level 1/2) and hardware tamper detection are enabled; JTAG/SWD debug access is disabled on deployed units.
- [ ] Release build excludes development credentials, no-auth modes, fake crypto, verbose APDU/frame logging, and host simulator code.

---

## 5. Input Handling, Protocol State & Metrology Integrity

- [x] HDLC validates exact frame length, address encoding, HCS/FCS, expected destination/source SAPs, frame type, `N(S)`/`N(R)`, and negotiated limits. *(Validated in `ami_dlms_hdlc.c`)*
- [x] Invalid or incomplete frames resynchronize safely and cannot hold parser state or buffers indefinitely. *(Resync & bounded frame buffer in `ami_dlms_hdlc.c`)*
- [x] BER, ACSE, A-XDR, APDU, selective access, and key-transfer parsers check every length before reading and use overflow-safe arithmetic. *(Implemented in `ami_dlms_association.c` & `ami_dlms_apdu.c`)*
- [x] Unsupported selectors, malformed requests, oversized values, and invalid object access fail closed without partial responses. *(Fail-closed error generation in `ami_dlms_apdu.c`)*
- [ ] Profile generic buffer counts and metrology sample registers are checked before array access; producer and consumer snapshot ownership is race-safe.
- [ ] Per-client buffers, queues, retries, logging, and CPU work are bounded; malformed traffic cannot exhaust memory or starve metrology tasks.
- [x] SET/ACTION behavior matches the approved access matrix, including key-transfer authorization and read-only policy. *(Denied read-write policy in `ami_dlms_apdu.c`)*

---

## 6. RTOS, Transport & Availability

- [ ] UART/optical DMA ownership, buffer lifetime, cache/MPU behavior, interrupt-to-task handoff, and error recovery are documented and reviewed.
- [ ] Task stack sizes are measured on target with worst-case parser/APDU paths and safety margin; FreeRTOS stack overflow hooks are enabled and verified.
- [ ] Queue depths, task priorities, timeouts, hardware watchdog behavior, and overload/backpressure policy are verified under malformed and high-rate traffic.
- [ ] Shared COSEM snapshots are consistent across concurrent metrology, billing, and communication tasks.
- [ ] Production transport does not expose the host simulator or development control port.

---

## 7. Verification, Conformance & Release Evidence

- [x] Host unit tests pass, including negative cases for malformed input, authentication failure, replay, tampered tags, access denial, and buffer exhaustion. *(9/9 test suites green in CTest)*
- [x] Automated containerized Docker test runner verified with 100% pass rate across all unit tests and End-to-End DLMS/COSEM HDLC-over-TCP integration test (via `docker compose up --build`).
- [ ] Parsers have fuzz/property tests with sanitizers or equivalent host instrumentation; findings are triaged and fixed.
- [ ] Hardware tests verify TrustZone isolation, Secure crypto vectors, UART/HDLC operation, reset behavior, power-loss counter durability, and recovery.
- [ ] Security tests verify unexpected peer addresses, sequence errors, reconnect behavior, session timeout, and concurrent requests.
- [ ] Official DLMS Conformance Test Tool (CTT) verification passed without critical errors.
- [ ] Interoperability validated against utility Head-End System (HES) and Gurux DLMSDirector across optical probe and remote WAN channels.
- [ ] Static analysis/compiler warnings and dependency changes are reviewed; release binary and configuration are archived with traceable source revision.
- [ ] Independent security review has no unresolved Critical/High findings, or residual risks have written approval from the product security owner.
- [ ] Release notes state tested board revision, profile/configuration, known limitations, and the exact evidence for security claims.

---

## 8. Current Implementation Roadmap & Priority Backlog

### Phase 1: Core Protocol & Host Logic (In Progress / Partially Verified)
- [x] Verify HDLC parsing, APDU dispatch, COSEM access matrix, and HLS5 challenge-response in host tests.
- [x] Containerize automated test suite with simulator and E2E Python DLMS client.
- [ ] Implement Invocation Counter saturation handling (halt ciphering at `0xFFFFFFFF`).
- [ ] Implement progressive delay / lockout on repeated authentication failures.
- [ ] Validate Conformance Block negotiation bits in AARQ.

### Phase 2: Hardware Security, TrustZone & Key Storage (Target STM32U5)
- [ ] Implement and validate Secure-world crypto, hardware TRNG health checks, and protected key storage.
- [ ] Implement atomic wrapped-key installation (Class ID 64 / AES Key Wrap) inside Secure World.
- [ ] Implement persistent Invocation Counter wear-leveling / window reservation in non-volatile memory.
- [ ] Enforce TrustZone peripheral and memory attribution (SAU/GTZC) and validate all NSC entry gates.

### Phase 3: Transport Integration & RTOS Robustness
- [ ] Integrate production UART/Optical transport driver with DMA boundary checks.
- [ ] Measure RTOS stack watermark under worst-case APDU ciphering and malformed traffic.
- [ ] Integrate COSEM Security Event Log (OBIS `0.0.96.11.x.255`) for audit trails.
- [ ] Exclude loopback simulator and test endpoints from release binaries.

### Phase 4: Certification, Conformance & Deployment
- [ ] Execute full suite of DLMS User Association CTT tests.
- [ ] Validate end-to-end telemetry and key rotation with utility Head-End System (HES).
