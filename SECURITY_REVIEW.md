# Security Review

## Scope and conclusion

This document combines findings from earlier reviews and a source-based review of the Secure and NonSecure firmware sources, build wiring, DLMS association/APDU and cryptographic paths, and host tests. Findings are conditional security-readiness issues unless stated otherwise. The review did not establish an externally reachable exploit path in the current firmware: the DLMS task is started, but its receive hooks are not wired to a physical UART in the reviewed sources. No live device, network, or physical interface was attacked. Hardware behavior and runtime TrustZone configuration require validation on the target board.

## Findings

### 1. DLMS port defaults to unauthenticated access

- **Severity:** Medium
- **Confidence:** 9/10
- **Evidence:** [`NonSecure/AMI/Src/ami_dlms_port.c`](./NonSecure/AMI/Src/ami_dlms_port.c#L28), [`NonSecure/AMI/Src/ami_dlms_association.c`](./NonSecure/AMI/Src/ami_dlms_association.c#L434), [`NonSecure/AMI/Src/ami_dlms_apdu.c`](./NonSecure/AMI/Src/ami_dlms_apdu.c#L445)
- **Status:** Fixed for firmware: no-auth is excluded from the firmware association implementation even if a policy requests it, and the port rejects no-auth policy configuration. The legacy no-auth helper remains host-only.
- **Remaining issue and impact:** Product-specific authentication policy is still not configured. Firmware rejects all associations until an approved HLS policy is installed. Per-SAP client policy and validated peer-SAP binding remain to be implemented.
- **Reachability:** No firmware call to the UART receive hooks or configured UART was found. The fail-closed policy is enforced in firmware source/build, but target transport reachability remains unintegrated.

### 2. HLS proof counter reuse and predictable fallback RNG

- **Severity:** Medium
- **Confidence:** 8/10
- **Evidence:** [`NonSecure/AMI/Inc/ami_security.h`](./NonSecure/AMI/Inc/ami_security.h#L139), [`NonSecure/AMI/Src/ami_security.c`](./NonSecure/AMI/Src/ami_security.c#L363), [`NonSecure/AMI/Src/ami_security.c`](./NonSecure/AMI/Src/ami_security.c#L478)
- **Issue and impact:** The HLS context uses a fixed invocation counter, and the proof callback reads it without advancing it. The association exchange invokes proof generation for both sides, so using this callback unchanged can reuse the GMAC IV with the same key. The fallback software RNG also starts from a fixed seed, causing sequences to repeat after reset. If these callbacks are enabled with unchanged key/counter state, proof freshness is weakened and replay may be possible when the challenge repeats.
- **Reachability:** The review found no production initialization of these HLS callbacks or random provider; their use appears in tests. This is a conditional security-readiness issue, not a demonstrated current authentication bypass.

### 3. LLS is disabled in firmware builds

- **Status:** Mitigated: firmware association code does not accept LLS, and `ami_dlms_port_set_security()` rejects any policy that supplies an LLS verifier. The non-firmware host simulator and tests retain their existing LLS behavior.
- **Remaining issue:** If a future product requires LLS, it must be restricted to a formally approved protected transport and selected client/SAP policy; that exception is not implemented.

### 4. HLS credentials are stored in NonSecure memory

- **Severity:** Medium
- **Confidence:** 9/10
- **Evidence:** [`NonSecure/AMI/Inc/ami_security.h`](./NonSecure/AMI/Inc/ami_security.h#L139), [`NonSecure/AMI/Src/ami_security.c`](./NonSecure/AMI/Src/ami_security.c#L491), [`NonSecure/CMakeLists.txt`](./NonSecure/CMakeLists.txt#L59), [`Secure/Core/Src/secure_nsc.c`](./Secure/Core/Src/secure_nsc.c#L51)
- **Status:** The production NonSecure build now excludes raw-key HLS and software-crypto APIs at compile time. Those APIs remain available to non-firmware host builds, with no change to host-test source or CMake configuration.
- **Remaining issue and impact:** The Secure NSC implementation still does not provide production key provisioning/storage or a key-handle-based HLS service. Do not configure production HLS until that Secure service exists and is validated; otherwise there is no protected production key path.
- **Reachability:** No firmware call site or provisioned HLS key was found. This was a conditional readiness gap, not an active key disclosure demonstrated in the current build.

### 5. Secure World cipher service is not AES-GCM

- **Severity:** High
- **Confidence:** 10/10
- **Evidence:** [`Secure/Core/Src/secure_nsc.c`](./Secure/Core/Src/secure_nsc.c#L641), [`NonSecure/AMI/Src/ami_dlms_cipher.c`](./NonSecure/AMI/Src/ami_dlms_cipher.c#L32), and [`NonSecure/AMI/Src/ami_dlms_apdu.c`](./NonSecure/AMI/Src/ami_dlms_apdu.c#L612).
- **Issue:** The Secure World cipher routine transforms data with repeating-key XOR and derives an output tag from key bytes and input length. It does not implement AES-GCM or verify the supplied tag. The NonSecure adapter treats a successful return as authenticated plaintext.
- **Impact and conditions:** If this backend is enabled and reachable on a communication interface, ciphered APDUs do not receive the confidentiality and integrity guarantees expected from AES-GCM. This review did not establish that an external peer can currently reach the path.
- **Recommended remediation:** Fail closed until a reviewed AES-GCM implementation is integrated in Secure World. Verify encryption/decryption with standard known-answer vectors and confirm that a modified tag is rejected before plaintext is dispatched. Remove fixed demonstration keys and test fallbacks from production builds.

### 6. Secure key-management entry points lack effective authorization

- **Severity:** Medium
- **Confidence:** 9/10
- **Evidence:** [`Secure_nsclib/secure_nsc.h`](./Secure_nsclib/secure_nsc.h#L49), [`Secure/Core/Src/secure_nsc.c`](./Secure/Core/Src/secure_nsc.c#L239), and [`NonSecure/AMI/Src/ami_dlms_apdu.c`](./NonSecure/AMI/Src/ami_dlms_apdu.c#L297).
- **Issue:** The factory provisioning token is a public compile-time constant, and the provisioning NSC function accepts it as its authorization check. The wrapped-key transfer NSC function validates inputs and unwrap integrity, but does not independently verify authenticated-session or provisioning authorization. DLMS-level HLS checks occur in NonSecure code and are not an authorization boundary for a directly callable Secure API.
- **Impact and conditions:** Code able to execute in NonSecure state could potentially call these NSC functions directly, bypassing the protocol-layer authorization and changing Secure-held keys. This is not evidence of a remote APDU-only route or an observed compromise.
- **Recommended remediation:** Replace the public token with a Secure-enforced provisioning lifecycle and authorization policy. Require the Secure service to validate an authorized session or a separately authenticated, one-time provisioning mechanism before key changes. Persist key updates atomically in protected storage.

## Safe validation

Use isolated host tests or a dedicated development board. Test the Secure backend against AES-GCM known-answer vectors, verify altered tags are rejected without dispatching plaintext, and verify direct NSC key-management calls fail without Secure-enforced authorization. Do not use production meters or third-party networks for these checks.

## Not validated

- Physical UART or other transport exposure and end-to-end remote reachability.
- Runtime SAU/IDAU/GTZC attribution, CMSE enforcement, and peripheral isolation.
- SAES, RNG, key-storage persistence, or power-loss behavior on STM32U585 hardware.
- Whether a production image enables the reviewed cipher backend or exposes the NSC calls under its final build configuration.

## Recommended remediation before production

1. Configure and verify the product-specific fail-closed association policy; constrain any permitted no-auth association to approved client roles and peer SAPs.
2. Replace fixed/reused proof counters and the fallback software RNG with a qualified Secure-world source and durable, correctly managed counters.
3. Keep production authentication keys and cryptographic operations in a validated Secure-world service; do not link test/fallback key-handling implementations into release images.
4. Reassess reachability and validate TrustZone, transport, and security behavior on the target hardware after integration.
5. Replace the Secure World cipher demonstration with a reviewed AES-GCM implementation before enabling ciphered DLMS traffic.
6. Enforce key-management authorization inside Secure World and persist key changes atomically in protected storage.
