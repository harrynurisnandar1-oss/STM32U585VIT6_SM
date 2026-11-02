# E2 DLMS/COSEM Host Baseline

## Status

This document records the E2-selected protocol profile and the behavior currently implemented and testable on the host. The selected values below still require formal product/security approval and target validation; they are not a declaration of full IEC 62056/SPLN conformance.

## Selected E2 target profile

| Area | Selected target |
|---|---|
| Referencing | Logical Name (LN) |
| Client HDLC SAP | `16` only |
| Server HDLC address | Physical/logical `1/1`; the current firmware framing baseline uses server address value `1` |
| Client authentication | HLS5-GMAC only; no unauthenticated or LLS association on the selected firmware profile |
| Cipher suite | DLMS Security Suite 0 (AES-128-GCM/GMAC) |
| HLS challenge | Exactly 16 bytes for firmware associations |
| Ordinary data services | GET and the implemented Class 7 selective-access subset; read-only except the explicitly authorized key-transfer operation |
| Key provisioning/update | Remote key transfer using RFC 3394 wrapped keys under a per-device KEK provisioned during manufacturing; KEK and plaintext keys remain in Secure-world storage |
| Key-transfer authorization | HLS5 alone is not sufficient. Secure policy must additionally authorize the client identity and SAP for key transfer. |

This is the profile selected for E2 planning. It intentionally assigns HLS5 to SAP 16, so SAP 16 is not an unauthenticated public association in this product profile. The exception and departure from the conventional public-client behavior must be explicitly approved by the product/security owner and verified with the intended head-end client.

## Current host baseline

| Area | Implemented/tested baseline |
|---|---|
| Referencing | Logical Name (LN) |
| Host simulator transport | Raw HDLC over TCP on loopback; not DLMS Wrapper and not physical UART |
| Association | AARQ/AARE processing and association state handling covered by host tests |
| Application services | GET and the implemented Class 7 selective-access subset |
| Access policy | Read-only Association View; advertised attributes are read paths and no methods are advertised |
| Host simulator authentication | No-Authentication for development only |
| Firmware authentication | HLS5-GMAC OID with a 16-byte client challenge is the only HLS mechanism admitted by firmware policy; No-Authentication, LLS, HLS3, HLS4, and other challenge lengths are rejected |
| Data source | Caller-provided COSEM snapshot/sample data; no ADE9000 acquisition is performed by this host baseline |

## Objects currently exposed by Association View

Association View is the authoritative list for what this implementation announces to the DLMS client. The current bounded view contains:

| Class ID | Logical name (OBIS) | Meaning |
|---:|---|---|
| 15 | `0.0.40.0.0.255` | Association LN |
| 8 | `0.0.1.0.0.255` | Clock |
| 3 | `1.0.32.7.0.255` | Phase A voltage |
| 3 | `1.0.52.7.0.255` | Phase B voltage |
| 3 | `1.0.72.7.0.255` | Phase C voltage |
| 3 | `1.0.31.7.0.255` | Phase A current |
| 3 | `1.0.51.7.0.255` | Phase B current |
| 3 | `1.0.71.7.0.255` | Phase C current |
| 3 | `1.0.1.8.0.255` | Active energy import |
| 3 | `1.0.2.8.0.255` | Active energy export |
| 7 | `0.0.99.98.0.255` | Event profile |
| 7 | `1.0.98.1.0.255` | Billing profile |

The current 12-object host Association View is read-only. Remote key transfer is a selected target-profile exception, not part of this current view: Class 64, its method/access rights, protected authorization, and Secure-world callback/storage must be completed before key transfer can be used. Do not expose a placeholder method or report remote provisioning as implemented.

The OBIS catalog contains additional entries, but catalog entries are not automatically implemented or announced. Add an object to the product profile only after its class/version, attributes, data type, scaler/unit, access rights, snapshot source, and GET behavior are implemented and tested.

## Conformance test guidance

- Configure Gurux to use LN referencing and HDLC when testing the host simulator.
- Use Authentication None only for that development simulator. It is not representative of the firmware authentication policy.
- Run tests for the services and objects listed above. An object outside the approved profile may be recorded as out of scope; an advertised object that fails GET is an implementation defect.
- `ExcludeBasicTests = True` means those tests were skipped, not passed. Set it to `False` for diagnostic gap analysis and retain the detailed request/response trace for failures.
- Keep CTT results separate for host simulator and target firmware, and record endpoint, profile, and excluded tests.

## Not included in this baseline

This host profile does not establish support for Short Name referencing, general writable objects, image transfer, all catalog OBIS entries, production HLS/key provisioning, physical UART, ADE9000 acquisition, or metrology accuracy. The key-transfer operation is an approved design requirement but is not yet a production implementation.

## Production security completion gates

Phase 1 cannot be declared complete until all of these are implemented and verified:

1. A Secure-world HLS5/Suite-0 crypto service using a qualified hardware-backed CSPRNG and protected per-device credentials. The current Secure-world NSC file contains only callback registration; the protocol's callback interfaces are not a production crypto service. NonSecure AES initialization and its HAL driver integration have been removed, but this is not a replacement crypto service.
2. Manufacturing provisioning of a unique per-device KEK into protected Secure storage, with no default key or plaintext key embedded in either image.
3. Secure-world RFC 3394 unwrap and atomic installation of received GUEK/authentication-key batches; plaintext keys never cross the NSC boundary.
4. An additional secure authorization decision for key transfer that binds the authenticated peer identity and SAP to a provisioned allow-list; HLS authentication alone must not authorize key replacement.
5. Durable, atomic anti-replay RX and monotonic TX invocation-counter storage, plus power-failure and reset testing.
6. Firmware integration tests on the STM32U585 target for HLS5 challenge/proof, encrypted APDUs, rejected legacy mechanisms, unauthorized key transfer, key-update interruption, RNG health failure, and counter persistence.

Until these gates pass, firmware must remain fail-closed and the HLS5/key-provisioning target profile remains unvalidated for production.
