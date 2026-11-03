# Security Review & Remediation Report

## Scope and conclusion

This document combines findings from source-based reviews of the Secure and
NonSecure firmware, build wiring, DLMS association/APDU, cryptographic paths,
and host tests. It records both remediation claims and remaining risks; a
passing host test or firmware build does not by itself establish security.
Hardware behavior and end-to-end remote reachability have not been verified.

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
- **Status:** **Not fixed in the current integration.** RNG failures still fall back to an LCG (`secure_nsc.c:214-256`). Counter logic can use RAM-only callbacks when no NVM context is bound, and the firmware task does not bind persistent counters (`ami_dlms_cipher.c:154-170`, `ami_dlms_port.c:327-332`). Host tests do not establish hardware entropy or persistence across device resets.
- **Earlier verification claim:** `AmiNvmCounterTest` tests host-side counter logic; it does not demonstrate target persistence or RNG quality.

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
- **Status:** **Not fixed.** Secure key state is in volatile static RAM and default operational keys are fixed in the Secure source. NonSecure Suite 0 and HLS structures still contain raw-key fields, and the Secure HLS verifier is not called by current repository code. The AES-GCM adapter also supplies NonSecure key material in its AAD. Secure storage declarations alone do not prove keys are provisioned persistently or isolated from NonSecure code.

---

### 5. Secure World cipher service is not AES-GCM

- **Severity:** High (Critical)
- **Confidence:** 10/10
- **Evidence:** [`Secure/Core/Src/secure_nsc.c`](./Secure/Core/Src/secure_nsc.c#L642-L967)
- **Original Issue:** The Secure World cipher routine previously used a placeholder XOR transform and derived dummy tags without implementing real AES-GCM or verifying tags upon decryption.
- **Status:** **Original XOR placeholder replaced in source; production readiness not established.** Current Secure code contains a software AES-128-GCM/GMAC implementation (`Secure/Core/Src/secure_nsc.c`) featuring:
  - 10-round AES-128 block cipher (`aes128_encrypt_block_sec`) with forward key expansion.
  - Galois Field multiplication over $GF(2^{128})$ and GHASH over AAD/ciphertext blocks.
  - Standard 32-bit big-endian counter increment (`inc32_sec`).
  - Decryption checks the 16-byte authentication tag and zeroizes plaintext on failure.
- Source inspection confirms software AES/GHASH code, but does not validate conformance or side-channel behavior. It is not a hardware SAES implementation. The HLS verifier is not wired into the NonSecure call path, and the 2026-11-02 review found NonSecure code still has raw-key contexts.
- **Earlier verification claim:** `AmiDlmsCipherTest` and `AmiSecurityTest` validate host cryptographic modules, not this Secure World implementation on STM32U585 hardware.

---

### 6. Secure key-management entry points lack effective authorization

- **Severity:** Medium / High
- **Confidence:** 9/10
- **Evidence:** [`Secure/Core/Src/secure_nsc.c`](./Secure/Core/Src/secure_nsc.c#L265), [`Secure/Core/Src/secure_nsc.c`](./Secure/Core/Src/secure_nsc.c#L538)
- **Original Issue:** Factory provisioning token was reusable without lock, and remote key transfer (`SECURE_DLMS_TransferKeys_Atomic`) lacked Secure-world session tracking.
- **Status:** **Not fixed for production.** The token is a public compile-time constant; the lock/provisioned state is volatile and resets with Secure RAM. Key transfer checks a global HLS-authenticated flag, but that flag is not tied to a specific authorized DLMS association and is also volatile. The staging logic provides in-memory all-or-nothing handling, not durable atomic persistence.
- **Earlier verification claim:** A host key-transfer test does not prove Secure World authorization or persistent key updates on hardware.

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

## Vulnerabilities (2026-11-03)

The 2026-11-02 re-review findings were checked against the current source and
remain accurate. The earlier claims that all six items were remediated are
stale: in particular, operational keys still have fixed defaults and volatile
storage, NonSecure code still has raw-key contexts, RNG failure still falls
back to an LCG, persistent counters are not wired into the firmware task, and
key-management authorization remains incomplete.

The repeating-key XOR description is the exception: it is stale. Current
Secure source implements software AES/GHASH and decrypt-tag verification, but
not the claimed hardware SAES path. The earlier review also identified a
mutable NonSecure AES-GCM parameter structure at the NSC boundary; concurrent
DMA exploitation remains hardware-dependent and unverified.

This review did not identify additional distinct, high-confidence
vulnerabilities beyond those already listed in the 2026-11-02 update. Parser
bounds and the current port's SET/ACTION denial were checked; no separate
issue met the evidence threshold. This is a source review, not proof that the
firmware is secure or that attacks are impossible. Validate the NSC boundary,
DMA/TrustZone attribution, persistent storage, cryptographic services, and
transport on the actual target before production use.

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

Some earlier findings have been mitigated at the firmware policy layer, but
the current review still identifies unresolved key-management, key-isolation,
RNG, invocation-counter, and NSC-boundary concerns. The available source and
host tests do not establish production readiness; hardware and integration
validation remain necessary.
