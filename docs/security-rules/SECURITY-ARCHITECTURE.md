# Security Architecture

## Design principles

- Keep the protocol stack in NonSecure world, but keep long-term secrets and security-critical cryptographic decisions in Secure world.
- Use explicit, fail-closed policy at each boundary. Do not rely on development defaults for production.
- Validate inputs at the earliest layer that has enough context, then repeat authorization checks at the operation that changes or reveals protected data.
- Keep interfaces narrow, bounded, typed, and auditable. Prefer fixed-size buffers and explicit lengths over heap-based message construction.
- Make security state persistent when reset must not undo it; make session state ephemeral and clear it on teardown.

## Recommended component layout

```text
External DLMS client
        |
        v
Transport adapter (UART/optical, DMA, timeout, bounded queues)
        |
        v
HDLC link layer (frame bounds, HCS/FCS, peer SAP, N(S)/N(R), link state)
        |
        v
DLMS/ACSE association (approved auth allow-list, session binding, timeout)
        |
        +---- plaintext only before approved ciphering policy permits it
        v
Ciphered APDU adapter (suite checks, authenticate-before-use, anti-replay)
        |
        v
COSEM authorization + data model (OBIS allow-list, attributes, methods, validity)
        |
        +---- consistent read-only snapshot from metrology/storage layer

NonSecure protocol code -- narrow NSC calls --> Secure security service
                                                   |-- RNG
                                                   |-- AES-GCM / HLS proof
                                                   |-- key unwrap/install
                                                   |-- durable RX/TX counters
                                                   +-- protected key storage
```

The diagram describes the intended secure integration. The current FreeRTOS default task does not yet implement the transport-to-APDU path, and the Secure NSC file currently contains callback registration rather than the product security service.

## Layer responsibilities

### 1. Transport adapter

- Own UART/optical DMA buffers and define their lifetime; do not let DMA modify a buffer while a parser consumes it.
- Use bounded queues, explicit timeout and idle-frame behavior, and backpressure. Discard incomplete or overlong streams and resynchronize on a valid HDLC flag.
- Keep simulator, debug console, and test-only paths out of production builds unless explicitly configured.

### 2. HDLC link layer

- Parse one bounded frame at a time; validate frame format, exact declared length, address encoding, HCS/FCS, and negotiated maximum information length.
- Compare source and destination SAPs to the configured client/meter identity. Track link state and enforce `N(S)`/`N(R)` transitions; reset sequence state only on valid link establishment/teardown.
- Do not dispatch an I-frame payload to DLMS until all link-layer checks succeed. Treat CRC correctness as integrity against transmission errors, not authentication against an attacker.

### 3. Association and authorization

- Construct production policy explicitly using `ami_dlms_process_aarq_with_security()` or a stricter wrapper. Do not call the legacy no-auth `ami_dlms_process_aarq()` in production.
- Bind authenticated identity, mechanism, negotiated context, peer SAPs, client/server titles, and session state together. Reject unexpected new AARQ requests while a session is active unless a defined teardown policy has run.
- Enforce per-object and per-method access rights independently of authentication success. HLS success must not automatically grant every COSEM operation.
- Use bounded retries, lockout/backoff policy appropriate for meter availability, and clear challenges on every success, failure, timeout, and reset path.

### 4. Ciphered APDU and Secure service

- The NonSecure APDU layer may parse the DLMS security-control field and invocation counter, but only the Secure backend decides whether key handles and cryptographic operations are permitted.
- Verify authentication tags before parsing or acting on plaintext. Commit accepted RX counters only after successful authentication and persistent atomic update.
- Reserve the next TX counter durably before encryption. If durable storage fails, do not encrypt or transmit.
- Provide NSC operations such as random generation, HLS proof, authenticated decrypt/encrypt, wrapped-key transfer, and counter reservation/commit. Each operation validates NS pointers and sizes, copies sensitive inputs into Secure-owned memory if required, and zeroizes temporary material.
- Keep key unwrap, KEK use, and key installation entirely in Secure world. The NonSecure side should pass only bounded wrapped key blobs and symbolic key types.
- Do not expose generic callbacks or unrestricted peripheral access as a substitute for a Secure service API.

### 5. COSEM data and metrology

- Read from an immutable or atomically copied snapshot. Validate all producer counts against declared capacities before indexing arrays.
- Expose only approved OBIS entries, classes, attributes, and selective-access modes. Invalid samples must map to value-unavailable or a protocol error, never a fabricated zero.
- Keep billing/event persistence, timestamp source, rollover, and integrity policy in the board/storage layer; the model should not claim records are durable unless that layer guarantees it.
- Make Profile Generic selection bounded and deterministic; reject unsupported selectors and column projections rather than returning broader data.

### 6. Boot, updates, and platform controls

- Secure boot authenticates both images before NonSecure execution. Signed updates validate product identity, version, signature, and anti-rollback counter before activation.
- Define flash/SRAM/peripheral attribution in one reviewed partition configuration. Secure peripherals and key storage must remain inaccessible to NonSecure DMA and bus masters.
- Lock production debug access according to the threat model and document the recovery/provisioning path.
- Separate development and production build configurations. Production builds must fail configuration if no-auth policy, test crypto backend, development credentials, or simulator service is enabled.

## State and failure handling

| State/data | Location | Lifetime / handling |
|---|---|---|
| Long-term DLMS keys and KEK | Secure protected storage | Persist; rotate using authenticated atomic procedure |
| RX/TX invocation counters | Secure protected persistent storage | Monotonic per key/profile; commit atomically |
| HLS challenges and transient proofs | NonSecure session buffer or Secure scratch | Short-lived; clear on completion, error, timeout, or reset |
| Decrypted APDU plaintext | Bounded transient workspace | Consume only after authentication; zeroize on all exits |
| Metrology/profile snapshot | Board/application data layer | Consistent copy; bounds-checked; no secret material |
| HDLC parser/link state | Per-link context | Reset on valid link lifecycle, not arbitrary client input |

Any Secure service error, invalid pointer/length, failed RNG, counter storage failure, authentication failure, or partial key-transfer condition shall fail closed. The error path shall clear sensitive temporary data and leave durable state either unchanged or fully committed, never partially updated.

## Integration gates

Before enabling the device protocol task, complete the threat model and checklist, approve the DLMS/SPLN product profile, implement the Secure service, verify the TrustZone map and boot chain on hardware, and add tests for hostile frames, reset/power failure, counter persistence, and task/resource limits. Host simulator success is not a release gate for device security by itself.

