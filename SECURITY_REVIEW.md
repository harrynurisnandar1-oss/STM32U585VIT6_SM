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

### 5. Secure World AES-GCM is software-only and not hardware-validated

- **Severity:** Medium (production-readiness gap)
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

This section records the comprehensive source review organized around the
Confidentiality, Integrity, and Availability (CIA) triad. It updates earlier
findings where current source provides more specific evidence. No live device
or network was attacked, and no externally reachable exploit path was
established.

### Confidentiality

#### C1. SRAM3 attribution changed in source; hardware verification pending

- **Severity:** High
- **Confidence:** 9/10
- **Evidence:** [`Secure/Core/Src/main.c`](./Secure/Core/Src/main.c#L167),
  [`Secure/STM32U585xx_FLASH_s.ld`](./Secure/STM32U585xx_FLASH_s.ld#L43), and
  `Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_hal_gtzc.h:54-57`.
- **Earlier issue:** SRAM3 attributes appeared NonSecure while Secure data and stack were linked there.
- **Current status:** **Fixed in source; not verified on hardware.** `MPCBB_SecConfig_array` and `MPCBB_PrivConfig_array` are now set to `0xFFFFFFFF`, MPCBB configuration is locked, and GTZC interrupts are enabled in `Secure/Core/Src/main.c`. Confirm the effective attribution, lock result, and fault behavior on the target board before treating the hardware risk as closed.

#### C2. Fixed/default keys and volatile key state

- **Severity:** High
- **Confidence:** 9/10
- **Evidence:** [`Secure/Core/Src/secure_nsc.c`](./Secure/Core/Src/secure_nsc.c#L43),
  [`Secure_nsclib/secure_nsc.h`](./Secure_nsclib/secure_nsc.h#L49).
- **Issue and impact:** GUEK, GBEK, and AK are fixed source-visible defaults.
  The fallback KEK is derived from the device UID and a fixed salt, and the UID
  is exposed through an NSC API. Provisioned keys and provisioning state are
  volatile RAM, so reset loses the provisioned state and restores defaults.
  If these keys protect live traffic, confidentiality and authentication are
  compromised.
- **Status:** Already documented; earlier claims that keys are durably stored
  or fully isolated are inaccurate.
- **Reachability:** Current firmware does not configure a functioning DLMS
  cipher path; remote exploitation was not established.
- **Latest verification:** Still present in current source. Provisioning does
  not make the key/state persistent, and fixed operational defaults remain.

#### C3. Raw HLS/cipher keys remain in NonSecure structures

- **Severity:** High
- **Confidence:** 9/10
- **Evidence:** [`NonSecure/AMI/Inc/ami_dlms_cipher.h`](./NonSecure/AMI/Inc/ami_dlms_cipher.h#L16),
  [`NonSecure/AMI/Src/ami_dlms_cipher.c`](./NonSecure/AMI/Src/ami_dlms_cipher.c#L254),
  and `NonSecure/AMI/Src/ami_security.c:762-779`.
- **Issue and impact:** The Suite 0 context stores raw keys and copies them
  during initialization; HLS proof generation uses a NonSecure key context.
  Compromised NonSecure execution could disclose those credentials. The Secure
  HLS verifier is not called by current repository code.
- **Status:** Already documented; production key isolation is not complete.
- **Reachability:** No production caller initializing this context was found.
- **Latest verification:** Partially changed only: the current port initializes
  a dummy all-zero key context, but raw-key fields/copies remain in NonSecure
  code. The Secure HLS verifier is not wired to the port. The HLS proof callback
  currently reports success and length without writing a proof; do not treat
  this as a functioning authenticated integration.

### Integrity

#### I1. Key-transfer authorization can be bypassed by a NonSecure caller

- **Severity:** High
- **Confidence:** 9/10
- **Evidence:** [`Secure_nsclib/secure_nsc.h`](./Secure_nsclib/secure_nsc.h#L49),
  [`Secure/Core/Src/secure_nsc.c`](./Secure/Core/Src/secure_nsc.c#L263),
  and [`Secure/Core/Src/secure_nsc.c`](./Secure/Core/Src/secure_nsc.c#L528).
- **Issue and impact:** A public provisioning token and weakly bound global HLS state can permit a NonSecure caller to authorize key operations outside a verified, fresh DLMS association.
- **Status:** **Partially fixed; still unsafe at the NSC boundary.** Transfer now checks `is_provisioned` and the HLS flag and clears the flag after successful transfer. However, the provisioning token remains public; provisioning state is volatile; and HLS verification accepts caller-supplied challenges and sets a global flag that is not bound to a specific association or expiry. Because the default AK is source-visible, a NonSecure caller can potentially construct a proof for caller-chosen challenges, set this flag, and then invoke transfer. The code review also found the pre-provisioned/default-key lifecycle remains insecure. These controls do not establish factory authorization or session-bound remote authorization.
- **Integration note:** The firmware HLS proof callback does not compute/write a proof, while the port task's cipher backend setup/binding errors are ignored. Current source therefore does not demonstrate a working secure key-transfer path.

#### I2. Counter storage failures and overflow are not fail-closed

- **Severity:** Medium
- **Confidence:** 9/10
- **Evidence:** [`NonSecure/AMI/Src/ami_nvm_counter.c`](./NonSecure/AMI/Src/ami_nvm_counter.c#L70),
  [`NonSecure/AMI/Src/ami_dlms_cipher.c`](./NonSecure/AMI/Src/ami_dlms_cipher.c#L154),
  and [`NonSecure/AMI/Src/ami_dlms_port.c`](./NonSecure/AMI/Src/ami_dlms_port.c#L321).
- **Issue and impact:** Corrupt/missing records, ignored initialization/binding failures, storage-domain mismatch, or counter overflow can cause replay acceptance or GCM nonce reuse.
- **Status:** **Partially fixed; fail-closed runtime behavior is not established.** TX overflow and RX high-water write failure now return failure. However, invalid/unreadable records still initialize low counters. On TX lease-write failure, `current_tx_counter` and `persisted_tx_limit` have already advanced in RAM before the write is checked; a subsequent call can potentially allocate a counter in a lease that was never persisted, then reuse it after reset. Valid-record initialization also adds the lease step without an overflow guard. The firmware task ignores counter initialization/bind results; the TAMP driver directly accesses the peripheral from NonSecure code while TAMP is attributed Secure; and cipher callbacks retain RAM-only fallback behavior. The task also does not successfully attach the backend using the port's setter. Host counter tests do not establish reset-safe behavior on hardware.

#### I3. RNG errors return predictable output as success

- **Severity:** Medium
- **Confidence:** 9/10
- **Evidence:** [`Secure/Core/Src/secure_nsc.c`](./Secure/Core/Src/secure_nsc.c#L195).
- **Issue and impact:** On RNG timeout/error, API can return success with predictable LCG output.
- **Status:** **Still present.** The current target path falls back to an LCG when RNG data is unavailable or error flags are set; the non-RNG build path also returns LCG output as success. UID and SysTick mixing do not make this a CSPRNG. Hardware RNG behavior remains untested.

#### I4. Mutable NonSecure AES-GCM parameters at NSC boundary

- **Severity:** Medium
- **Confidence:** 8/10
- **Evidence:** [`Secure/Core/Src/secure_nsc.c`](./Secure/Core/Src/secure_nsc.c#L931).
- **Issue and impact:** NSC validates pointers/lengths from mutable NonSecure structures, then reads fields again during the operation, allowing a possible DMA TOCTOU.
- **Status:** **Still present in current source.** The AES-GCM entry continues to dereference `params->...` after validation; key-transfer similarly validates then rereads the descriptor array. No Secure-owned snapshot is used for all subsequent checks/operations. Exploitation depends on target DMA and TrustZone attribution and has not been demonstrated.

#### I5. Secure/NonSecure image authenticity and rollback protection absent

- **Severity:** High production-readiness gap
- **Confidence:** 8/10
- **Evidence:** [`Secure/Core/Src/main.c`](./Secure/Core/Src/main.c#L124).
- **Issue and impact:** The Secure startup obtains the NonSecure reset vector
  and jumps to it; source review found no signature verification or
  anti-rollback check before execution. If no independent ROM/boot-stage
  protection provides these controls, unauthorized or downgraded firmware
  could execute and compromise integrity/confidentiality.
- **Status:** Not previously recorded in the CIA section. ROM boot configuration
  and option bytes are not visible in this source review.
- **Validation required:** Review the full boot chain, provisioning state,
  option bytes, and signed-image verification on target hardware before
  treating this as a confirmed deployed-device exposure.

#### I6. HLS proof callback reports success without generating a proof

- **Severity:** High if enabled; current status is integration defect
- **Confidence:** 9/10
- **Evidence:** `NonSecure/AMI/Src/ami_dlms_port.c:19-28`;
  `NonSecure/AMI/Src/ami_dlms_association.c:423-441`.
- **Issue and impact:** The callback validates inputs, sets `proof_length` to 16,
  returns success, but does not write proof bytes. The association path
  initializes proof/response buffers to zero. If this callback is used to
  complete HLS, zero proof material may be treated as a successful response
  instead of a real GMAC proof.
- **Reachability:** No production call to `ami_dlms_process_hls_reply` was
  found, and current transport is not operationally wired. This is not a
  demonstrated remote authentication bypass; it must fail closed before HLS
  integration is enabled.

### Availability

No separate, high-confidence exploitable availability vulnerability was
confirmed by this source-only review. Availability protections remain
incomplete/unverified: `Error_Handler()` loops indefinitely, no watchdog
recovery was found, and transport/resource-exhaustion behavior was not tested.
These are operational resilience gaps, not proof of a remote denial-of-service
path. See the error handler in `Secure/Core/Src/main.c` and
`NonSecure/Core/Src/main.c`.

### Other audit conclusions and limitations

- Firmware rejects no-auth and LLS. The current task configures an HLS callback,
  but that callback reports success without generating a proof; the task also
  does not successfully attach the cipher backend and the weak UART transmitter
  is not physically wired. This is not a functioning secured DLMS integration,
  and the stub must fail closed before HLS is enabled.
- The previous repeating-key XOR description is stale. Current Secure source
  contains software AES/GHASH and decrypt-tag verification, but not the claimed
  hardware SAES path. Host tests do not establish target conformance or
  side-channel behavior.
- TAMP is attributed Secure while the supplied counter driver accesses it from
  NonSecure code; runtime access and backup-domain persistence require board
  validation. The record's CRC detects accidental corruption but is not keyed
  authenticity.
- The latest ChangeLOG remediation statements are only partly supported by
  source: SRAM3 attributes changed, TX overflow/RX write checks were added, and
  key-transfer guards were added. Hardware attribution remains unverified;
  RNG fallback, raw-key contexts, volatile keys, NSC re-reads, counter
  initialization/binding failures, the HLS proof stub, and backend attachment
  issues remain. README/CHANGELOG claims of durable key storage or complete
  remediation overstate current integration.
- No live target was accessed and no exploit was executed. SRAM attribution,
  DMA races, boot ROM/option-byte behavior, physical transport reachability,
  RNG quality, and persistent storage must be verified on the actual board.

## Previously Recorded Validation Summary

The results below are historical entries from prior work, not tests rerun as
part of the 2026-11-03 CIA source review. Host tests and cross-compilation do
not validate runtime TrustZone attribution, hardware RNG/SAES, secure
persistence, boot authenticity, or the absence of vulnerabilities.

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
