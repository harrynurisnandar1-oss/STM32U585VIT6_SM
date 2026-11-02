# Security Review

## Scope and conclusion

This review examined the Secure and NonSecure firmware sources, build wiring, DLMS association/APDU and cryptographic paths, and host tests. It found three conditional security-readiness issues. The review did not establish an externally reachable exploit path in the current firmware: the DLMS task is started, but its receive hooks are not wired to a physical UART in the reviewed sources. These findings should be resolved before production integration.

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

## Recommended remediation before production

1. Configure and verify the product-specific fail-closed association policy; constrain any permitted no-auth association to approved client roles and peer SAPs.
2. Replace fixed/reused proof counters and the fallback software RNG with a qualified Secure-world source and durable, correctly managed counters.
3. Keep production authentication keys and cryptographic operations in a validated Secure-world service; do not link test/fallback key-handling implementations into release images.
4. Reassess reachability and validate TrustZone, transport, and security behavior on the target hardware after integration.
