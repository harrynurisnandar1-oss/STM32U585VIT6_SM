# Security Review

## Scope and conclusion

This review examined the Secure and NonSecure firmware sources, build wiring, DLMS association/APDU and cryptographic paths, and host tests. It found three conditional security-readiness issues. The review did not establish an externally reachable exploit path in the current firmware: the DLMS task is started, but its receive hooks are not wired to a physical UART in the reviewed sources. These findings should be resolved before production integration.

## Findings

### 1. DLMS port defaults to unauthenticated access

- **Severity:** Medium
- **Confidence:** 9/10
- **Evidence:** [`NonSecure/AMI/Src/ami_dlms_port.c`](./NonSecure/AMI/Src/ami_dlms_port.c#L28), [`NonSecure/AMI/Src/ami_dlms_association.c`](./NonSecure/AMI/Src/ami_dlms_association.c#L434), [`NonSecure/AMI/Src/ami_dlms_apdu.c`](./NonSecure/AMI/Src/ami_dlms_apdu.c#L445)
- **Issue and impact:** A newly initialized port has custom security disabled, so AARQ processing uses the legacy association helper that permits no-auth association. The APDU ciphering guard does not require ciphering for `AUTH_NONE`. If connected to a communication interface, an unauthenticated client could associate and issue plaintext GET requests, exposing available meter readings or billing data.
- **Reachability:** No firmware call to the UART receive hooks or configured UART was found in the reviewed sources. This is a conditional release/integration risk, not evidence that the present image exposes a reachable service.

### 2. HLS proof counter reuse and predictable fallback RNG

- **Severity:** Medium
- **Confidence:** 8/10
- **Evidence:** [`NonSecure/AMI/Inc/ami_security.h`](./NonSecure/AMI/Inc/ami_security.h#L139), [`NonSecure/AMI/Src/ami_security.c`](./NonSecure/AMI/Src/ami_security.c#L363), [`NonSecure/AMI/Src/ami_security.c`](./NonSecure/AMI/Src/ami_security.c#L478)
- **Issue and impact:** The HLS context uses a fixed invocation counter, and the proof callback reads it without advancing it. The association exchange invokes proof generation for both sides, so using this callback unchanged can reuse the GMAC IV with the same key. The fallback software RNG also starts from a fixed seed, causing sequences to repeat after reset. If these callbacks are enabled with unchanged key/counter state, proof freshness is weakened and replay may be possible when the challenge repeats.
- **Reachability:** The review found no production initialization of these HLS callbacks or random provider; their use appears in tests. This is a conditional security-readiness issue, not a demonstrated current authentication bypass.

### 3. HLS credentials are stored in NonSecure memory

- **Severity:** Medium
- **Confidence:** 9/10
- **Evidence:** [`NonSecure/AMI/Inc/ami_security.h`](./NonSecure/AMI/Inc/ami_security.h#L139), [`NonSecure/AMI/Src/ami_security.c`](./NonSecure/AMI/Src/ami_security.c#L491), [`NonSecure/CMakeLists.txt`](./NonSecure/CMakeLists.txt#L59), [`Secure/Core/Src/secure_nsc.c`](./Secure/Core/Src/secure_nsc.c#L51)
- **Issue and impact:** The HLS context stores the authentication key directly, and its callback accesses it from NonSecure code. The software AEAD backend also uses the caller's key pointer as backend state; `ami_security.c` is compiled into the NonSecure image. The Secure NSC implementation currently registers callbacks rather than providing a cryptographic/key-storage service. If this path is enabled, compromise of NonSecure code could expose the HLS key, contrary to the intended TrustZone separation.
- **Reachability:** No firmware call site or provisioned HLS key was found. This is a production-readiness gap, not an active key disclosure demonstrated in the current build.

## Recommended remediation before production

1. Require an explicit, fail-closed production association policy; ensure no-auth associations cannot be selected by default.
2. Replace fixed/reused proof counters and the fallback software RNG with a qualified Secure-world source and durable, correctly managed counters.
3. Keep production authentication keys and cryptographic operations in a validated Secure-world service; do not link test/fallback key-handling implementations into release images.
4. Reassess reachability and validate TrustZone, transport, and security behavior on the target hardware after integration.
