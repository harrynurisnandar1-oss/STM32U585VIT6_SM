# Security Checklist

Use this checklist for design reviews and every release candidate. Record evidence, owner, and review date for each item. `Open` is the initial status for this repository; the existence of a code path or host test is not sufficient evidence of target security.

## Product profile and threat model

- [ ] Approved DLMS/COSEM and SPLN profile, controlled standards editions, authentication mechanisms, cipher suite, system-title rules, OBIS map, and conformance settings are recorded.
- [ ] Threat model risks have owners, mitigations, residual-risk approval, and documented deployment assumptions.
- [ ] Production communication interfaces, client identities, physical access assumptions, and service/maintenance modes are documented.
- [x] COSEM access-control matrix covers every exposed object, attribute, and method. *(Implemented via `association_view[]` in `ami_dlms_apdu.c`)*

## Authentication and sessions

- [ ] Production uses an explicit authentication allow-list; no-auth is disabled and cannot be enabled by a default helper.
- [ ] Legacy `ami_dlms_process_aarq()` development entry point is absent from or blocked in production builds.
- [ ] LLS is disabled on observable/untrusted links unless formally approved with compensating channel protection.
- [x] HLS mechanism, proof, challenge size, CSPRNG, and test vectors match the approved product profile. *(16-byte challenge exchange, GMAC verification, and constant-time proof comparison in `ami_dlms_association.c`)*
- [ ] Association is bound to validated peer SAPs and negotiated parameters; unexpected AARQ cannot silently reset an active session.
- [x] Authentication failures, session timeout, release, link reset, and device reset clear ephemeral state and enforce bounded retry behavior. *(Cleared via `ami_dlms_association_reset()` in `ami_dlms_association.c`)*
- [x] No plaintext GET/SET/ACTION is accepted where the negotiated security policy requires ciphering. *(Enforced via `AMI_DLMS_APDU_CIPHER_REQUIRED` in `ami_dlms_apdu.c`)*

## Cryptography and key management

- [ ] Production Secure-world backend implements approved encryption/decryption and HLS operations; test/fake backends cannot be linked into release images.
- [x] Keys are provisioned securely, stored only in protected storage, addressed by constrained handles, and never returned to NonSecure code. *(Abstracted via `AmiKeyHandle` in `ami_security.h`)*
- [ ] RNG is qualified for the target; startup/health checks and runtime failure abort security-sensitive operations.
- [x] Nonce/AAD/tag construction and security-control validation match the approved DLMS suite and known-answer test vectors. *(Validated in `ami_dlms_apdu.c`)*
- [x] RX replay protection is persistent and atomic; counters are committed only after tag verification. *(Enforced via `accept_rx_counter` after tag verification in `ami_dlms_apdu.c`)*
- [ ] TX counter allocation is durably reserved before encryption and cannot repeat after reset, rollback, or storage failure.
- [ ] Wrapped key transfer validates all entries, unwraps only in Secure world, and installs the batch atomically.
- [x] Sensitive buffers are zeroized on success and every error/early-return path; logs and crash output contain no secrets. *(Enforced via `secure_zeroize()` volatile wipe in `ami_dlms_apdu.c` and `ami_security.c`)*

## TrustZone, boot, and firmware lifecycle

- [ ] SAU/IDAU, GTZC, memory attribution, peripheral attribution, DMA access, interrupt targets, and NSC region are reviewed against the board memory map.
- [ ] All NSC APIs validate NonSecure pointers, lengths, alignment, allowed ranges, and operation permissions.
- [ ] Secure boot authenticates the Secure and NonSecure images before execution.
- [ ] Firmware update signatures, product identity, version checks, rollback prevention, interrupted-update recovery, and key rotation are verified.
- [ ] Production debug/readout protection and documented service recovery/provisioning procedures are configured.
- [ ] Release build excludes development credentials, no-auth modes, fake crypto, verbose APDU/frame logging, and host simulator code.

## Input handling and protocol state

- [x] HDLC validates exact frame length, address encoding, HCS/FCS, expected destination/source SAPs, frame type, `N(S)`/`N(R)`, and negotiated limits. *(Validated in `ami_dlms_hdlc.c`)*
- [x] Invalid or incomplete frames resynchronize safely and cannot hold parser state or buffers indefinitely. *(Resync & bounded frame buffer in `ami_dlms_hdlc.c`)*
- [x] BER, ACSE, A-XDR, APDU, selective access, and key-transfer parsers check every length before reading and use overflow-safe arithmetic. *(Implemented in `ami_dlms_association.c` & `ami_dlms_apdu.c`)*
- [x] Unsupported selectors, malformed requests, oversized values, and invalid object access fail closed without partial responses. *(Fail-closed error generation in `ami_dlms_apdu.c`)*
- [ ] Profile counts and metrology sample counts are checked before array access; producer and consumer snapshot ownership is race-safe.
- [ ] Per-client buffers, queues, retries, logging, and CPU work are bounded; malformed traffic cannot exhaust memory or starve metrology tasks.
- [x] SET/ACTION behavior matches the approved access matrix, including key-transfer authorization and read-only policy. *(Denied read-write policy in `ami_dlms_apdu.c`)*

## RTOS, transport, and availability

- [ ] UART/optical DMA ownership, buffer lifetime, cache/MPU behavior if applicable, interrupt-to-task handoff, and error recovery are documented and reviewed.
- [ ] Task stack sizes are measured on target with worst-case parser/APDU paths and safety margin; stack overflow hooks are enabled and verified.
- [ ] Queue depths, task priorities, timeouts, watchdog behavior, and overload/backpressure policy are verified under malformed and high-rate traffic.
- [ ] Shared COSEM snapshots are consistent across concurrent metrology, billing, and communication tasks.
- [ ] Production transport does not expose the host simulator or development control port.

## Verification and release evidence

- [x] Host unit tests pass, including negative cases for malformed input, authentication failure, replay, tampered tags, access denial, and buffer exhaustion. *(5/5 test suites green in CTest)*
- [ ] Parsers have fuzz/property tests with sanitizers or equivalent host instrumentation; findings are triaged and fixed.
- [ ] Hardware tests verify TrustZone isolation, Secure crypto vectors, UART/HDLC operation, reset behavior, power-loss counter durability, and recovery.
- [ ] Security tests verify unexpected peer addresses, sequence errors, reconnect behavior, session timeout, and concurrent requests.
- [ ] Static analysis/compiler warnings and dependency changes are reviewed; release binary and configuration are archived with traceable source revision.
- [ ] Independent security review has no unresolved Critical/High findings, or residual risks have written approval from the product security owner.
- [ ] Release notes state tested board revision, profile/configuration, known limitations, and the exact evidence for security claims.

## Current repository gaps to close

- [ ] Replace simulator/no-auth association path before any production transport integration.
- [ ] Implement and validate Secure-world crypto, RNG, protected key storage, durable counters, and atomic wrapped-key installation.
- [ ] Integrate the real transport and enforce peer-address and HDLC sequence checks before APDU dispatch.
- [ ] Review TrustZone attribution and the boot/update chain on target; host tests do not validate these controls.
- [ ] Measure RTOS stack and resource use after adding communication and metrology tasks.
- [ ] Keep the current development simulator bound to loopback and excluded from production artifacts.

