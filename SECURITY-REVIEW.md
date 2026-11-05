# Security Review Summary — 4 November 2026

## Executive summary

A read-only source review identified **two high-severity issues and one
medium-severity issue**. The first is a Secure key-management authorization
bypass available to a NonSecure caller. The second is that the NonSecure
firmware image is not cryptographically authenticated before execution. The
third can allow invocation-counter rollback after storage failures, risking
AES-GCM nonce reuse and replay acceptance.

These are source-level findings. The review did not establish a remote network
attack path or verify behavior on physical hardware. No source code was
changed as part of this review.

## Findings

| ID | Severity | Confidence | Location | Finding |
|---|---|---:|---|---|
| F1 | **HIGH** | 9/10 | [`secure_nsc.c`](./Secure/Core/Src/secure_nsc.c), lines 112, 226–252, 491–513, 850–927 | **Secure key-management authorization can be bypassed.** A NonSecure caller can use the public provisioning token and manipulate the HLS proof/verification flow to set the global authorization state, then transfer keys under a caller-selected master key. This crosses the Secure/NonSecure authorization boundary. A remote entry point was not established. |
| F2 | **HIGH** | 9/10 | [`main.c`](./Secure/Core/Src/main.c), lines 132–171, 180–187 | **NonSecure image is not cryptographically authenticated before execution.** Missing or invalid manifest magic is not consistently rejected, and the declared image hash is not verified; no signature check is present in the reviewed path. If an attacker can replace the NonSecure image and no independent boot protection exists, an image with plausible vector values may execute. |
| F3 | **MEDIUM** | 8/10 | [`ami_nvm_counter.c`](./NonSecure/AMI/Src/ami_nvm_counter.c), lines 69–80, 106–125 | **Counter recovery can roll back after storage failure.** Invalid records reset counters to low values. A failed TX lease write can leave RAM state advanced, allowing later calls to issue counters from an uncommitted lease. If ciphering is active, this can reuse AES-GCM nonces; RX rollback can permit replay. |

## Additional risks requiring target verification

- **Backup-register conflict and TAMP attribution:** Secure boot uses `BKP0R`
  for the anti-rollback epoch while the NonSecure counter driver uses it for
  counter-record metadata. The counter driver also accesses TAMP from
  NonSecure while Secure startup configures TAMP as Secure. Verify register
  ownership, access behavior, and retention on the target.
- **NSC descriptor double-fetch:** Some NSC paths validate fields through a
  NonSecure descriptor and read them again during use. Whether DMA can exploit
  this depends on actual DMA permissions and memory attribution.
- **Runtime reachability:** The reviewed source does not establish a complete
  production transport/provisioning path to these issues. Remote
  exploitability is therefore unconfirmed.

## Recommended priorities

1. Keep provisioning authorization and HLS session state entirely Secure;
   remove public fixed-token authorization and bind provisioning to a trusted,
   fresh authorization flow.
2. Require a valid cryptographic signature over the NonSecure image and
   manifest; fail closed if verification fails, and update anti-rollback
   state only after successful authentication.
3. Treat counter-record corruption and lease-write failures as fatal to
   protected operations. Never issue counters from state that was not
   durably committed.
4. Resolve TAMP/BKP register ownership and validate NSC descriptor handling
   under the actual hardware attribution configuration.

## Review limitations

This was a static source review dated 4 November 2026. It did not include
physical-device testing, network testing, exploit execution, or verification
of boot ROM and option-byte configuration. Confidence scores describe
confidence in the source analysis, not certainty of exploitation.
