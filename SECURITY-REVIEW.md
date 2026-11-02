# Security Review

## Scope and limitations

This is a read-only, source-based review of the STM32U585VIT6_SM firmware. No
live device, network, or physical interface was attacked. The source findings
below are not proof that a remote attacker can reach these code paths in the
current build. UART integration, runtime TrustZone configuration, and hardware
behavior require separate validation on the target board.

## Findings

### 1. Secure World cipher service is not AES-GCM

- **Severity:** High
- **Confidence:** 10/10
- **Evidence:** `Secure/Core/Src/secure_nsc.c` (lines 641-681),
  `NonSecure/AMI/Src/ami_dlms_cipher.c` (lines 32-55), and
  `NonSecure/AMI/Src/ami_dlms_apdu.c` (lines 612-621).
- **Issue:** The Secure World cipher routine transforms data with repeating-key
  XOR and derives an output tag from key bytes and input length. It does not
  implement AES-GCM authentication/encryption or verify the supplied tag. The
  NonSecure adapter treats a successful return as authenticated plaintext.
- **Impact and conditions:** If this backend is enabled and reachable on a
  communication interface, ciphered APDUs do not receive the confidentiality
  and integrity guarantees expected from AES-GCM. This review did not establish
  that an external peer can currently reach the path.
- **Recommended remediation:** Fail closed until a reviewed AES-GCM
  implementation is integrated in Secure World. Verify encryption/decryption
  with standard known-answer vectors and confirm that a modified tag is
  rejected before plaintext is dispatched. Remove fixed demonstration keys and
  test fallbacks from production builds.

### 2. Secure key-management entry points lack effective authorization

- **Severity:** Medium
- **Confidence:** 9/10
- **Evidence:** `Secure_nsclib/secure_nsc.h` (line 49),
  `Secure/Core/Src/secure_nsc.c` (lines 239-260 and 500-511), and
  `NonSecure/AMI/Src/ami_dlms_apdu.c` (lines 297-304).
- **Issue:** The factory provisioning token is a public compile-time constant,
  and the provisioning NSC function accepts it as its authorization check.
  The wrapped-key transfer NSC function validates inputs and unwrap integrity,
  but does not independently verify authenticated-session or provisioning
  authorization. DLMS-level HLS checks occur in NonSecure code and are not an
  authorization boundary for a directly callable Secure API.
- **Impact and conditions:** Code able to execute in NonSecure state could
  potentially call these NSC functions directly, bypassing the protocol-layer
  authorization and changing Secure-held keys. This is not evidence of a
  remote APDU-only route or an observed compromise.
- **Recommended remediation:** Replace the public token with a Secure-enforced
  provisioning lifecycle and authorization policy. Require the Secure service
  to validate an authorized session or a separately authenticated,
  one-time provisioning mechanism before key changes. Persist key updates
  atomically in protected storage.

## Safe validation

Use isolated host tests or a dedicated development board. Test the Secure
backend against AES-GCM known-answer vectors, verify altered tags are rejected
without dispatching plaintext, and verify direct NSC key-management calls fail
without Secure-enforced authorization. Do not use production meters or
third-party networks for these checks.

## Not validated

- Physical UART or other transport exposure and end-to-end remote reachability.
- Runtime SAU/IDAU/GTZC attribution, CMSE enforcement, and peripheral isolation.
- SAES, RNG, key-storage persistence, or power-loss behavior on STM32U585
  hardware.
- Whether a production image enables the reviewed cipher backend or exposes
  the NSC calls under its final build configuration.
