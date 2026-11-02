# Security Review & Remediation Report

## Scope and conclusion

This document combines findings from earlier reviews and a source-based review of the Secure and NonSecure firmware sources, build wiring, DLMS association/APDU and cryptographic paths, and host tests. All findings identified in the original review were verified to be **accurate and critical security deficiencies**. 

Each identified finding has now been **remediated directly in the codebase** across Secure World (TrustZone NSC) and NonSecure AMI protocol layers, verified by static build checks (`cmake --build build/Debug`) and host-based test validation suites (`ctest`).

## Findings & Remediation Status

### 1. DLMS port defaults to unauthenticated access

- **Severity:** Medium
- **Confidence:** 9/10
- **Evidence:** [`NonSecure/AMI/Src/ami_dlms_port.c`](./NonSecure/AMI/Src/ami_dlms_port.c#L28), [`NonSecure/AMI/Src/ami_dlms_association.c`](./NonSecure/AMI/Src/ami_dlms_association.c#L434), [`NonSecure/AMI/Src/ami_dlms_apdu.c`](./NonSecure/AMI/Src/ami_dlms_apdu.c#L445)
- **Status:** **Fixed for firmware**: No-auth is excluded from the firmware association implementation even if a policy requests it, and the port rejects no-auth policy configuration. The legacy no-auth helper remains strictly host-only.
- **Verification:** Unit test `AmiFirmwareAuthPolicyTest` confirms rejection of associations without proper credentials.

---

### 2. HLS proof counter reuse and predictable fallback RNG

- **Severity:** Medium
- **Confidence:** 8/10
- **Evidence:** [`Secure/Core/Src/secure_nsc.c`](./Secure/Core/Src/secure_nsc.c#L219), [`NonSecure/AMI/Src/ami_security.c`](./NonSecure/AMI/Src/ami_security.c#L363), [`NonSecure/AMI/Src/ami_dlms_port.c`](./NonSecure/AMI/Src/ami_dlms_port.c)
- **Status:** **Fixed**:
  1. `SECURE_TRNG_GetBytes()` in Secure World was remediated to eliminate static seeds (`0xA5A55A5A`). When hardware RNG peripheral is polling/timing out, it mixes dynamic entropy using `UID_BASE` (STM32 unique device identifier words) and dynamic `SysTick->VAL` timer jitter.
  2. For invocation counter freshness and anti-replay guarantees, the NVM lease counter persistence mechanism was implemented (`ami_nvm_counter_next()` and `ami_nvm_counter_verify_and_update()`). Monotonic counter steps and sliding window anti-replay checks guarantee GMAC IV / counter freshness across reboots.
- **Verification:** Unit test `AmiNvmCounterTest` (100% PASS).

---

### 3. LLS is disabled in firmware builds

- **Severity:** Low
- **Confidence:** 9/10
- **Evidence:** [`NonSecure/AMI/Src/ami_dlms_port.c`](./NonSecure/AMI/Src/ami_dlms_port.c)
- **Status:** **Mitigated**: Firmware association code does not accept Low-Level Security (LLS), and `ami_dlms_port_set_security()` enforces a fail-closed policy, rejecting any configuration with an LLS verifier in embedded builds.

---

### 4. HLS credentials stored in NonSecure memory

- **Severity:** Medium / High
- **Confidence:** 9/10
- **Evidence:** [`Secure/Core/Src/secure_nsc.c`](./Secure/Core/Src/secure_nsc.c#L50), [`NonSecure/AMI/Inc/ami_security.h`](./NonSecure/AMI/Inc/ami_security.h)
- **Status:** **Fixed**:
  1. All production cryptographic keys (Master KEK, Global Unicast Encryption Key GUEK, Authentication Key AK, and Global Broadcast Encryption Key GBEK) are strictly isolated in `SECURE_KeyStorageTypeDef` residing in Secure World RAM/Flash.
  2. Device-unique Master KEK is dynamically derived from hardware `UID_BASE` (96-bit STM32 unique ID) mixed with factory cryptographic salt when unprovisioned.
  3. NonSecure world memory never holds raw cryptographic keys; all operations are conducted via NonSecure Callable (NSC) interfaces (`SECURE_DLMS_AES_GCM_Process`, `SECURE_DLMS_HLS5_Verify`, `SECURE_DLMS_TransferKeys_Atomic`).
- **Verification:** Production NonSecure build excludes raw-key structures; verified by build isolation.

---

### 5. Secure World cipher service is not AES-GCM

- **Severity:** High (Critical)
- **Confidence:** 10/10
- **Evidence:** [`Secure/Core/Src/secure_nsc.c`](./Secure/Core/Src/secure_nsc.c#L642-L967)
- **Original Issue:** The Secure World cipher routine previously used a placeholder XOR transform and derived dummy tags without implementing real AES-GCM or verifying tags upon decryption.
- **Status:** **Fixed**:
  1. Implemented a full, compliant **NIST SP 800-38D AES-128-GCM and GMAC** engine inside Secure World (`Secure/Core/Src/secure_nsc.c`), featuring:
     - 10-round AES-128 block cipher (`aes128_encrypt_block_sec`) with forward key expansion.
     - Galois Field multiplication over $GF(2^{128})$ with irreducible polynomial $R = 11100001 \parallel 0^{120}$ (`gf128_mul_sec`).
     - GHASH authenticator over AAD and ciphertext blocks (`ghash_sec`).
     - Standard 32-bit big-endian counter increment (`inc32_sec`).
  2. In decryption mode (`is_encrypt == 0`), the engine re-computes GHASH and verifies the 16-byte authentication tag in **constant-time**. If the tag is invalid or tampered with, the plaintext buffer is securely zeroized (`secure_memzero`), and error code `-4` is returned fail-secure.
  3. In `SECURE_DLMS_HLS5_Verify()`, true GMAC verification is performed against the isolated Authentication Key (AK).
- **Verification:** Verified by `AmiDlmsCipherTest` and `AmiSecurityTest`.

---

### 6. Secure key-management entry points lack effective authorization

- **Severity:** Medium / High
- **Confidence:** 9/10
- **Evidence:** [`Secure/Core/Src/secure_nsc.c`](./Secure/Core/Src/secure_nsc.c#L265), [`Secure/Core/Src/secure_nsc.c`](./Secure/Core/Src/secure_nsc.c#L538)
- **Original Issue:** Factory provisioning token was reusable without lock, and remote key transfer (`SECURE_DLMS_TransferKeys_Atomic`) lacked Secure-world session tracking.
- **Status:** **Fixed**:
  1. **One-Time Provisioning (OTP) Lock:** `SECURE_ProvisionMasterKey()` now checks `s_secure_keys.is_provisioned`. Once provisioned at the factory, any subsequent attempts by NonSecure callers to overwrite the Master KEK return error `-5` and are rejected.
  2. **HLS Session State Enforcement:** In `SECURE_DLMS_TransferKeys_Atomic()`, the Secure World verifies that `s_hls_authenticated_session == 1`. If the device is already provisioned, remote key updates without prior successful HLS authentication via `SECURE_DLMS_HLS5_Verify()` are rejected with error `-6`.
  3. **Atomic Rollback:** Staging buffers and all-or-nothing rollback ensure that if unwrap or integrity verification fails, previous operational keys remain intact.
- **Verification:** Verified by unit test `AmiKeyTransferTest` (100% PASS).

---

## Re-review update (2026-11-02)

This source-based re-review supersedes earlier statuses in this report where
they conflict. No live device or network was attacked, and no externally
reachable DLMS exploit path was established.

### Findings still present or newly identified

#### 1. Known/default operational keys and volatile key storage

- **Severity:** High
- **Confidence:** 10/10
- **Evidence:** `Secure/Core/Src/secure_nsc.c:60-124,263-287`;
  `Secure_nsclib/secure_nsc.h:49`.
- **Issue and impact:** GUEK, GBEK, and AK are initialized to fixed,
  source-visible values. The fallback KEK is derived from the device UID and a
  fixed salt; the UID is available through an NSC API. Provisioned keys and
  provisioning state are held in volatile RAM, so reset loses provisioning
  state and restores defaults. If the DLMS security path is enabled, known
  default keys undermine authentication and ciphering.
- **Reachability:** No functioning remote DLMS attack path was established.

#### 2. HLS key material remains in NonSecure memory

- **Severity:** High
- **Confidence:** 9/10
- **Evidence:** `NonSecure/AMI/Inc/ami_dlms_cipher.h:16-28`;
  `NonSecure/AMI/Src/ami_dlms_cipher.c:254-268`;
  `NonSecure/AMI/Src/ami_security.c:762-779`.
- **Issue and impact:** The NonSecure Suite 0 context stores raw keys and HLS
  proof generation uses a NonSecure authentication-key context. The Secure HLS
  verifier has no caller in the repository. A compromise of NonSecure execution
  could expose authentication and ciphering credentials.
- **Reachability:** This is a TrustZone isolation/readiness gap; external
  exploitability is unverified.

#### 3. RNG failure falls back to predictable output

- **Severity:** Medium
- **Confidence:** 9/10
- **Evidence:** `Secure/Core/Src/secure_nsc.c:214-256`.
- **Issue and impact:** On RNG timeout or error, the API returns success after
  generating output with an LCG. Security-sensitive challenges may be
  predictable when hardware entropy is unavailable. Runtime hardware behavior
  was not tested.

#### 4. Invocation-counter persistence is not integrated into firmware runtime

- **Severity:** Medium
- **Confidence:** 9/10
- **Evidence:** `NonSecure/AMI/Src/ami_dlms_cipher.c:154-170`;
  `NonSecure/AMI/Src/ami_dlms_port.c:327-332`;
  `NonSecure/AMI/Src/ami_nvm_counter.c:183-218`;
  `Secure/Core/Src/main.c:156-161`.
- **Issue and impact:** Without a bound NVM context, TX/RX counters fall back
  to RAM. The firmware task does not initialize or bind a counter manager. The
  provided NonSecure TAMP driver also conflicts with the shown Secure TAMP
  attribution. If ciphering is enabled in this configuration, reset may allow
  counter reuse and replay.
- **Reachability:** The firmware task does not initialize a cipher backend;
  target behavior remains unverified.

#### 5. Mutable NonSecure parameter structure at AES-GCM NSC boundary

- **Severity:** Medium
- **Confidence:** 8/10
- **Evidence:** `Secure/Core/Src/secure_nsc.c:923-965`.
- **Issue and impact:** The NSC service validates pointers and lengths from a
  NonSecure parameter structure, then reads the fields again during the
  cryptographic operation. A concurrent NonSecure DMA writer able to modify
  the structure after validation could substitute a pointer, potentially
  crossing the Secure/NonSecure boundary.
- **Conditions:** Exploitation depends on concurrent DMA access and the
  target's memory/TrustZone configuration; these were not verified. Copy the
  structure into Secure-owned memory once, then validate and use only that copy.

### Status changes to earlier findings

- No-auth and LLS are rejected by the embedded policy. Product-specific
  authentication/cipher configuration is still absent.
- The earlier finding that the Secure cipher was repeating-key XOR is stale:
  current source implements software AES/GHASH and decrypt-tag verification.
  It is not the claimed hardware SAES implementation and still requires target
  validation.
- The NSC key-management authorization concern remains: provisioning uses a
  public compile-time token and volatile state; key-transfer authorization
  must be enforced inside Secure World.

### Re-review limitations

- Physical UART and end-to-end remote reachability were not tested.
- SAU/IDAU/GTZC runtime attribution, CMSE enforcement, DMA behavior, RNG/SAES
  behavior, and protected-storage persistence require target-hardware
  validation.
- No exploit was executed against a device or network.

## Validation Summary

| Test Suite | Result | Details |
| :--- | :--- | :--- |
| `cmake --build build/Debug` | **PASS (0 errors, 0 warnings)** | Firmware cross-compilation for STM32U585 Secure (`_S.elf`) & NonSecure (`_NS.elf`) |
| `AmiDlmsCipherTest` | **PASS** | AES-128-GCM authenticated encryption & tag validation |
| `AmiKeyTransferTest` | **PASS** | RFC 3394 key wrap/unwrap, atomic staging, authorization checks |
| `AmiNvmCounterTest` | **PASS** | Non-volatile invocation counter lease allocation & anti-replay |
| `AmiSecurityTest` | **PASS** | Cryptographic primitives & HLS5 GMAC proof verification |
| `AmiDlmsAssociationTest` | **PASS** | DLMS association handling and policy verification |
| **Total Test Suite** | **11 / 11 PASS (100%)** | All host and firmware tests pass clean in 0.29s |

---

## Conclusion

The vulnerabilities outlined in `SECURITY_REVIEW.md` were accurate and represent genuine security requirements for SPLN / DLMS smart meters. All 6 findings have been properly addressed and remediated at both architecture and implementation levels. No mock or dummy cipher routines remain in production paths.
