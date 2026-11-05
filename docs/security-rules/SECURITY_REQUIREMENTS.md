# Security Requirements

## Purpose and scope

This document defines the security baseline for the STM32U585VIT6 three-phase postpaid smart-meter firmware and its DLMS/COSEM interface. It covers the Secure and NonSecure images, TrustZone boundary, metrology and billing data, HDLC transport, association/authentication, cryptographic services, key management, firmware lifecycle, and the host simulator.

These are requirements, not a statement that the current implementation satisfies them. The current repository contains protocol components and host tests, but the Secure cryptographic backend, production key provisioning, and device communications task are not integrated. Product-specific DLMS/SPLN profiles and controlled standards editions must be selected before claiming conformance.

## Security objectives

The application shall:

1. Protect meter readings, billing history, event records, credentials, and cryptographic keys from unauthorized disclosure or modification.
2. Reject unauthenticated, malformed, replayed, misaddressed, and unauthorized requests without corrupting a valid session or meter state.
3. Preserve metrology and billing integrity across resets, faults, communications errors, and attempted key changes.
4. Keep secrets and security-critical operations inside the Secure world, exposing only narrow, validated NonSecure-callable services.
5. Fail closed when authentication, cryptography, counter persistence, storage integrity, or security configuration cannot be verified.
6. Make security behavior auditable through controlled configuration, traceable builds, and security-focused verification.

## Mandatory requirements

### Authentication and authorization

- Production association shall use an explicitly selected and approved DLMS authentication profile. No-auth association shall be disabled in production configuration and shall never be selected implicitly by a convenience API.
- Authentication policy shall be configured allow-list style. Unsupported mechanisms, missing credentials, malformed ACSE data, and backend errors shall be rejected.
- LLS shall only be enabled where the communication channel is physically protected and the product profile explicitly permits its cleartext password exchange. Prefer the approved HLS mechanism for remote/untrusted links.
- The implementation shall bind every association to the validated client/server addresses, negotiated context, authentication mechanism, and security parameters. A new association request shall not silently reset an active session.
- COSEM access rights shall be enforced for each class, logical name, attribute, and method. Read-only policy shall deny SET and ACTION operations except explicitly authorized Security Setup key transfer.
- Session release, timeout, link reset, authentication failure, and device reset shall clear ephemeral challenges and session state. Failed authentication attempts shall be rate-limited or otherwise bounded.

### Cryptography and key lifecycle

- Approved cryptographic algorithms, key sizes, nonce construction, AAD, tag size, system-title rules, and invocation-counter behavior shall be defined by the selected DLMS security suite and product profile. Test doubles shall never be linked into a production image.
- Secret keys, KEKs, passwords, and private credentials shall be generated or provisioned securely and stored only in protected Secure-world storage or an approved hardware-backed facility. They shall not be compiled into source, logs, NonSecure memory, crash dumps, or readable COSEM attributes.
- Secure-world cryptographic services shall validate every pointer, length, algorithm, key handle, and operation mode crossing the NSC boundary; reject invalid or overlapping ranges; and avoid returning key bytes to NonSecure callers.
- AES-GCM encryption shall never reuse a `(key, nonce)` pair. Transmit invocation counters shall be reserved durably before encryption. Receive counters shall be checked against a persistent anti-replay policy and committed atomically only after successful authentication.
- Authentication tags shall be verified before plaintext is consumed. On failure, plaintext and intermediate buffers shall be cleared with a non-optimizable zeroization routine.
- Wrapped-key transfer shall validate the complete request, unwrap and validate all keys in Secure world, and install the batch atomically. Partial installation shall be impossible. Key updates shall be authenticated, authorized, auditable, and recoverable after power loss.
- Random challenges and key material shall come from a qualified cryptographically secure random source. RNG failure shall abort the operation.

### Parsing, transport, and resource limits

- All external input shall be length-checked before access. Parsers shall use overflow-safe arithmetic, reject non-canonical or unsupported encodings, and enforce explicit maximum frame, APDU, BER, A-XDR, profile, and nesting sizes.
- HDLC shall validate frame format, HCS/FCS, destination/source SAPs, control type, sequence numbers, negotiated information limits, and session state before passing APDUs to the application.
- Truncated, oversized, malformed, or unsupported inputs shall be rejected without out-of-bounds access, stale-buffer disclosure, partial response emission, or persistent parser desynchronization.
- Buffer ownership and concurrency shall be explicit for DMA, interrupts, tasks, and protocol handlers. Shared metrology/profile snapshots shall be read atomically or copied through a bounded consistent-snapshot mechanism.
- Communication handling shall have bounded execution time, bounded queue growth, per-client/session resource limits, and safe timeout behavior. Untrusted traffic shall not cause unbounded logging, allocation, retries, or task starvation.
- The firmware shall not expose diagnostic or simulator services on production interfaces unless they are separately authenticated, authorized, and explicitly enabled in a release configuration.

### TrustZone, boot, and configuration

- Secure/NonSecure flash, SRAM, peripherals, DMA channels, interrupts, and NSC entry points shall follow a reviewed partition map. SAU/IDAU and GTZC settings shall be verified on the target and covered by configuration review.
- Secure boot shall authenticate the Secure and NonSecure images before execution. Firmware updates shall be signed, version-checked, rollback-protected, and recoverable from interrupted updates.
- Debug access, lifecycle state, option bytes, readout protection, and production provisioning shall be configured and documented for the deployed meter.
- NSC entry points shall be minimal, memory-safe, privilege-aware, and free of general-purpose pointer callbacks unless the callback is validated as a NonSecure address.
- Security-relevant configuration changes shall be authenticated, authorized, validated, and recorded in tamper-aware event history where supported.

### Data and operations

- Meter values, billing snapshots, event logs, system titles, and security metadata shall be classified and exposed only according to the approved COSEM access profile.
- Logs shall not contain passwords, key material, plaintext ciphered APDUs, full authentication challenges/proofs, or sensitive billing data. Diagnostic verbosity shall be disabled or constrained in production.
- Security failures shall return protocol-appropriate errors where safe, avoid leaking secrets or key-existence details, and preserve the availability and integrity of metrology functions.
- Builds shall use reviewed compiler/linker settings, reproducible dependency versions where practical, warning/static-analysis review, and an auditable release artifact manifest.

## Acceptance criteria

A release is security-eligible only when all deployment checklist items in `SECURITY_CHECKLIST.md` are evidenced as complete, the threat model risks have owners and mitigations, and the product security profile has been approved. Passing host unit tests alone does not establish device, TrustZone, cryptographic, or standards conformance.

