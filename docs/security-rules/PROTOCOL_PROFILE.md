# E2 DLMS/COSEM Host Baseline & Standards Alignment

## Status

This document records the E2 target protocol profile, alignment with official PT PLN (Persero) technical standards (`STANDAR_PLN/`), and the implementation baseline currently active and verified in host tests. The values below serve as the formal architectural reference prior to physical STM32U585 hardware validation.

## PLN Technical Reference Standards (`STANDAR_PLN/`)

The DLMS/COSEM protocol stack and security architecture are strictly aligned with the following official standard documents:

1. **SPLN D3.006:2021** (*Three-Phase AC Postpaid Static Meters Class 0.2S, 0.5S, and 1*):
   - Master standard for technical requirements, metrology, insulation/dielectric testing, EMC, and telemetry for 3-phase postpaid meters.
   - Includes supplements: **SPLN D3.006:2021 SUP.1:2022** and **SPLN D3.006:2021 SUP.2:2023**.
2. **SPLN D3.006-2:2021 / SPLN D3.006.2.2021.CSV** (*OBIS Code Table Structure and Postpaid Meter Parameter Attributes*):
   - Reference mapping for all 276 OBIS (Object Identification System) codes in `NonSecure/AMI/Inc/ami_obis_catalog.h`.
   - Includes supplements: **SPLN D3.006.2:2021 SUP.1:2022** and **SPLN D3.006.2:2021 SUP.2:2023**.
3. **SPLN D3.023:2021** (*AMI Smart Meter Interoperability and Data Communication Testing Specification*):
   - Test guidelines for HDLC protocol conformance, frame construction, timing, and Head-End System (HES) integration.
4. **SPLN D5.002:2024** (*Cybersecurity Specification for Smart Grid and Advanced Metering Infrastructure Systems*):
   - Current PLN AMI cybersecurity governance standard: cryptographic hardware isolation, brute-force mitigation, key lifecycle management (KEK), strict prohibition of plaintext password transmission (LLS) over open networks, and anti-replay Invocation Counter enforcement.
5. **IEC 62056 Series** (*DLMS/COSEM International Standards*):
   - IEC 62056-5-3 (Application Layer & Security Suite 0/1 AES-GCM).
   - IEC 62056-6-1 (OBIS Identification System).
   - IEC 62056-6-2 (COSEM Interface Classes).
   - IEC 62056-46 (Data Link Layer using HDLC Protocol).

---

## Selected E2 Target Profile (Firmware Policy)

| Parameter | E2 Target Configuration | PLN Standard Compliance Basis |
|---|---|---|
| **Addressing Method** | Logical Name (LN) referencing | SPLN D3.006:2021 & IEC 62056 |
| **Client HDLC SAP** | `16` (`0x10`) | Dedicated E2 profile client SAP |
| **Server HDLC Address** | Physical `1`, Logical `1` (Server Address `1`) | IEC 62056-46 / SPLN D3.023:2021 |
| **Client Authentication** | **HLS 5 (GMAC)** OID `2.16.756.5.8.2.5` | SPLN D5.002:2024 & IEC 62056-5-3 |
| **Cipher Suite** | DLMS Security Suite 0 (AES-128-GCM / GMAC) | SPLN D3.006:2021 & SPLN D5.002:2024 |
| **Challenge Length** | Exactly 16 octets (Mutual 4-pass handshake) | IEC 62056-5-3 HLS Mechanism 5 |
| **Normal Data Services** | `GET-Request-Normal` & Class 7 Selective Access | SPLN D3.006:2021 (Read-Only Baseline) |
| **Access Policy** | Read-Only (rejects general `SET` and `ACTION`) | Billing transaction integrity protection |
| **Key Management** | Remote Key Transfer RFC 3394 AES Key Wrap via KEK | SPLN D5.002:2024 & Class 64 Security Setup |
| **Key Transfer Authorization** | Requires peer SAP & authenticated identity verification | Prevents unauthorized key replacement |

*Policy Note:* Target firmware fails closed by rejecting `No-Auth` and `LLS` associations. Enforcing HLS5 on SAP `16` prevents unauthenticated access paths on the data communication channel.

---

## Current Host Baseline

| Area | Implemented & Host-Tested Baseline |
|---|---|
| **Referencing** | Logical Name (LN) referencing verified |
| **Transport Harness** | Raw HDLC-over-TCP port `4059` on loopback simulator |
| **Association State Machine** | `AARQ`/`AARE`, dynamic conformance negotiation, ciphered initiate, mutual 4-pass HLS5 GMAC |
| **Anti-Brute-Force Lockout** | HLS5 failure tracking, progressive backoff (5s to 40s), hard lockout after 5 failures (SPLN D5.002:2024) |
| **Block Transfer (GBT)** | xDLMS Datablock transfer (`0xC0 0x02` / `0xC4 0x02`) for large profile & buffer payloads |
| **Data Services** | `GET-Request-Normal`, Datablock Transfer, and Class 7 selective access (23-channel 15-min Load Profile, 3-capture-object Tamper Logbook with 9 PLN Tamper Modes) |
| **Load Control Breaker** | Disconnect Control Class 70 state machine (`Disconnected`, `Connected`, `Ready`) & Action Methods 1/2 |
| **Access Rights** | Comprehensive Association View (66 active objects tested via `GET` attribute 2 & GBT) |
| **Host Simulator Auth** | `No-Auth` permitted for local development/simulator environment only |
| **Firmware Auth Policy** | Rejects `No-Auth`, `LLS`, `HLS3/MD5`, `HLS4/SHA-1`, and non-16-byte challenges |
| **Data Source** | Safe snapshot sample data for logic verification without ADE9000 hardware |

---

## Registered Objects in Association View (66 Active Objects)

The Association View is the authoritative object list advertised by the meter to the DLMS Client/HES in accordance with SPLN D3.002:2021, SPLN D3.006-2:2021, and IEC 62056-6-2:

| No | Class ID | Logical Name (OBIS) | Object Description (PLN Postpaid Standard) |
|---:|---:|---|---|
| 1 | 15 | `0.0.40.0.0.255` | Association LN |
| 2 | 8 | `0.0.1.0.0.255` | Clock (Meter real-time clock) |
| 3 | 1 | `0.0.42.0.0.255` | Logical Device Name |
| 4 | 1 | `1.0.0.2.0.255` | Software / Firmware Version |
| 5 | 1 | `0.0.96.1.0.255` | Meter Serial Number (Meter ID) |
| 6 | 1 | `0.0.96.1.1.255` | Meter Type |
| 7 | 64 | `0.0.43.0.0.255` | Security Setup |
| 8 | 7 | `0.0.99.98.0.255` | Event Profile (Operational Event Log #1 - 2 columns) |
| 9 | 7 | `1.0.98.1.0.255` | Billing Profile (Monthly historical billing data) |
| 10 | 7 | `1.0.99.1.0.255` | 15-minute Load Profile (23 channels) |
| 11 | 7 | `1.96.98.128.0.255` | Instant Profile (11 instantaneous measurement channels) |
| 12 | 7 | `1.0.99.14.2.255` | Power Quality Voltage Log Profile Phase R (Sag/Swell R) |
| 13 | 7 | `1.0.99.14.3.255` | Power Quality Voltage Log Profile Phase S (Sag/Swell S) |
| 14 | 7 | `1.0.99.14.4.255` | Power Quality Voltage Log Profile Phase T (Sag/Swell T) |
| 15 | 1 | `1.135.32.0.0.255` | Power Quality Voltage Register Phase R |
| 16 | 1 | `1.135.52.0.0.255` | Power Quality Voltage Register Phase S |
| 17 | 1 | `1.135.72.0.0.255` | Power Quality Voltage Register Phase T |
| 18 | 3 | `1.0.32.7.0.255` | Instantaneous Voltage Phase R (Phase A Voltage) |
| 19 | 3 | `1.0.52.7.0.255` | Instantaneous Voltage Phase S (Phase B Voltage) |
| 20 | 3 | `1.0.72.7.0.255` | Instantaneous Voltage Phase T (Phase C Voltage) |
| 21 | 3 | `1.0.31.7.0.255` | Instantaneous Current Phase R (Phase A Current) |
| 22 | 3 | `1.0.51.7.0.255` | Instantaneous Current Phase S (Phase B Current) |
| 23 | 3 | `1.0.71.7.0.255` | Instantaneous Current Phase T (Phase C Current) |
| 24 | 3 | `1.0.1.8.0.255` | Total Active Import Energy (+A) |
| 25 | 3 | `1.0.2.8.0.255` | Total Active Export Energy (-A) |
| 26 | 3 | `1.0.3.8.0.255` | Total Reactive Import Energy (+R) |
| 27 | 3 | `1.0.4.8.0.255` | Total Reactive Export Energy (-R) |
| 28 | 3 | `1.0.1.8.1.255` | Active Import Energy Tariff 1 (LWBP 1) |
| 29 | 3 | `1.0.1.8.2.255` | Active Import Energy Tariff 2 (WBP) |
| 30 | 3 | `1.0.1.8.3.255` | Active Import Energy Tariff 3 (LWBP 2) |
| 31 | 3 | `1.0.1.8.4.255` | Active Import Energy Tariff 4 |
| 32 | 3 | `1.0.2.8.1.255` | Active Export Energy Tariff 1 |
| 33 | 3 | `1.0.2.8.2.255` | Active Export Energy Tariff 2 |
| 34 | 3 | `1.0.2.8.3.255` | Active Export Energy Tariff 3 |
| 35 | 3 | `1.0.2.8.4.255` | Active Export Energy Tariff 4 |
| 36 | 3 | `1.0.3.8.1.255` | Reactive Import Energy Tariff 1 |
| 37 | 3 | `1.0.3.8.2.255` | Reactive Import Energy Tariff 2 |
| 38 | 3 | `1.0.3.8.3.255` | Reactive Import Energy Tariff 3 |
| 39 | 3 | `1.0.3.8.4.255` | Reactive Import Energy Tariff 4 |
| 40 | 3 | `1.0.14.7.0.255` | Mains Frequency (Hz) |
| 41 | 3 | `1.0.13.7.0.255` | Total Power Factor |
| 42 | 3 | `1.0.33.7.0.255` | Power Factor Phase R (L1) |
| 43 | 3 | `1.0.53.7.0.255` | Power Factor Phase S (L2) |
| 44 | 3 | `1.0.73.7.0.255` | Power Factor Phase T (L3) |
| 45 | 3 | `1.0.12.7.124.255` | Total Voltage THD (%) |
| 46 | 3 | `1.0.32.7.124.255` | Voltage THD Phase R (L1) |
| 47 | 3 | `1.0.52.7.124.255` | Voltage THD Phase S (L2) |
| 48 | 3 | `1.0.72.7.124.255` | Voltage THD Phase T (L3) |
| 49 | 3 | `1.0.11.7.124.255` | Total Current THD (%) |
| 50 | 3 | `1.0.31.7.124.255` | Current THD Phase R (L1) |
| 51 | 3 | `1.0.51.7.124.255` | Current THD Phase S (L2) |
| 52 | 3 | `1.0.71.7.124.255` | Current THD Phase T (L3) |
| 53 | 3 | `1.0.1.7.0.255` | Total Instantaneous Active Power (+P, Watt) |
| 54 | 3 | `1.0.3.7.0.255` | Total Instantaneous Reactive Power (+Q, var) |
| 55 | 71 | `0.0.17.0.3.255` | Demand Limiter Class 71 (Power Limit Parameters) |
| 56 | 70 | `0.0.96.3.10.255` | Disconnect Control Class 70 (Load Breaker Control) |
| 57 | 3 | `1.0.15.8.0.255` | Combined Total Active Energy (+A + -A) |
| 58 | 3 | `1.0.12.31.0.255` | Low Voltage Limit |
| 59 | 3 | `1.0.12.33.0.255` | Low Voltage Validation Delay |
| 60 | 1 | `1.0.12.32.0.255` | Low Voltage Event Count |
| 61 | 3 | `1.0.13.31.0.255` | Compensation Threshold Power Factor |
| 62 | 6 | `0.0.14.0.0.255` | Tariff Register Activation |
| 63 | 11 | `0.0.11.0.0.255` | Special Days Table |
| 64 | 20 | `0.0.13.0.0.255` | Activity Calendar |
| 65 | 22 | `0.1.15.0.4.255` | Single Action Schedule (Historical Data Scheduler) |
| 66 | 22 | `0.0.15.0.2.255` | Single Action Schedule (Billing Scheduler) |

*Object Note:* The full catalog covers 276 OBIS codes (`ami_obis_catalog.h`). All 66 objects exposed in the Association View have real data-model mappings and are functionally verified via the host test suite. Large object-list responses are automatically segmented via xDLMS DataBlock Transfer (GBT).

---

## Conformance Testing Guidelines (DLMS CTT & Gurux)

1. Use **Logical Name (LN)** referencing and the **HDLC** interface when testing with GXDLMSDirector.
2. *Authentication None* mode is permitted only on the host development simulator.
3. All 66 objects in the Association View must respond to `GET` attribute 2 with success status.
4. Set `ExcludeBasicTests` to `False` in CTT for full diagnostic auditing.
5. Archive test logs separately for the host simulator and STM32 target firmware binaries.

---

## Production Security Completion Gates

In accordance with SPLN D5.002:2024 and the STM32U5 TrustZone architecture, production readiness requires:

1. **Secure-World Cryptographic Services:** Integration of STM32U585 hardware SAES/AES-GCM and hardware TRNG isolated in the Secure World.
2. **Manufacturing KEK Provisioning:** Storage of a unique per-meter Master Key (KEK) in Secure Flash/OTP with no plaintext key material in the NonSecure image.
3. **Atomic Wrapped Key Installation:** RFC 3394 key unwrapping and atomic installation of operational keys (GUEK/AK) in the Secure World.
4. **Strict Peer Authorization:** Key transfer authorization must validate the registered peer identity and permitted SAP.
5. **Durable Monotonic Counter:** Power-loss-resilient Invocation Counter storage (*wear-leveling NVM*) to prevent replay attacks and *nonce reuse*.
6. **STM32U5 Target Penetration Testing:** Verification of frame manipulation rejection, tampered tag rejection, power-loss during key writes, and TRNG failure handling.
