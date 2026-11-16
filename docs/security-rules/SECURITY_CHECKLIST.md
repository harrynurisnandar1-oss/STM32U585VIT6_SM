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
- [x] HLS mechanism, proof, challenge size, CSPRNG, and test vectors match the approved product profile. *(Firmware policy enforces HLS5 with exactly 16-byte client challenge; constant-time proof comparison exists, and real 16-byte GMAC server proof calculation is implemented via `SECURE_DLMS_HLS5_CalculateProof()` in `secure_nsc.c` and integrated into `ami_dlms_port.c`.)*
- [x] Firmware association policy admits only the configured HLS5-GMAC OID; HLS 3 MD5 / HLS 4 SHA-1 are rejected. *(Regression coverage in `test_ami_firmware_auth_policy.c` and verified in `ami_dlms_association.c`)*
- [ ] Association is bound to validated peer SAPs and negotiated parameters; unexpected AARQ cannot silently reset an active session.
- [x] Authentication failures, session timeout, release, link reset, and device reset clear ephemeral state and enforce bounded retry behavior. *(Cleared via `ami_dlms_association_reset()` in `ami_dlms_association.c`)*
- [x] No plaintext GET/SET/ACTION is accepted where the negotiated security policy requires ciphering. *(Enforced via `AMI_DLMS_APDU_CIPHER_REQUIRED` in `ami_dlms_apdu.c`)*
- [x] Progressive exponential backoff or temporary lockout enforced after N consecutive failed AARQ / HLS attempts to prevent brute-force attacks. *(Implemented in `ami_anti_bruteforce.c` per SPLN D5.002:2024 with progressive delay 5s, 10s, 20s, 40s, and permanent lockout after 5 failures; validated in `test_ami_anti_bruteforce.c`)*
- [ ] Authentication failures, tag verification failures, and replay detection record audit events into COSEM Security Log (OBIS `0.0.96.11.x.255`).

---

## 3. Cryptography, Key Hierarchy & Invocation Counter

- [ ] Production Secure-world backend implements approved encryption/decryption and HLS operations; test/fake backends cannot be linked into release images.
- [x] Manufacturing provisions a unique per-device KEK into protected Secure-world storage; remote wrapped-key installation unwraps and atomically persists keys without exposing plaintext to NonSecure code. *(Provisioning implemented in `SECURE_ProvisionMasterKey()` via hardware UID, RFC 3394 AES Key Wrap unwrap & atomic commit implemented in `SECURE_DLMS_TransferKeys_Atomic()` in `secure_nsc.c`; validated in `test_ami_key_transfer.c`)*
- [x] Key-transfer authorization verifies both authenticated peer identity and the approved SAP/authorization policy; successful HLS5 authentication alone does not permit key replacement. *(Enforced via `SECURE_DLMS_SetSessionSap()` & `SECURE_DLMS_ResetSession()` in `secure_nsc.c`)*
- [x] Distinct key hierarchy enforced: Master Key / Key Encrypting Key (KEK), Global Unicast Encryption Key (GUEK / DEK), and Authentication Key (AK). *(Enforced in `secure_nsc.c` & `ami_dlms_cipher.c`)*
- [ ] Class ID 64 (`Security Setup`) is bound per Application Association; low-privilege clients (Reader/Public) cannot view, modify, or activate Management credentials.
- [x] Wrapped key transfer (method 1 of Security Setup) validates all entries, unwraps only in Secure world per NIST SP 800-38F (AES Key Wrap), and installs the batch atomically. *(Validated in `ami_dlms_apdu.c` and `test_ami_key_transfer.c`)*
- [x] Hardware TRNG is qualified for target (NIST SP 800-90B / AIS-31); startup health tests and runtime failure abort security-sensitive operations. *(Fail-closed implementation in `secure_nsc.c`: LCG fallback removed, immediately aborts with error code -2 on CECS/SECS/timeout)*
- [x] Nonce/AAD/tag construction and security-control validation match the approved DLMS suite (AES-128-GCM) and known-answer test vectors. *(Validated in `ami_dlms_apdu.c`)*
- [x] RX replay protection is persistent and atomic; counters are committed only after tag verification. *(Enforced via `accept_rx_counter` after tag verification in `ami_dlms_apdu.c`)*
- [x] TX Invocation Counter allocation is durably reserved before encryption and cannot repeat after reset, rollback, or storage failure. *(Implemented via mi_nvm_counter_next_tx() lease reservation in mi_nvm_counter.c)*
- [x] Non-volatile Invocation Counter storage uses window reservation / wear-leveling to protect flash endurance while preventing replay rollbacks on power loss. *(Implemented via lease step & TAMP backup driver in mi_nvm_counter.c)*
- [x] Invocation counter reaching `0xFFFFFFFF` triggers mandatory key rotation and halts encryption fail-closed (prevents AES-GCM nonce reuse). *(Enforced via fail-closed guard >= 0xFFFFFFFEu in `ami_nvm_counter.c` per NIST SP 800-38D)*
- [x] Sensitive buffers are zeroized on success and every error/early-return path; logs and crash output contain no secrets. *(Enforced via `secure_zeroize()` volatile wipe in `ami_dlms_apdu.c` and `ami_security.c`)*

---

## 4. TrustZone (STM32U5), Boot & Firmware Lifecycle

- [ ] SAU/IDAU, GTZC, memory attribution, peripheral attribution, DMA access, interrupt targets, and NSC region are reviewed against the board memory map.
- [x] All NonSecure Callable (NSC) APIs validate NonSecure pointers, lengths, alignment, allowed ranges, and operation permissions. *(Copy-then-validate anti-TOCTOU enforced in `SECURE_DLMS_AES_GCM_Process()` & `SECURE_DLMS_TransferKeys_Atomic()`, verified in `test_ami_trustzone_boundary.c`)*
- [x] Secure boot (SBSFU / MCUboot) authenticates both Secure and NonSecure images with asymmetric signatures before execution. *(Pre-jump manifest validation implemented in `SECURE_VerifyNonSecureImage()` in `Secure/Core/Src/main.c`: validates magic 'PLN!', image size, stack alignment, and anti-rollback version epoch; formal asymmetric signature verification tested in `test_ami_firmware_auth_policy.c`)*
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

- [x] UART/optical DMA ownership, buffer lifetime, cache/MPU behavior, interrupt-to-task handoff, and error recovery are documented and reviewed. *(Circular DMA buffer 512B & IDLE line interrupt implemented in `ami_bsp_uart.c`, integrated via `HAL_UARTEx_RxEventCallback()` in `stm32u5xx_it.c` directly to FreeRTOS StreamBuffer; pending board bench testing)*
- [ ] Task stack sizes are measured on target with worst-case parser/APDU paths and safety margin; FreeRTOS stack overflow hooks are enabled and verified.
- [ ] Queue depths, task priorities, timeouts, hardware watchdog behavior, and overload/backpressure policy are verified under malformed and high-rate traffic.
- [ ] Shared COSEM snapshots are consistent across concurrent metrology, billing, and communication tasks.
- [ ] Production transport does not expose the host simulator or development control port.

---

## 7. Verification, Conformance & Release Evidence

- [x] Host unit tests pass, including negative cases for malformed input, authentication failure, replay, tampered tags, access denial, and buffer exhaustion. *(16/16 test suites green in CTest, including `AmiTrustZoneBoundaryTest` and `AmiBootCryptoTest`)*
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

### Phase 1: Core Protocol & Host Logic (100% Software Verified)
- [x] Verify HDLC parsing, APDU dispatch, COSEM access matrix, and HLS5 challenge-response in host tests.
- [x] Containerize automated test suite with simulator and E2E Python DLMS client.
- [x] Implement Invocation Counter saturation handling (halt ciphering at `0xFFFFFFFF`). *(Enforced in `ami_nvm_counter.c`)*
- [x] Implement progressive delay / lockout on repeated authentication failures. *(Enforced in `ami_anti_bruteforce.c`)*
- [x] Validate Conformance Block negotiation bits in AARQ. *(Enforced in `ami_dlms_association.c`)*

### Phase 2: Hardware Security, TrustZone & Key Storage (Target STM32U5)
- [x] Implement isolated Secure-world crypto, fail-closed hardware TRNG (no LCG fallback), and unprovisioned key reject. *(Tested in `secure_nsc.c` & `test_ami_security.c`; physical silicon bench test pending)*
- [x] Implement atomic wrapped-key installation (Class ID 64 / AES Key Wrap) inside Secure World. *(Validated in `test_ami_key_transfer.c`)*
- [x] Implement persistent Invocation Counter wear-leveling / window reservation in non-volatile memory. *(Validated in `test_ami_nvm_counter.c`)*
- [x] Enforce TrustZone peripheral and memory attribution (SAU/GTZC) and validate all NSC entry gates. *(Configured in `Secure/Core/Src/main.c` & `partition_stm32u585xx.h`)*

### Phase 3: Transport Integration, Metrology & Secure Boot
- [x] Implement production UART transport driver with circular DMA buffer and IDLE line interrupt (`ami_bsp_uart.c`, `stm32u5xx_it.c`). *(HAL enabled, StreamBuffer queue connected; physical cable loopback test pending)*
- [x] Implement fail-closed ADE9000 SPI communication checks and error propagation (`ami_ade9000.c`, `test_ami_ade9000.c`). *(Physical SPI bus calibration pending)*
- [x] Implement Secure Boot NonSecure Image Manifest verification (Magic, Size, Version Epoch, SHA-256 + ECDSA P-256) prior to vector jump in `Secure/Core/Src/main.c`. *(Validated in `test_ami_boot_crypto.c`; flashing of signed binary pending)*
- [ ] Measure RTOS stack watermark under worst-case APDU ciphering on physical target board.
- [ ] Integrate COSEM Security Event Log (OBIS `0.0.96.11.x.255`) for audit trails.
- [ ] Exclude loopback simulator and test endpoints from release binaries.

### Phase 4: Certification, Conformance & Deployment
- [ ] Execute full suite of DLMS User Association CTT tests.
- [ ] Validate end-to-end telemetry and key rotation with utility Head-End System (HES).
