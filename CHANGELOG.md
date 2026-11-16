# 📋 Firmware Release Changelog: STM32U585VIT6 + ADE9000 Smart Meter

Dokumen ini mencatat seluruh riwayat pembaruan fitur, perbaikan bug, pengerasan keamanan (*security hardening*), dan verifikasi pengujian firmware Smart Meter 3-Fasa.

---

## 📌 Ringkasan Fase Pengembangan (Spin 1 vs Spin 2)

* **🚀 [SPIN 1 (v1.0.0 s.d. v2.12.0)](#-spin-1--fondasi-protokol-arsitektur-trustzone--integrasi-awal)**:
  * Fondasi protokol komunikasi DLMS/COSEM (IEC 62056) & ACSE Association.
  * Parser streaming HDLC Frame Format 3 zero-heap dengan validasi CRC-16 (HCS/FCS).
  * Arsitektur keamanan ARM Cortex-M33 TrustZone (pemisahan Secure vs NonSecure).
  * Kriptografi DLMS Security Suite 0 (AES-128-GCM) & Otentikasi HLS 5 (GMAC).
  * Manajemen kunci terisolasi (Master Key KEK & Remote Key Wrap RFC 3394).
  * Wear-leveling NVM Invocation Counter anti-replay & integrasi host simulator TCP (port 4059).

* **🏭 [SPIN 2 (v2.13.0 s.d. v2.21.0)](#-spin-2--pelengkapan-fitur-produksi-kesiapan-mains-board--sertifikasi)**:
  * Anti-Brute-Force & Lockout progresif eksponensial (SPLN D5.002:2024).
  * General Block Transfer (GBT) untuk segmentasi data profil dan objek besar.
  * Ekspansi Data Model COSEM PLN hingga **66 objek aktif** pada Association View.
  * Logika pemutus beban Disconnect Control Breaker (Class 70) dengan multi-mode.
  * Histori Profiling Class 7: Load Profile 15-menit (23-kanal), Billing bulanan, Tamper Logbook (9 modus), PQ Voltage Log (R/S/T), dan Instant Profile (11 kanal).
  * Kesiapan driver target (HAL UART circular DMA non-blocking & SPI ADE9000 fail-closed).
  * Pengerasan keamanan 4 Pilar: Code Coverage 79.2% (91.9% crypto), Exhaustive Bit-Flip Negative Testing, Compiler Memory Barrier Zeroization, dan TrustZone Boundary Overflow Checks.
  * Verifikasi hermetis Docker Alpine Linux (15/15 CTest Suites lulus 100% + Python E2E Client).

---

## 📑 Daftar Isi Cepat (*Quick Jump*)

| Fase | Rentang Rilis | Sorotan Utama |
|---|---|---|
| **SPIN 2** | [v2.21.0](#v2210---2026-11-16) *(Terbaru)* | Remediasi F1/F2/C1/I5/A1/RDP (Pure C): ECDSA P-256 Secure Boot, Flash Keystore, Anti-TOCTOU, IWDG, 16/16 CTest |
| | [v2.20.0](#v2200---2026-11-09) | 4 Pilar Keamanan, Code Coverage 79.2%, 15/15 CTest PASS, TrustZone boundary check |
| | [v2.19.0](#v2190---2026-11-06) | Profil Kualitas Tegangan R/S/T, Instant Profile 11-kanal, Calendar/Schedule Class |
| | [v2.18.1](#v2181---2026-11-04) | Remediasi SIGFPE division-by-zero, Docker runner cmake, unit test resilience |
| | [v2.18.0](#v2180---2026-11-04) | Load Profile 23-kanal 15-menit & Tamper Logbook 9 Modus PLN |
| | [v2.17.0](#v2170---2026-11-04) | Driver UART DMA IDLE line, SPI ADE9000 return-code guard, HLS5 proof NSC |
| | [v2.16.0](#v2160---2026-11-03) | Disconnect Control Breaker Class 70 (Mode 1..6, remote/manual disconnect) |
| | [v2.15.0](#v2150---2026-11-03) | Ekspansi Association View dari 13 menjadi 49 Objek Aktif Standar PLN |
| | [v2.14.0](#v2140---2026-11-03) | xDLMS Datablock Transfer / General Block Transfer (GBT) payload besar |
| | [v2.13.0](#v2130---2026-11-03) | Anti-Brute-Force Lockout SPLN D5.002:2024 (Penalti 5s..40s & Hard Lockout 5x) |
| **SPIN 1** | [v2.12.0](#v2120---2026-11-03) | Remediasi CIA Triad: MPCBB SRAM3, anti TOCTOU, TRNG fail-secure |
| | [v2.6.0 s.d. v2.11.0](#v260---2026-11-02) | Suite 0 Ciphered APDU (AES-GCM), NVM Anti-Replay Counter, NSC Gateway |
| | [v2.0.0 s.d. v2.5.2](#v200---2026-10-13) | LLC Sublayer wrapping, HDLC Parameter Handshake, Standalone TCP Simulator |
| | [v1.0.0 s.d. v1.4.1](#v100-added-08102026) | Fondasi BER-TLV AARQ/AARE, HLS 5 Challenge, Class 64 Security Setup |

---

# 🚀 SPIN 1 — Fondasi Protokol, Arsitektur TrustZone & Integrasi Awal

## Scope and limitations

Sprint 1 is an initial protocol slice, not a complete DLMS/COSEM stack. It does not parse HDLC frames, validate client/server addresses, negotiate an InitiateRequest/InitiateResponse user-information payload, implement selective column projection, or transfer large responses using block transfer. Ciphered APDUs and Class 7 selective access are present, but production security depends on the Secure-world SAL callbacks described below. Pass application APDUs here only after the HDLC layer validates framing and addressing. This code is not ready for network exposure or product deployment.

The HLS mechanism OID and cryptographic proof function are configured by the product security profile. This project does not claim a particular HLS algorithm until that profile and its keys are integrated through the TrustZone Secure service.

## Standards references

The implementation scope is intended for review against IEC 62056-5-3, IEC 62056-6-2, and SPLN D3.006:2021. Verify BER/A-XDR encoding, authentication mechanism details, diagnostic values, conformance negotiation, OBIS mappings, and product security requirements against controlled editions before claiming compliance. No conformance certification is claimed.

## Build and run host checks

Host tests are configured with CMake and run with CTest:

```powershell
cmake -S tests -B tests/build -G Ninja
cmake --build tests/build --parallel
ctest --test-dir tests/build --output-on-failure
```

These checks validate host logic only; they do not validate FreeRTOS scheduling, STM32U585 TrustZone behavior, or UART/HDLC operation. Docker and CMock are not part of this configuration.

## [v1.0.0] Added 08/10/2026 

- Added bounded BER-TLV parsing for AARQ and building of AARE using logical-name application context OID `2.16.756.5.8.1.1`.
- Added initial unauthenticated development association flow and association state tracking.
- Added bounded xDLMS `GET-Request-Normal` parsing without selective access and `GET-Response-Normal` encoding for supported COSEM values and access errors.
- Added read-only Class-8 Clock support at logical name `0.0.1.0.0.255`: attribute 2 (12-byte date-time), attribute 3 (time-zone minutes), and attribute 4 (clock status).
- Added CTest host suites for COSEM model, security facade, DLMS association, and DLMS APDU logic.

## [v1.1.0] Added 09/10/2026 

### LLS and HLS association authentication

- Added LLS AARQ parsing for mechanism OID `2.16.756.5.8.2.1`, ACSE authentication requirement, and calling-authentication-value. Password verification is delegated to `verify_lls`; the password is not copied into the association context.
- Added configurable HLS mechanism OID and challenge exchange. The meter generates a 16-byte server challenge through the configured CSPRNG callback and returns it in AARE. Association stays `AUTH_PENDING` until `ami_dlms_process_hls_reply()` validates the client proof and generates the server proof for the client's challenge.
- Added bounded challenge storage, constant-time proof comparison, cleanup on reset/failure, and tests for successful/failed LLS plus the HLS challenge transition.
- Added callbacks for LLS verification, random challenge generation, and profile-specific HLS proof generation. Production callbacks are expected to bridge to protected credential/key storage and cryptographic services in the TrustZone Secure world.

### Security limitations

- HLS algorithm details vary by selected mechanism. The exact mechanism OID, key use, and proof calculation must be taken from the approved product profile. This code provides an integration hook; it does not claim a production HLS-GMAC implementation.
- LLS sends its password in the AARQ without cryptographic protection. Use LLS only on a physically trusted/protected link; use the product-approved HLS profile where stronger peer authentication is required.
- The legacy `ami_dlms_process_aarq()` function remains as a no-auth development entry point. Production code should call `ami_dlms_process_aarq_with_security()` and explicitly configure permitted authentication mechanisms.

## [v1.2.0] Added 10/10/2026 

### Global-ciphered APDU after HLS

- HLS-associated sessions now reject plaintext GET requests. The new APDU entry point accepts only global-ciphered GET (`GLO-GET-REQUEST`, `0xC8`) and SET (`GLO-SET-REQUEST`, `0xC9`) requests; it authenticates/decrypts the inner APDU and emits global-ciphered GET/SET responses (`0xCC` / `0xCD`).
- Added a Secure-world SAL backend contract for AES-GCM processing. It receives the security-control byte and invocation counter and is responsible for deriving nonce/AAD from the approved DLMS profile, system titles, and Secure-held keys.
- Added fail-closed checks for the configured security-control value, authentication tag, and receive invocation counter. Receive counter acceptance must be atomic/persistent; transmit counters must be allocated durably before encryption to prevent reuse.
- Ciphered SET requests are decrypted and returned as encrypted `read-write-denied` responses because the meter's COSEM model is intentionally read-only.
- Added host tests for plaintext rejection after HLS, ciphered GET response generation, tampered-tag rejection, and ciphered read-only SET denial. These tests use a deliberately non-cryptographic fake backend to test framing/state only; they do not validate AES-GCM implementation.

### Integration requirements

- The Secure backend must implement the product-selected DLMS security suite, key selection, system-title handling, nonce/AAD construction, and 128-bit tag policy. The receive-counter callback must reject replay/rollback and commit only authenticated counters; the transmit callback must persist a fresh counter before use.
- No concrete STM32U585 TrustZone AES-GCM backend or provisioned keys are included here. Until that SAL callback is implemented and validated against approved DLMS test vectors, do not use the test backend or treat this feature as production-ready.

## [v1.3.0] Added 12/10/2026

### Profile Generic Class 7 for event and billing history

- Added two read-only Profile Generic objects to the COSEM model: the standard event-log logical name `0.0.99.98.0.255` and a billing-history profile configured as `1.0.98.1.0.255`. The billing OBIS instance is a product-profile choice and must be checked against the applicable PLN/SPLN meter specification before release.
- Added a bounded, fixed-capacity snapshot for up to eight event records and eight billing-period records. Event rows contain a 12-byte COSEM date-time and event code; billing rows contain a date-time plus active-import and active-export energy counters. The board/metrology layer supplies these rows and manages timestamp generation and rollover; no fake records are generated by the model.
- Implemented read-only Class 7 attributes 1–8: logical name, A-XDR buffer array, capture-object list, trigger-based capture period (`0`), FIFO sort method, timestamp sort object, entries in use, and profile capacity. Event capture objects describe Clock and event-code data; billing capture objects describe Clock and the active-import/export registers.
- Added A-XDR passthrough for already encoded array/structure values so Class 7 buffer and capture-object attributes can travel through the existing GET response path, including the global-ciphered path after HLS.
- The snapshot arrays are provided oldest first; producers must keep the counts at or below `AMI_COSEM_PROFILE_MAX_ENTRIES` and handle circular-buffer policy. Reads of buffer and entries-in-use return value-unavailable until their corresponding profile-validity bit is set.

### Scope notes

- This adds basic full-buffer reads only. Selective access by entry range or date range, profile capture/reset methods, persistent storage, and a FreeRTOS producer task are not implemented in this slice.
- Class 7 A-XDR encoding and the configured billing OBIS must be reviewed against the controlled IEC 62056 and product/SPLN profile editions. This change alone does not establish DLMS conformance.

## [v1.4.0] Added 12/10/2026

### SPLN D3.002:2021 OBIS review and date-range ordering

- Reviewed the provided `obis.md` inventory as the authoritative SPLN D3.002:2021 reference. Repeated logical names in the source table are recorded as source-profile entries and were not “corrected” based on neighboring OBIS patterns.
- Fixed Class 7 time-range ordering to compare calendar/time fields only (year through hundredths). Day-of-week, deviation, and clock status no longer affect range validation or row inclusion.
- The firmware COSEM model still implements only its explicitly listed register, Clock, Security Setup, and Profile Generic objects. The remaining SPLN OBIS objects require corresponding snapshot data, access semantics, and product mapping before they can be exposed; this review does not claim full SPLN object coverage.

### Three-phase measurement OBIS catalog

- Added the read-only Class-3 measurement OBIS entries from SPLN D3.002:2021 for neutral values, frequency, phase/total power factor, active/reactive/apparent power, and per-phase active/reactive/apparent energy.
- Added a bounded snapshot sample bank keyed by logical name. The COSEM model supplies the catalog-defined data type, scaler, and unit; metrology/board code supplies raw values and validity, so missing measurements return value-unavailable instead of a valid zero.
- The OBIS row for Q2 phase-S energy repeats `1.0.45.8.0.255` from Q1 phase-S in the supplied table. It is intentionally omitted from the additional catalog pending clarification against the controlled standard; no substitute OBIS is guessed.
- Demand registers, tariffs, power-quality metrics, event profiles, and control/configuration objects remain outside this measurement-register slice.

## [v1.4.1] Added 13/10/2026

### Security Setup Class 64 and wrapped key transfer

- Added Security Setup metadata object `0.0.43.0.0.255`, Class 64 version 1, with read-only attributes for logical name, security policy, security suite, client/server system titles, and an empty certificates array when no certificate store is configured.
- Corrected the requested class number: Class 15 is Association LN; Security Setup is Class 64. The Class 64 model does not expose plaintext keys as readable attributes.
- Added ciphered Action Request support for Security Setup method 2 (`key_transfer`) after HLS. The accepted Suite 0 key-type values are 0 (global unicast encryption key) and 2 (authentication key). Incoming 16-byte keys are represented as 24-byte AES-128 key-wrap values under the KEK.
- The Secure-world callback receives the wrapped batch and is responsible for RFC 3394 unwrap, KEK protection, integrity validation, atomic installation of all requested keys, and rejecting a failed/partial update. NonSecure code does not unwrap or retain plaintext keys. Host tests use a fake callback and do not verify key cryptography or hardware isolation.

### Profile Generic selective access

- Extended Get-Request-Normal parsing for Class 7 buffer attribute 2: selector 1 filters by Clock date-time range; selector 2 filters by entry range (one-based, inclusive; `to_entry = 0` means through the newest entry).
- Range descriptors must identify Clock `0.0.1.0.0.255`, attribute 2, and use complete date-time values. Entry descriptors must select all columns (`from_selected_value` and `to_selected_value` both zero). Unsupported selectors, range objects, and column projections fail with `scope-of-access-violated`; the parser never silently returns the full log for an unsupported selection.
- Selective access is applied before A-XDR encoding, so the response contains only matching rows. Large response block transfer and partial-column projection remain future work.

### VS Code CMake Tools for host tests

- Added a host-test CMake preset in `tests/CMakePresets.json` and workspace settings in `.vscode/settings.json` that point CMake Tools to `tests/`, separate from the CubeMX firmware CMake project at the repository root.
- Updated `tests/README.md` with the reason the extension previously selected the ARM firmware project and the steps to configure/build/run host CTest from VS Code.

### HDLC stream framing engine (ISO/IEC 13239 & IEC 62056-46)

- **Frame Format 3 Architecture (`ami_dlms_hdlc.h`, `ami_dlms_hdlc.c`)**:
  - Implemented compliant byte-stream framing based on the DLMS HDLC profile (Frame Format Type 3, `0xA0` prefix with 11-bit length encoding).
  - Built a zero-heap streaming state machine (`ami_dlms_hdlc_parse_byte`) with deterministic O(1) buffer bounds (`AMI_HDLC_MAX_FRAME_BYTES = 544u`), completely eliminating dynamic memory allocation and buffer overflow vectors.
  - Implemented variable multi-octet HDLC addressing (1 to 4 bytes per SAP) conforming to DLMS server/client logical device conventions (supporting up to 28-bit addresses with LSB extension-bit handling).
  - Implemented dual-stage CCITT CRC-16 (X.25 polynomial `0x8408` inverted) verification:
    - **HCS (Header Check Sequence)**: Validates destination, source, and control fields before payload processing.
    - **FCS (Frame Check Sequence)**: Validates complete frame integrity over all transmitted octets.
- **Control & Information Frame Dispatch (`ami_dlms_hdlc_build_frame`)**:
  - Added native response generator for Supervisory/Unnumbered frames: auto-responds to connection requests (`SNRM`, `0x93`) and disconnections (`DISC`, `0x53`) with Unnumbered Acknowledge (`UA`, `0x73`) while properly reversing source and destination SAPs.
  - Built bidirectional Information (`I`) frame transmission with 3-bit send/receive sequence counter rotation (`N(S)` and `N(R)`).
- **Target Integration & Testing**:
  - Registered `ami_dlms_hdlc.c` into the `NonSecure` ARM Cortex-M33 firmware build.
  - Added comprehensive CTest test suite `AmiDlmsHdlcTest` (`test_ami_dlms_hdlc.c`) validating standard CCITT CRC test vectors (`"123456789"` -> `0x906E`), variable address encoding/decoding boundaries, invalid address rejection, truncated inputs, HCS/FCS tampering detection, and I-frame APDU roundtrips.

### Threat-hunting hardening & memory safety

Conducted an in-depth security threat hunting audit across input parsers, cryptographic boundaries, and state machines, resolving the following vulnerabilities:

- **CWE-226: Sensitive Residual Key Material in RAM (`ami_dlms_apdu.c`)**:
  - *Vulnerability*: Post-execution of Security Setup Class 64 `key_transfer` Actions, `memset` only wiped `response_plain_length` (5 bytes), leaving raw 24-byte wrapped AES keys lingering in `workspace` memory.
  - *Fix*: Enforced full-capacity zeroization across the entire caller-owned `workspace` buffer on every path after decryption, ensuring all ephemeral keys and plaintext APDUs are thoroughly scrubbed.
- **CWE-20 / CWE-703: HDLC Stream Desynchronization & Flag Lockup (`ami_dlms_hdlc.c`)**:
  - *Vulnerability*: Injected headers with inflated length fields followed by premature `0x7E` delimiters caused the parser to interpret flags as payload data, locking up stream synchronization until buffer saturation.
  - *Fix*: Enforced strict delimiter synchronization: an incoming `0x7E` on frames shorter than expected immediately aborts the invalid frame and resynchronizes to the next delimiter.
- **CWE-384: Session Desynchronization on Reconnection (`ami_dlms_hdlc.c`)**:
  - *Vulnerability*: Subsequent `SNRM` or `DISC` frames generated `UA` acknowledgments without resetting sequence counters, causing subsequent `I`-frame sequence number mismatches with newly connected test clients.
  - *Fix*: Added atomic reset of sequence counters (`tx_sequence = 0u`, `rx_sequence = 0u`) inside `ami_dlms_hdlc_build_frame()` upon generating `UA` responses for `SNRM`/`DISC`.
- **CWE-284: Active Session Teardown DoS (`ami_dlms_association.c`)**:
  - *Vulnerability*: Stray or unauthenticated AARQ bytes received while a valid HLS session was active forced the state machine directly into `AMI_DLMS_ASSOC_ERROR`, severing active HES communications.
  - *Fix*: Protected active sessions by cleanly rejecting stray AARQ requests without corrupting active association state or invalidating active challenge/session keys.
- **CWE-703: Fail-Closed Transactional A-XDR Serialization (`ami_cosem_model.c`)**:
  - *Vulnerability*: Buffer exhaustion during Profile Generic (Class 7) multi-row encoding broke loop execution but still proceeded to finalize partial headers, emitting corrupted A-XDR streams to clients.
  - *Fix*: Made Profile Generic encoding transactional: any partial row encoding failure immediately aborts the response and returns `AMI_COSEM_INVALID_ARGUMENT`.
- **Regression Verification**:
  - Added strict assertion coverage across host unit tests confirming zeroization, session preservation under malformed inputs, framing recovery, and clean sequence rollover.

## [v2.0.0] - 2026-10-13

### Added
- **LLC Sublayer Auto-Wrapping & Stripping (`ami_dlms_hdlc.c`)**:
  - Menambahkan deteksi dan pelepasan 3-byte LLC header (`0xE6 0xE6 0x00` / `0xE6 0xE7 0x00`) pada stream I-frame masuk (RX).
  - Menambahkan penyisipan otomatis header LLC respons (`0xE6 0xE7 0x00`) pada pembuatan frame balasan (TX).
  - *Rasional*: Memenuhi kepatuhan IEC 62056-46 agar paket APDU yang dikirim meter dikenali secara valid oleh software DLMS pihak ketiga (seperti Gurux DLMSDirector) tanpa dianggap sebagai frame tidak dikenal (*unsupported format*).
- **HDLC Parameter Negotiation pada Handshake SNRM (`ami_dlms_hdlc.c`)**:
  - Menambahkan fungsi `make_ua_parameters` untuk mengekstrak usulan parameter buffer client dari payload SNRM dan mengembalikan format parameter balasan (`0x81 0x80 ...`) pada frame UA. Ukuran TX/RX dinegosiasikan saling terbalik sesuai arah aliran data dan dibatasi aman (maksimal 512 byte, window size = 1).
  - *Rasional*: Mencegah timeout koneksi pada client resmi yang mewajibkan konfirmasi parameter batas ukuran frame sebelum membuka sesi data.
- **Standalone Interactive TCP Host Simulator (`tests/gurux_simulator.c`)**:
  - Menambahkan executable simulator socket TCP WinSock2/POSIX pada port `4059` yang menghubungkan parser stream HDLC dengan mesin dispatching APDU/COSEM.
  - *Rasional*: Memungkinkan pengujian *black-box* interaktif secara menyeluruh (*end-to-end*) menggunakan GUI GXDLMSDirector di PC host sebelum perangkat keras board fisik tersedia.

### Changed
- **Peningkatan Diagnostik Live Logger Simulator (`tests/gurux_simulator.c`)**:
  - Mengubah output logging terminal simulator untuk mencetak jenis frame, kontrol byte, SAP pengirim/tujuan, ukuran payload, dan deteksi byte pertama secara real-time.
  - *Rasional*: Mempercepat identifikasi kesalahan format pengalamatan (Addressing/SAP mismatch) atau ketidaksesuaian protokol transport (WRAPPER vs HDLC) saat client mencoba terhubung.

### Fixed
- **Penghitungan Panjang Frame Format 3 Sesuai Gurux (`ami_dlms_hdlc.c`)**:
  - Mengoreksi field panjang 11-bit pada Frame Format Type 3 agar mencakup seluruh oktet dari awal frame format hingga byte akhir FCS.
  - *Rasional*: Memperbaiki kegagalan validasi frame pada Gurux yang menolak frame jika penghitungan panjang hanya mencakup payload internal.
- **Cakupan HCS/FCS CRC-16 HDLC (`ami_dlms_hdlc.c`)**:
  - Menyelaraskan rentang byte yang dihitung oleh CRC HCS dan FCS agar mencakup Frame Format dan panjang oktet, serta menambahkan vektor uji SNRM riil Gurux (`7E A0 08 02 03 2D 93 26 CE 7E`) ke dalam suite pengujian.
  - *Rasional*: Menjamin interoperabilitas penuh perhitungan checksum CRC terhadap implementasi referensi resmi Gurux.

## [v2.1.0] - 2026-10-14

### Added
- **Dynamic DLMS Conformance Negotiation (IEC 62056-5-3 )**:
  - Menambahkan ekstraksi bitmask `proposed_conformance` dari payload xDLMS `InitiateRequest` pada `parse_aarq` (`ami_dlms_association.c`).
  - Mengimplementasikan kalkulasi negosiasi conformance secara dinamis menggunakan operasi bitwise AND antara proposal client dan layanan yang diimplementasikan server (`Get` `0x10` dan `SelectiveAccess` `0x04`).
  - Menolak asosiasi (`result = 1, diagnostic = 2`) apabila hasil negosiasi conformance menghasilkan irisan kosong (tidak ada kesamaan layanan antara client dan meter).
- **Kepatuhan Pengujian CTT `T_APPL_IDLE_N1` & `T_APPL_OPEN_9`**:
  - Mengimplementasikan *silent drop* untuk permintaan data `GET` pada kondisi unassociated (sesuai aturan CTT `T_APPL_IDLE_N1`), sembari tetap merespons permintaan Short Name Read (`0x05`) dengan `ExceptionResponse` (*service not supported*) untuk verifikasi status disconnect.
  - Menolak AARQ secara permanen jika client mengirimkan `dedicated_key` pada `InitiateRequest` saat ciphering/security suite tidak diaktifkan (`T_APPL_OPEN_9`).
- **Toleransi Kolom Entry Selector Profile Generic (`ami_dlms_apdu.c`)**:
  - Menyesuaikan parser `parse_profile_selection` agar menerima nilai kolom indeks 0 atau 1 pada pengujian `ReadRowsByEntry` dari Gurux CTT.
  - Menginisialisasi data snapshot simulasi awal (1 baris *Event Log* dan 1 baris *Billing History*) pada `tests/gurux_simulator.c` agar atribut *buffer* mengembalikan struktur data valid.

### Changed
- **Penyelarasan Mask Layanan Server Terverifikasi (`ami_dlms_association.c`)**:
  - Mengubah conformance mask default server dari `0x00 0x50 0x1F` (yang mengiklankan Set, Action, Event Notification tanpa implementasi) menjadi mask faktual Sprint 1: `0x00 0x00 0x14` (`Get` + `SelectiveAccess`).
  - Menyelaraskan seluruh unit test asosiasi host (`test_ami_dlms_association.c`) terhadap nilai conformance mask baru.
- **Validasi & Negosiasi Max PDU Size (`T_APPL_OPEN_14`)**:
  - Menyesuaikan negosiasi batas ukuran PDU pada balasan AARE agar mengikuti batas penerimaan client jika di bawah 500 byte, dan menolak ukuran AARQ jika di bawah batas minimum frame xDLMS valid.

### Fixed
- **Verifikasi 100% CTest Suite**:
  - Memperbarui seluruh host test CTest (`AmiCosemTest`, `AmiSecurityTest`, `AmiDlmsAssociationTest`, `AmiDlmsApduTest`, `AmiDlmsHdlcTest`), seluruh 5 test suite lulus 100%.

### Scope & CTT Testing Notes
- **Meter Karakteristik Read-Only**: Pengujian `Write` pada CTT harus dimatikan (*disabled/unchecked*) karena meter Sprint 1 secara arsitektur menolak seluruh operasi `SET`/penulisan register.
- **Profile Generic Range Filtering**: Seleksi parsial kolom (*column projection*) dan *selective access by date range* mendalam dialokasikan pada Sprint 3.
- **Sprint 2 Handoff**: Fondasi komunikasi HDLC framing, AARQ/AARE state machine, negosiasi conformance, dan pembacaan objek dasar (LDN, Firmware, Clock) dinyatakan selesai dan siap dilanjutkan ke Sprint 2 (Authentication LLS/HLS & Security Setup Class 64).

## [v2.1.1] - 2026-10-14

### Fixed
- **Perbaikan GitHub Actions Workflow CI (`.github/workflows/ci.yml`)**:
  - Mengoreksi versi action checkout dari `actions/checkout@v7` (versi non-eksisten/typo) menjadi `actions/checkout@v4` agar pipeline GitHub Actions dapat dieksekusi tanpa error resolusi action.
  - Memperluas matriks pengujian CI menjadi dual-platform:
    - **Host Tests (Ubuntu Linux)**: Eksekusi tes berbasis `Ninja` + `GCC` untuk kecepatan dan determinisme.
    - **Host Tests (Windows)**: Eksekusi tes native Windows menggunakan CMake MSVC toolchain untuk memverifikasi kompatibilitas socket WinSock2 (`ws2_32`) dan library host.
- **Header Kompatibilitas Socket Linux (`tests/gurux_simulator.c`)**:
  - Menambahkan `#include <netinet/tcp.h>` pada cabang POSIX/Linux agar konstanta socket `TCP_NODELAY` terdefinisi dengan benar di kernel Linux/GCC (penyebab kegagalan build pada runner Ubuntu).

### Added
- **Penegasan Standar C11 pada Host Tests (`tests/CMakeLists.txt`)**:
  - Menambahkan konfigurasi `CMAKE_C_STANDARD 11` dan `CMAKE_C_STANDARD_REQUIRED ON` pada build tests agar seluruh compiler (GCC, Clang, dan MSVC `/std:c11`) mengompilasi kode dengan standar C11 yang konsisten.
- **Penyempurnaan Aturan `.gitignore`**:
  - Mengabaikan direktori `.vscode/` secara penuh dan menghapus pelacakan file konfigurasi lokal (`.vscode/settings.json`) dari index git.
  - Menambahkan aturan ignore untuk artefak kompilasi host executable (`*.exe`, `*.dll`, `*.lib`, `*.so`), binary flash firmware (`*.axf`, `*.srec`, `*.s19`), direktori IDE (`Debug/`, `Release/`), dump/log files (`*.log`, `*.dmp`), dan temporary editor swap files (`*.swp`, `*~`).

## [v2.1.2] - 2026-10-14

### Added
- **Otomatisasi Modern CMake Workflow (`tests/CMakePresets.json`)**:
  - Memperbarui skema `CMakePresets.json` ke versi 6 (`cmakeMinimumRequired` 3.25.0) dengan penambahan `workflowPresets` (`host-tests`).
  - Mengintegrasikan rantai tahapan Configure (`Ninja`), Build (`Debug`), dan CTest otomatis dalam satu perintah (`cmake --workflow --preset host-tests`).
- **Integrasi DevTools & Debugger VS Code (`.vscode/`)**:
  - **`launch.json`**: Menambahkan konfigurasi native GDB (`cppdbg`) untuk debugging interaktif satu-klik pada binary `gurux_simulator.exe` serta unit test individual (`test_ami_dlms_association`, `test_ami_dlms_apdu`, `test_ami_dlms_hdlc`).
  - **`tasks.json`**: Menambahkan automated tasks `CMake: Build Host Tests`, `CMake: Configure Host Tests`, `CTest: Run All Tests`, dan `CMake: Run Full Workflow`.
  - **`settings.json`**: Menetapkan direktori sumber host tests dan mengaktifkan preset secara deterministik untuk integrasi CMake Tools extension di VS Code.

### Fixed
- **Validasi CMakePresets Version Compatibility**:
  - Mengatasi error validasi `should NOT have additional properties: workflowPresets` pada CMake Tools VS Code dengan meningkatkan versi format preset ke versi 6 dan menghapus deklarasi preset legacy yang tidak digunakan.
- **Sinkronisasi Direktori Output Build (`binaryDir`)**:
  - Menyelaraskan `binaryDir` pada `CMakePresets.json` ke `${sourceDir}/build` agar seragam dengan target path pada `tasks.json` dan `launch.json`.


## [v2.2.0] - 2026-10-26

### Added
- **Multi-Pipeline Continuous Integration (CI) Architecture (`.github/workflows/`)**:
  - **`windows-ci.yml`**: Pipeline khusus untuk pengujian dan build aplikasi native Windows (`runs-on: windows-latest`) menggunakan CMake dan Ninja, menjalankan eksekusi menyeluruh pada seluruh suite CTest host dan `gurux_simulator.exe`.
  - **`stm32u585-freertos-ci.yml`**: Pipeline cross-compilation embedded untuk target arsitektur ARM Cortex-M33 (STM32U585) dengan FreeRTOS. Mengompilasi multi-context TrustZone (`Secure` dan `NonSecure`), memverifikasi konsumsi memori footprint (`arm-none-eabi-size`), dan mengunggah artefak binary firmware (`.elf`, `.bin`, `.hex`, `.map`).
  - **`cpp-analysis.yml`**: Pipeline analisis statis dan audit keamanan kode otomatis menggunakan GitHub CodeQL (`security-extended` & `security-and-quality`) untuk mendeteksi memory leak, buffer overflow, boundary check error, serta potensi celah keamanan logika pada implementasi protokol DLMS/COSEM dan HDLC framing.

### Fixed
- **Resolusi Header Missing `math.h` pada Embedded ARM Toolchain (`stm32u585-freertos-ci.yml`)**:
  - Memperbaiki kegagalan preprocessor `fatal error: math.h: No such file or directory` pada saat mengompilasi HAL driver STM32 (`stm32u5xx_hal_def.h`).
  - Menginstal paket C standard library target ARM Newlib (`libnewlib-arm-none-eabi` dan `libstdc++-arm-none-eabi-newlib`) bersamaan dengan `gcc-arm-none-eabi` pada runner Ubuntu, sehingga seluruh header C runtime standar dan library `libm.a` tersedia secara lengkap dan deterministik.
- **Koreksi Multi-Context Artifact Paths**:
  - Menyesuaikan jalur pengumpulan artefak firmware pada runner CI agar mengambil file hasil build dari direktori TrustZone `Secure/build/` dan `NonSecure/build/` sesuai struktur `ExternalProject_Add` STM32CubeMX.


## [v2.2.1] - 2026-10-28

### Added
- **Konfigurasi Workspace VS Code & Integrasi CTest Explorer (`.vscode/settings.json`, `.vscode/tasks.json`)**:
  - Menetapkan `cmake.sourceDirectory` ke `${workspaceFolder}/tests` dengan preset otomatis `host-tests` agar Test Explorer dan tombol CTest pada extension VS Code CMake Tools langsung mendeteksi serta menjalankan seluruh 5 unit test suite host secara interaktif.
  - Menyediakan automated tasks pada `tasks.json` untuk memudahkan build firmware ARM STM32 (`cmake --build --preset Debug`), eksekusi CTest host, dan menjalankan `gurux_simulator.exe`.

### Fixed
- **Toolchain Environment CI Windows (`.github/workflows/windows-ci.yml`)**:
  - Menambahkan step `ilammy/msvc-dev-cmd@v1` sebelum tahap Configure CMake pada runner `windows-latest`. Memastikan toolchain compiler C MSVC (`cl.exe`) otomatis terdaftar pada environment `$PATH` ketika CMake dipanggil dengan generator `Ninja`, mencegah kegagalan `CMAKE_C_COMPILER not found`.
- **Refactoring & Eliminasi Warning Kompiler HDLC (`NonSecure/AMI/Src/ami_dlms_hdlc.c`)**:
  - Merefaktor formatting source code yang sebelumnya termampatkan dalam satu baris (*single-line compression*) menjadi struktur standar C yang rapi, menghilangkan seluruh peringatan kompiler GCC/Clang `-Wmisleading-indentation`.
  - Memperkuat ketahanan parsing frame control (`decode_raw`) dengan menambahkan masking bit P/F (Poll/Final `0x10`) pada evaluasi tipe frame `SNRM`, `UA`, dan `DISC`.
  - Membersihkan duplikasi blok kode yang sempat menyebabkan redefinisi fungsi (`get_crc`, `put_be32`, `decode_raw`, dll).
- **Inisialisasi Field C99/C11 Designated Initializers (`tests/test_ami_dlms_apdu.c`)**:
  - Memperbaiki deklarasi `AmiDlmsAssociation association` pada `test_voltage_get_request_response` dan `test_unknown_obis_returns_access_error` menggunakan designated initializer `.state = AMI_DLMS_ASSOC_ASSOCIATED`, mengeliminasi peringatan kompiler `-Wmissing-field-initializers`.
- **Penyelarasan Output Unit Test HDLC (`tests/test_ami_dlms_hdlc.c`)**:
  - Merapikan struktur pengujian, membersihkan duplikasi kode di luar fungsi `main()`, dan menambahkan laporan status eksplisit `DLMS HDLC tests passed.` saat test selesai, menyelaraskan konsistensi log output dengan suite CTest lainnya.
- **Koreksi Panduan & Path Hardcoded (`tests/test_note.md`)**:
  - Menghapus path hardcoded lokal non-portabel (`D:\STM32U585VIT6_SM`) dan menggantinya dengan petunjuk direktori proyek relatif yang portabel di berbagai workstation.
  - Memperbaiki target path kompilasi dan eksekusi `gurux_simulator.exe` dari `build/host-tools/` (folder build firmware STM32) menjadi `tests/build/host-tools/gurux_simulator.exe`.
- **Pembersihan Git Hygiene (`.gitignore`)**:
  - Menambahkan direktori `.settings/`, `.project`, dan `.cproject` agar file metadata lokal STM32CubeIDE/Eclipse tidak terdeteksi sebagai file untracked pada repositori git.
- **Restorasi Konfigurasi Presets Root (`CMakePresets.json`)**:
  - Membersihkan konfigurasi preset `Host_testing` yang sempat ditambahkan secara keliru pada file konfigurasi preset root firmware ARM.

## [v2.3.0] - 2026-10-28

### Added
- **Integrasi Suite Port ke CTest Host (tests/CMakeLists.txt, tests/test_ami_port.c)**:
  - Memasukkan ami_dlms_port.c dan test executable baru test_ami_port ke dalam target CTest host native sehingga teruji otomatis di CI dan mesin host tanpa bergantung Docker.
  - Memperbaiki penanganan AARQ pada ami_dlms_port.c agar unsolicited AARQ tidak mereset/memutus sesi terautentikasi yang sedang aktif.
  - Menambahkan API ami_dlms_port_set_security() pada port layer agar mode autentikasi (LLS/HLS) dapat dikonfigurasi secara eksplisit dan tidak memaksa mode no-auth.
  - Menangani error parsing APDU/GET agar kegagalan tidak memicu transmisi frame kosong/korup ke jalur serial.
  - Mengubah implementasi weak ami_dlms_hardware_uart_tx() agar mengembalikan 0 (gagal) sebelum UART fisik dihubungkan, mencegah respons yang belum terkirim dianggap sukses.
  - Menambahkan pengecekan return value ami_dlms_freertos_start_task() di MX_FREERTOS_Init() dengan fallback Error_Handler() jika alokasi RTOS gagal.
  - Membuka pelacakan Git pada direktori docker_test/ (hanya docker_test/build-docker/ yang diabaikan) agar seluruh test harness Docker dapat ditinjau dan direproduksi di CI.
- **Phase 2 Hardware Porting Abstraction Layer (NonSecure/AMI/Inc/ami_dlms_port.h, NonSecure/AMI/Src/ami_dlms_port.c)**:
  - Mengimplementasikan layer abstraksi porting perangkat keras (Hardware Abstraction Layer) yang memisahkan core stack DLMS/COSEM dari driver fisik UART/transceiver.
  - Menyediakan non-blocking byte stream dan buffer stream parser (ami_dlms_port_process_byte(), ami_dlms_port_process_buffer()) dengan zero-copy handling untuk efisiensi RAM mikrokontroler.
  - Menyediakan otomasi HDLC link-layer control: auto-response SNRM -> UA, DISC -> UA, RR keepalive, serta LLC framing encapsulation (0xE6 0xE6 0x00 / 0xE6 0xE7 0x00) pada pertukaran I-Frame APDU (AARQ/AARE, GET, SET, Action).
  - Menyediakan handler ISR cepat (ami_dlms_uart_rx_isr(), ami_dlms_uart_rx_buffer_isr()) yang kompatibel dengan interrupt UART Rx dan DMA circular buffer STM32 HAL.
  - Mengintegrasikan FreeRTOS task worker berprioritas tinggi (ami_dlms_freertos_start_task()) berbasis FreeRTOS StreamBufferHandle_t dan CMSIS-RTOS v2 osThreadNew(), diinisialisasi secara otomatis pada NonSecure/Core/Src/app_freertos.c (MX_FREERTOS_Init()).
- **Dedicated Docker Test Suite & Security Hardening (docker_test/)**:
  - Ditempatkan pada folder terisolasi docker_test/ tanpa menyentuh maupun menimpa folder tests/ yang telah ada.
  - Mengadopsi arsitektur Multi-Stage Build berbasis Alpine Linux (builder & minimal runner) yang mengeliminasi seluruh compiler, linker (binutils), dan package development dari image produksi.
  - Memangkas ukuran image secara drastis dari 407.13 MB menjadi hanya 23 MB (-94.3% footprint).
  - Mengeliminasi kerentanan keamanan (vulnerabilities) dari 2.463 CVE (16 Critical, 474 High) menjadi 0 Critical, 0 High, 0 Medium pada pemindaian Docker Scout.
  - Menyertakan test suite CTest baru test_ami_port.c untuk menguji lifecycle SNRM, DISC, AARQ, dan GET request via port abstraction.
  - Menyertakan client test Python otomatis test_dlms_e2e_client.py yang mensimulasikan software DLMS client (seperti Gurux DLMSDirector) untuk menguji full handshake: SNRM -> UA, AARQ -> AARE, GET Tegangan Fasa R (230 V), GET Jam Meter, dan DISC -> UA.
  - Dilengkapi script eksekusi run_tests.sh, Docker Compose (docker-compose.yml), Windows runner (run_docker_tests.bat), dan panduan penggunaan docker_test/README.md.
  - Terverifikasi 100% PASS pada seluruh 6 suite unit test dan 100% PASS pada pengujian integrasi End-to-End.

## [v2.4.0] - 2026-10-28

### Added
- **Katalog Lengkap Kode OBIS SPLN D3.006:2021 (`NonSecure/AMI/Inc/ami_obis_catalog.h`)**:
  - Mengintegrasikan seluruh 276 kode OBIS objek DLMS/COSEM dari dokumen spesifikasi teknis PLN ke dalam header C standar.
  - Setiap kode OBIS didefinisikan sebagai macro C berupa 6 octet array `{A, B, C, D, E, F}` lengkap dengan metadata keterangan nama objek, class ID COSEM, dan representasi notasi desimal OBIS (`A.B.C.D.E.F`).
  - Mengelompokkan seluruh 276 kode OBIS ke dalam 22 kategori fungsional terstruktur:
    1. Meter Identification & Device Information (Tipe meter, Meter ID, HW/SW version, Pelanggan ID, Unit PLN ID, Mode ekspor-impor, LDN).
    2. Meter Constants & Auxiliary Power (Konstanta aktif/reaktif, Kapasitas baterai, Kapasitas superkapasitor).
    3. Measurement Algorithms & General Configuration (Algoritma pengukuran energi aktif).
    4. Script Tables & Control I/O (Reset meter, Clear all tamper, Trigger test, Control I/O state/script, Limiter script, Tariff script, Push script, Alarm push 1/2, Image activation).
    5. Electrical Instantaneous Measurements (Frekuensi, Arus dan Tegangan fase R, S, T, dan Netral).
    6. Power Factor & Phase Angles (Power factor total & per fase, Sudut tegangan dan arus fase R, S, T, dan Netral).
    7. Instantaneous Power Registers (Daya aktif, reaktif, semu total dan per fase R, S, T kirim & terima).
    8. Cumulative Energy Registers (Energi aktif, reaktif, semu total dan per fase kirim & terima, Energi reaktif kuadran Q1..Q4 per fase, Billing compensation, Total active energy).
    9. Tariff-Based Energy Registers (Energi aktif, reaktif, dan semu kirim & terima untuk Tarif 1 s.d. 5).
    10. Demand & Maximum Demand Registers (Demand daya aktif, reaktif, semu kirim/terima, Maksimum demand total dan per Tarif 1 s.d. 5).
    11. Power Quality (PQ), Harmonics & Voltage Profiles (THD/TDD tegangan & arus per fase dan total, Low voltage limits/delay/count, PQ voltage registers R/S/T, PQ voltage log profiles R/S/T, PF batas kompensasi).
    12. Profiles (Data historikal / Billing profile, Scheduler billing, Load profile, Instant profile, Load profile descriptor & monitor).
    13. Power Limiter & Disconnect Control (Power limiter, Disconnect control / breaker).
    14. Alarm Registers, Status & Descriptors (Alarm register 1 & 2, Status register, Alarm descriptor 1 & 2, Alarm filter 1 & 2, Alarm monitor 1 & 2).
    15. Alarm Log Profiles (Flash error, RAM error, Clock loss, Low battery, MCU fault, ADC sampling fault, Supercap fault, Relay fault).
    16. Event Log Profiles (Power supply fail/up, ToU config change, Non-ToU config change, Stop measurement, I/O status change, Relay state change, Meter reset, Time/date change, Asymmetric power, Ready remote reconnect, Pinhole button, Manufacturer-specific events, Total meter off count).
    17. Tamper Log Profiles (Pembukaan tutup meter/terminal, Salah urutan fase, Arus/tegangan tidak sefase, Arus netral putus, Hilang tegangan fase, Medan magnet, Reverse power 3P/1P, Kuadran 4 low PF, Over limit, INSCEK, GNDCEK, Arus tidak seimbang 6 jam, Tutup terminal dibuka bertegangan/tanpa tegangan, Total kejadian tamper).
    18. Time, Clock & Calendar Management (Clock Class 8, Activity calendar, Tariff activation, Special days table).
    19. Push Setup & IP Registration (Push setup alarm 1/2, load profile, data historikal, IP registration).
    20. Image Transfer / Firmware Upgrade Management (Image transfer 0/1/2, Activation schedulers 0/1/2, Image activation script, Active firmware identifiers & signatures 0/1/2).
    21. COSEM Associations & Security Setup (Association LN 1 s.d. 6, Security setup LN 2 s.d. 6, Frame counter unicast & broadcast).
    22. Communication Ports & HDLC Setup (Optical port setup, Optical HDLC setup, RS232/RS485 HDLC setup).
  - Menjaga 100% kompatibilitas mundur (*backward compatibility*) dengan seluruh 65 macro yang telah digunakan pada implementasi `ami_cosem_model.c` dan unit test suite.
  - Memperbaiki anomali pada tabel dokumen PLN baris 39 (`Energi reaktif Q2 fase S` di mana kolom C bernilai 45 namun Kode OBIS tertulis `1.0.46.8.0.255`), dikoreksi menjadi octet `46` sesuai standar IEC 62056-6-2 / DLMS Blue Book dan diberikan catatan dokumentasi yang jelas.


## [v2.5.0] - 2026-10-29

### Added
- **Implementasi HLS Mechanism 5 (GMAC) & Cryptographic Core AES-128-GCM (`NonSecure/AMI/Inc/ami_security.h`, `NonSecure/AMI/Src/ami_security.c`)**:
  - Mengimplementasikan cryptographic core AES-128 standar NIST FIPS 197 (Forward cipher, S-Box substitution, ShiftRows, MixColumns, AddRoundKey, Key Expansion 10 round, Galois field `xtime`).
  - Mengimplementasikan GHASH dan operasi perkalian Galois Field GF(2^128) standar NIST SP 800-38D dengan polinomial reduksi R = 0xE1 || 0^120.
  - Mengimplementasikan NIST SP 800-38D Galois/Counter Mode (AES-128-GCM) authenticated encryption dan decryption (`ami_sec_aes_128_gcm_encrypt()`, `ami_sec_aes_128_gcm_decrypt()`) dengan IV/Nonce 96-bit (12 bytes) dan Tag 128-bit (16 bytes).
  - Mengimplementasikan AES-128-GMAC mode autentikasi data mandiri (`ami_sec_aes_128_gmac()`).
  - Mengimplementasikan kalkulasi dan verifikasi autentikasi DLMS/COSEM High Level Security (HLS) Mechanism 5 GMAC (`ami_sec_dlms_hls5_gmac_calculate()`, `ami_sec_dlms_hls5_gmac_verify()`):
    - OID Mekanisme HLS 5: `{2, 16, 756, 5, 8, 2, 5}` (`0x60, 0x85, 0x74, 0x05, 0x08, 0x02, 0x05`).
    - Format IV 96-bit: System Title (8 bytes) || Invocation Counter IC (4 bytes Big-Endian).
    - Format AAD: Security Control Byte (`0x10`, auth-only) || Authentication Key AK (16 bytes) || Challenge (up to 64 bytes).
    - Format tag proof: Security Control Byte (1 byte) || IC (4 bytes) || GMAC Tag (16 bytes) = 21 octet.
  - Constant-time verification (`ami_sec_constant_time_equals()`) untuk mencegah side-channel timing attack.
  - Volatile memory zeroization (`secure_zeroize()`) untuk membersihkan round key, subkey hash H, dan buffer sensitif dari RAM setelah eksekusi.
  - Adapter callback `ami_sec_hls5_gmac_proof_callback()` untuk integrasi langsung dengan state machine asosiasi DLMS/COSEM (`AmiDlmsAssociation`).
- **Verifikasi Uji Mandiri NIST SP 800-38D KAT & HLS 5 Handshake (`tests/test_ami_security.c`, `tests/test_ami_dlms_association.c`)**:
  - Menambahkan pengujian Known Answer Test (KAT) NIST SP 800-38D:
    - Test Case 1 (Empty Plaintext & AAD): Memvalidasi output Tag GMAC `58e2fccefa7e3061367f1d57a4e7455a`.
    - Test Case 2 (16B Plaintext, Empty AAD): Memvalidasi Ciphertext dan Tag GMAC `ab6e47d42cec13bdf53a67b21257bddf`.
    - Test Case 4 (60B Plaintext, 20B AAD): Memvalidasi Ciphertext dan Tag GMAC `5bc94fbc3221a5db94fae95ae7121a47`.
  - Memvalidasi tamper resilience: penolakan seketika dekripsi saat ciphertext atau tag dirusak 1 bit.
  - Memvalidasi fungsi GMAC authentication-only.
  - Memvalidasi end-to-end HLS 5 GMAC challenge-response calculation & verification.
  - Memvalidasi integrasi callback `ami_sec_hls5_gmac_proof_callback` pada 4-pass mutual authentication handshake di `test_ami_dlms_association.c`.
  - Seluruh 6 suite CTest lulus 100% (6/6 passed) dan build firmware Cortex-M33 bebas error/warning.
  
## [v2.5.1] - 2026-10-30

### Fixed
- **Penanganan Transparent Flag (0x7E) pada Payload & FCS HDLC (`NonSecure/AMI/Src/ami_dlms_hdlc.c`)**:
  - Memperbaiki kegagalan pembacaan objek DLMS/COSEM (seperti `0.0.96.96.0.255` dari software Gurux DLMSDirector) ketika nilai checksum CRC FCS atau payload I-frame mengandung byte `0x7E` sebelum akhir batas frame.
  - Menambahkan verifikasi integritas Header Check Sequence (HCS): saat terdeteksi byte `0x7E` sebelum panjang total frame tercapai (`c->frame_length < expected`), parser mengevaluasi HCS header terlebih dahulu. Jika HCS terotentikasi valid, byte `0x7E` diperlakukan sebagai transparent payload data/byte FCS yang sah tanpa memutus sinkronisasi stream.
- **Suite Uji HDLC Transparent Flag (`tests/test_ami_dlms_hdlc.c`)**:
  - Menambahkan test case `test_transparent_flag_in_payload_and_fcs()` yang memverifikasi frame riil Gurux dengan checksum berakhiran `0x7E 0x0A 0x7E`.

### Added
- **Sinkronisasi Katalog Objek SPLN D3.006:2021 pada Profil Gurux (`STM32U585VIT6_SM.gxc`)**:
  - Memetakan dan menyinkronkan 276 objek OBIS pascabayar standar PLN ke dalam file workspace Gurux DLMSDirector XML, meliputi meter identification, instantaneous measurement, billing profiles, event logs, tamper logs, dan disconnect control (breaker).

## [v2.5.2] - 2026-10-31

### Protocol and data-model baseline
- **Mendefinisikan baseline E2 untuk Phase 1 (`docs/security-rules/PROTOCOL_PROFILE.md`)**:
  - Mencatat profil host yang benar-benar diimplementasikan: Logical Name referencing, raw HDLC-over-TCP pada simulator, layanan GET dan subset selective access, kebijakan read-only, serta perbedaan autentikasi simulator dan firmware.
  - Mendokumentasikan 12 objek yang saat ini diumumkan dalam Association View dan membedakannya dari katalog OBIS yang lebih luas.
  - Menyatakan secara eksplisit bahwa dokumen ini adalah baseline pengembangan, bukan profil produksi yang disetujui atau klaim conformance penuh.
- **Menambah regresi keterpaduan Association View (`tests/test_ami_dlms_apdu.c`)**:
  - Memastikan semua objek yang diumumkan saat ini dapat menjawab GET attribute 2 dengan snapshot uji.
  - Memperjelas cakupan Phase 1: baseline host protokol/data model terbatas telah tercatat dan diuji; persetujuan profil produk dan validasi target masih diperlukan.
- **Memperbarui panduan Phase 1 (`README.md`, `tests/test_note.md`)**:
  - Menautkan baseline protokol/data model dan membedakan hasil host dari firmware, data ADE9000, dan metrologi.
- **Mencatat profil target E2 yang dipilih (`docs/security-rules/PROTOCOL_PROFILE.md`, `README.md`)**:
  - LN referencing; client SAP 16 saja dengan HLS5-GMAC dan Suite 0; server physical/logical 1/1; layanan GET dan selective access yang sudah diimplementasikan.
  - No-Auth/LLS tetap ditolak untuk firmware. HLS5 pada SAP 16 berarti tidak ada asosiasi public tanpa autentikasi dan memerlukan persetujuan produk/security formal.
  - Key update jarak jauh ditetapkan memakai RFC 3394 wrapped keys di bawah KEK per-device yang diprovisikan saat manufaktur. Key-transfer memerlukan otorisasi identitas peer/SAP tambahan; tidak dibuka pada Association View saat ini.
- **Membatasi firmware ke HLS5 dan alamat HDLC target (`ami_dlms_association.c`, `ami_dlms_port.c`)**:
  - Firmware menolak HLS selain HLS5-GMAC, challenge client selain 16 byte, serta No-Auth/LLS.
  - Firmware menolak HLS lawas meskipun konfigurasi callback meminta OID mekanisme tersebut.
  - Frame firmware hanya diproses untuk server address `1` dan client SAP `16`; host simulator tidak berubah.
- **Menambah regresi profil keamanan dan peer (`tests/test_ami_firmware_auth_policy.c`, `tests/test_ami_port.c`)**:
  - Menguji penolakan No-Auth, LLS, HLS3/MD5, HLS4/SHA-1, dan ukuran challenge yang salah; jalur HLS5 dengan challenge 16-byte masuk ke status `AUTH_PENDING` dengan callback test.
  - Menguji validasi alamat peer untuk profil firmware.
- **Mencatat gate provisioning produksi**:
  - Secure-world saat ini belum menyediakan layanan HLS5/Suite 0, CSPRNG yang tervalidasi, KEK per-device, atomic key storage/remote key transfer, otorisasi peer, atau counter invocation persisten.
  - Karena Phase 1 disepakati mewajibkan backend dan provisioning produksi, Phase 1 tetap belum selesai sampai gate pada `docs/security-rules/PROTOCOL_PROFILE.md` diimplementasikan dan diuji di STM32U585 target.

### Security hardening
- **Membatasi API kriptografi berbasis raw key hanya untuk pengujian host (`NonSecure/AMI/Inc/ami_security.h`, `NonSecure/AMI/Src/ami_security.c`)**:
  - API AES-128-GCM/GMAC, kalkulasi/verifikasi HLS 5 yang menerima key bytes, konteks HLS yang menyimpan authentication key, serta software backend berbasis pointer key hanya tersedia pada build host; API tersebut dikeluarkan dari build firmware STM32 (`STM32U585xx` atau `USE_HAL_DRIVER`).
- **Mempertahankan API tersebut di host tests (`tests/CMakeLists.txt`)**:
  - Tidak ada perubahan pada source test atau konfigurasi CMake host; test vectors dan test adapter tetap berfungsi seperti sebelumnya.
- **Menerapkan default autentikasi fail-closed pada firmware (`ami_dlms_port.c`, `ami_dlms_association.c`)**:
  - AARQ pada firmware diproses dengan policy yang dikonfigurasi. Jika policy belum dipasang, struktur policy bernilai nol dan menolak no-auth maupun mekanisme lain yang tidak dikonfigurasi.
  - Helper legacy no-auth `ami_dlms_process_aarq()` hanya disediakan pada build host/non-firmware, sehingga tidak dapat digunakan oleh firmware.
- **Menolak autentikasi no-auth dan LLS pada seluruh build firmware (`ami_dlms_association.c`, `ami_dlms_port.c`)**:
  - Jalur asosiasi firmware tidak menerima no-auth meskipun `allow_no_auth` diminta dalam konfigurasi; host tests dan simulator tetap mempertahankan perilaku yang lama.
  - `ami_dlms_port_set_security()` mengembalikan `AMI_DLMS_PORT_SECURITY_POLICY_REJECTED` apabila konfigurasi firmware mengizinkan no-auth atau memasang callback LLS. Policy aktif tidak berubah saat konfigurasi ditolak.
  - LLS dinonaktifkan pada semua link firmware sampai pengecualian link terlindungi disetujui dan dirancang secara eksplisit.
- **Menambahkan tes kebijakan autentikasi firmware (`tests/test_ami_firmware_auth_policy.c`)**:
  - Target tes dikompilasi dengan macro firmware STM32 dan memastikan AARQ no-auth serta LLS ditolak walaupun policy/callback tersebut diberikan.
- **Memperjelas status key protection (`docs/security-rules/SECURITY_CHECKLIST.md`, `SECURITY-REVIEW.md`)**:
  - Checklist kini menandai provisioning dan penyimpanan production key di Secure world sebagai belum selesai; keberadaan tipe `AmiKeyHandle` saja bukan bukti secure storage.
  - Security review mencatat bahwa pembatasan raw-key API mengurangi risiko paparan dari jalur tersebut di NonSecure firmware, tetapi Secure NSC key storage/provisioning dan layanan HLS berbasis key handle belum diimplementasikan.

### Verification
- Build firmware NonSecure berhasil; pemeriksaan object memastikan simbol API AES/HLS raw-key dan software backend tidak dikompilasi ke image.
- Seluruh suite CTest lulus; test policy firmware memastikan no-auth dan LLS ditolak, sementara test host lama tetap berjalan tanpa perubahan.
- Build NonSecure berhasil setelah penegakan HLS5-only dan peer address profile; CTest 7/7 lulus, termasuk penolakan HLS lawas serta pemeriksaan client SAP/server address.
- Perubahan ini **bukan** implementasi Secure-world key store dan belum membuktikan isolasi key pada hardware target.

## [v2.6.0] - 2026-11-02

### Added
- **DLMS Security Suite 0 Ciphered APDU Engine (`NonSecure/AMI/Inc/ami_dlms_cipher.h`, `NonSecure/AMI/Src/ami_dlms_cipher.c`)**:
  - Implementasi enkripsi dan dekripsi terotentikasi AES-128-GCM sesuai standar DLMS/COSEM Green Book (IEC 62056-5-3 / SPLN D5.002:2024).
  - Menangani konversi dan pemrosesan `glo-get-request` (`0xC8`) ke `glo-get-response` (`0xCC`), `glo-set-request` (`0xC9`) ke `glo-set-response` (`0xCD`), dan `glo-action-request` (`0xCB`) ke `glo-action-response` (`0xCE` / `0xCF`).
  - Verifikasi Security Control byte (`SC = 0x30`, Suite 0 Auth + Encrypt).
  - Pembentukan 12-byte Nonce / IV: `System Title (8 bytes) || Invocation Counter (4 bytes Big Endian)`.
  - Pembentukan 17-byte AAD: `Security Control (1 byte) || Authentication Key (16 bytes)`.
  - Verifikasi tag autentikasi 16-byte GCM dengan perbandingan waktu-konstan (`constant_time_memcmp`) dan pembersihan memori kerja (`secure_zeroize`) saat terjadi kegagalan autentikasi.
- **Mekanisme Persistensi Invocation Counter & Anti-Replay**:
  - Penegakan kenaikan strictly monotonic untuk Invocation Counter yang diterima (`rx_counter > last_rx_counter`).
  - Penolakan otomatis counter duplikat, bernilai nol, atau counter yang diputar ulang dengan status `AMI_DLMS_APDU_COUNTER_REJECTED`.
  - Alokasi counter transmisi server (`tx_counter`) secara monotonik per respons terenkripsi.
- **Integrasi Cipher pada Port DLMS (`NonSecure/AMI/Inc/ami_dlms_port.h`, `NonSecure/AMI/Src/ami_dlms_port.c`)**:
  - Penambahan API `ami_dlms_port_set_cipher_backend()` untuk menghubungkan backend cipher eksternal maupun TrustZone ke pipeline pemrosesan HDLC.
  - Perutean otomatis frame APDU `0xC8`, `0xC9`, dan `0xCB` yang masuk melalui `ami_dlms_process_ciphered_request()`.
- **Suite Uji CTest untuk DLMS Cipher (`tests/test_ami_dlms_cipher.c`)**:
  - `test_cipher_suite0_glo_get_roundtrip`: Pengujian end-to-end enkripsi, pengiriman, dan dekripsi respons Tegangan Fase A (`1.0.32.7.0.255`).
  - `test_cipher_suite0_anti_replay_invocation_counter`: Memvalidasi penerimaan counter baru dan penolakan counter lama/replay.
  - `test_cipher_suite0_tag_tampering`: Memvalidasi penolakan kriptografi dan zeroization buffer saat tag dirusak 1 bit.
  - `test_cipher_suite0_port_integration`: Memvalidasi framing HDLC yang membawa payload APDU terenkripsi.
- **Gerbang NonSecure Callable (NSC) TrustZone ARM Cortex-M33 (`Secure_nsclib/secure_nsc.h`, `Secure/Core/Src/secure_nsc.c`)**:
  - Implementasi gateway `CMSE_NS_ENTRY`: `SECURE_TRNG_GetBytes()`, `SECURE_DLMS_HLS5_Verify()`, dan `SECURE_DLMS_AES_GCM_Process()`.
  - Passing parameter berbasis struct pointer yang mematuhi konvensi register ARM AAPCS (`r0-r3`).
- **Driver Metrologi AFE ADE9000 & Feeder Snapshot (`NonSecure/AMI/Inc/ami_ade9000.h`, `NonSecure/AMI/Src/ami_ade9000.c`, `tests/test_ami_ade9000.c`)**:
  - Pemetaan register SPI 3-Fasa (RMS Tegangan/Arus, Daya Aktif/Reaktif, Energi, Frekuensi/Periode, Status).
  - Snapshot feeder yang mengonversi dan mengisi `AmiCosemRegisterSnapshot` untuk pembacaan real-time register DLMS/COSEM.

### Removed
- **Modul Token STS Prabayar (`ami_sts_token.h`, `ami_sts_token.c`, `test_ami_sts_token.c`, `SECURE_KEY_STS`)**:
  - Membersihkan seluruh decoder token prabayar, parser STS 20-digit, pengurangan kredit, dan buku besar kredit.
  - Menegaskan kembali arsitektur Smart Meter AMI 3-Phase **Pascabayar (Postpaid)** sesuai mandat SPLN D3.006:2021 dan batas area kerja E2 Protocol Data & Security.

## [v2.7.0] - 2026-11-02

### Added
- **Buffer Profile Generic Class 7 (Load Profile 15-menit `1.0.99.1.0.255` & Monthly Billing Freeze `0.0.98.1.0.255` / `1.0.98.1.0.255`)**:
  - Penambahan tipe data `AmiCosemLoadProfileEntry` dan array buffer `load_profile[]` pada `AmiCosemRegisterSnapshot` dengan kapasitas buffer hingga `AMI_COSEM_PROFILE_MAX_ENTRIES` (8 baris).
  - Penambahan flag validitas `AMI_COSEM_VALID_LOAD_PROFILE` (`1u << 14`) dan penyesuaian alokasi buffer encoded `AMI_COSEM_AXDR_ENCODED_MAX` menjadi 1024 bytes.
  - Implementasi 12 kolom capture object pada Load Profile 15-menit sesuai SPLN D3.006:2021:
    1. Clock (`0.0.1.0.0.255`, Class 8)
    2. Status Register (`0.0.96.10.1.255`, Class 1)
    3. Active Import Wh (`1.0.1.8.0.255`, Class 3)
    4. Active Export Wh (`1.0.2.8.0.255`, Class 3)
    5. Reactive Import varh (`1.0.3.8.0.255`, Class 3)
    6. Reactive Export varh (`1.0.4.8.0.255`, Class 3)
    7. Tegangan Fase A mV (`1.0.32.7.0.255`, Class 3)
    8. Tegangan Fase B mV (`1.0.52.7.0.255`, Class 3)
    9. Tegangan Fase C mV (`1.0.72.7.0.255`, Class 3)
    10. Arus Fase A mA (`1.0.31.7.0.255`, Class 3)
    11. Arus Fase B mA (`1.0.51.7.0.255`, Class 3)
    12. Arus Fase C mA (`1.0.71.7.0.255`, Class 3)
  - Implementasi 5 kolom capture object pada Monthly Billing Freeze: Clock, Active Import Wh, Active Export Wh, Reactive Import varh, Reactive Export varh.
  - Penanganan dinamis attribute Profile Generic:
    - Attribute 2 (`buffer`): Serialisasi array baris struktur berformat A-XDR.
    - Attribute 3 (`capture_objects`): Serialisasi array deskriptor kolom class_id, logical_name, attribute_index, data_index.
    - Attribute 4 (`capture_period`): Mengembalikan 900 detik (15 menit) untuk Load Profile dan 0 detik untuk trigger-based profiles (Billing Profile dan Event Log).
    - Attribute 5 (`sort_method`): Mengembalikan 1 (FIFO).
    - Attribute 6 (`sort_object`): Mengembalikan deskriptor Clock capture object.
    - Attribute 7 (`entries_in_use`): Mengembalikan jumlah entri aktif saat ini.
    - Attribute 8 (`profile_entries`): Mengembalikan kapasitas maksimum buffer (8 entri).
- **Dukungan Selective Access pada Profile Generic**:
  - **Selector 1 (Range by DateTime)**: Memfilter baris buffer berdasarkan rentang timestamp (`from_date` sampai `to_date`). Mengembalikan status `AMI_COSEM_SCOPE_VIOLATED` bila urutan tanggal terbalik (`from_date > to_date`).
  - **Selector 2 (Entry by Index)**: Memfilter baris buffer berdasarkan indeks 1-based (`from_entry` sampai `to_entry`). Mengembalikan `AMI_COSEM_SCOPE_VIOLATED` bila `from_entry == 0`, `from_entry > to_entry`, atau `to_entry > count`.
  - Penolakan akses berparameter (selection) pada atribut di luar buffer (attribute 1 atau 3) dengan status `AMI_COSEM_SCOPE_VIOLATED`.
- **Integrasi DLMS Association View (`NonSecure/AMI/Src/ami_dlms_apdu.c`)**:
  - Mendaftarkan Load Profile 15-menit `{7u, 0u, {1, 0, 99, 1, 0, 255}}` pada `association_view[]`.
  - Jumlah objek Association View bertambah menjadi 13 objek (Association, Clock, 3x Tegangan, 3x Arus, 2x Energi Aktif, Event Log, Billing Profile, Load Profile).
- **Penambahan Pengujian Unit & Verifikasi (`tests/test_ami_cosem.c`, `tests/test_ami_dlms_apdu.c`)**:
  - `test_profile_generic_buffers`: Memvalidasi serialisasi 12 elemen Load Profile, 5 elemen Billing Profile, capture period 900s, sort method FIFO, serta Selective Access Selector 1 dan Selector 2 termasuk pengujian skenario pelanggaran batas (scope violation).
  - `test_load_profile_apdu_selective_access`: Memvalidasi pemrosesan APDU xDLMS `GET-Request-Normal` berparameter selective access terhadap objek Load Profile melalui pipeline asosiasi DLMS.
  - Verifikasi CTest: **9/9 tests PASSing (100%)**.
  - Verifikasi Cross-Compilation Firmware ARM Cortex-M33 TrustZone: Berhasil dibangun dengan **0 error dan 0 warning**.

## [v2.8.0] - 2026-11-02

### Added
- **Konfigurasi Hardware Security Attribution Unit (SAU) STM32U585 (`Secure/Core/Inc/partition_stm32u585xx.h`)**:
  - Mengaktifkan SAU secara eksplisit (`SAU_INIT_CTRL_ENABLE = 1`) dengan kebijakan *Secure-by-Default* (`SAU_INIT_CTRL_ALLNS = 0`).
  - Pemetaan partisi alamat memori fisik SAU:
    - Region 0: Non-Secure Callable (NSC) Veneers (`0x0C0FE000` - `0x0C0FFFFF`, `NSC = 1`).
    - Region 1: Non-Secure Flash Bank 2 (`0x08100000` - `0x081FFFFF`, `NSC = 0`).
    - Region 2: Non-Secure SRAM3 (`0x20040000` - `0x200BFFFF`, 512 KB, `NSC = 0`).
    - Region 3: Non-Secure Peripherals (`0x40000000` - `0x4FFFFFFF`, `NSC = 0`).
    - Region 4: Non-Secure External Memory / OctoSPI (`0x60000000` - `0x9FFFFFFF`, `NSC = 0`).
    - Region 5: System Memory Bootloader ROM (`0x0BF90000` - `0x0BFA8FFF`, `NSC = 0`).
  - Penegakan isolasi System Control Block (SCB AIRCR):
    - `SYSRESETREQS = 1`: Reset sistem hanya dapat dieksekusi dari Secure World (mencegah eksploitasi reset dari NonSecure).
    - `PRIS = 1`: Memprioritaskan penanganan exception Secure di atas Non-Secure exception.
    - `BFHFNMINS = 0`: HardFault, NMI, dan BusFault ditangani langsung di domain Secure.
  - Konfigurasi ITNS (Interrupt Target Non-Secure) pada NVIC:
    - Mengisolasi interrupt perangkat keras kritis ke Secure World: `GTZC_IRQn`, `SAES_IRQn`, `TAMP_IRQn`, `RTC_S_IRQn`, `FLASH_S_IRQn`, `RCC_S_IRQn`, `RNG_IRQn`, `HASH_IRQn`, `PKA_IRQn`.
    - Mengarahkan interrupt komunikasi (USART, SPI, EXTI) ke Non-Secure World.
- **Konfigurasi Global TrustZone Controller (GTZC) (`Secure/Core/Src/main.c`)**:
  - Konfigurasi TZSC (TrustZone Security Controller) mengunci peripheral kriptografi ke domain Secure:
    - `GTZC_PERIPH_SAES` (Secure AES Engine)
    - `GTZC_PERIPH_RNG` (Hardware True Random Number Generator)
    - `GTZC_PERIPH_TAMP` (Tamper & Backup Registers)
    - `GTZC_PERIPH_RTC` (Real-Time Clock)
    - `GTZC_PERIPH_PKA` (Public Key Accelerator)
    - `GTZC_PERIPH_HASH` (SHA-256 Hash Accelerator)
  - Penguncian konfigurasi MPCBB3 pada SRAM3 (`HAL_GTZC_MPCBB_Lock(SRAM3_BASE)`) untuk mencegah modifikasi atribut RAM saat runtime.
  - Pengaktifan TrustZone Interrupt Controller (`HAL_GTZC_TZIC_EnableIT`) untuk menangkap upaya akses ilegal (Illegal Access Event) ke peripheral atau memori Secure.
  - Penguncian register konfigurasi TZSC1 (`HAL_GTZC_TZSC_Lock(GTZC_TZSC1)`).
- **Penanganan Exception Hardware SecureFault & GTZC Tamper (`Secure/Core/Src/stm32u5xx_it.c`, `Secure/Core/Inc/stm32u5xx_it.h`)**:
  - Implementasi `SecureFault_Handler()` dengan pembacaan dan pembersihan status register `SCB->SFSR`, pembersihan memori sensitif (*fail-secure zeroization*), pemanggilan callback terdaftar, dan lockdown.
  - Implementasi `GTZC_IRQHandler()` dan `HAL_GTZC_TZIC_Callback()` untuk merespons pelanggaran akses ilegal hardware secara real-time.
- **Pengamanan Gerbang Non-Secure Callable (NSC) (`Secure/Core/Src/secure_nsc.c`)**:
  - Validasi batas memori pointer NonSecure menggunakan ARM CMSE intrinsic `cmse_check_address_range()` pada seluruh API gateway NSC (`output`, `input`, `iv`, `aad`, `tag`). Mencegah serangan *Confused Deputy* jika NonSecure mencoba menyusupkan alamat Secure RAM.
  - Inisialisasi dan clocking hardware True Random Number Generator (RNG) dari register peripheral dengan health check timeout.
  - Pembersihan memori kerja stack secara waktu konstan (`secure_memzero`).
- **Verifikasi**:
  - Kompilasi target firmware Cortex-M33 TrustZone (`cmake --build --preset Debug`) sukses 100% tanpa error dan tanpa warning:
    - Secure Image (`STM32U585VIT6_SM_S.elf`): RAM 11,984 B (4.57%), ROM 12,392 B (1.19%), ROM_NSC 96 B (1.17%).
    - NonSecure Image (`STM32U585VIT6_SM_NS.elf`): RAM 17,224 B (3.29%), ROM 43,568 B (4.15%).
  - CTest host test suite: **9/9 tests PASSing (100%)**.

## [v2.9.0] - 2026-11-02

### Added
- **Persistensi Non-Volatile Memory (NVM) untuk Invocation Counter & Mekanisme Anti-Replay (`NonSecure/AMI/Inc/ami_nvm_counter.h`, `NonSecure/AMI/Src/ami_nvm_counter.c`)**:
  - Rekayasa struktur data `AmiNvmCounterRecord` (24-byte) dengan Magic `0x414D4943` ('AMIC'), `version` (1), `tx_invocation_counter` (lease bound), `rx_invocation_counter` (high-water mark), `reboot_count` (pelacak siklus restart), dan integritas data berbasis CRC-32 (IEEE 802.3).
  - **Penjaminan Keunikan Nonce (*Nonce Uniqueness Guarantee*) per NIST SP 800-38D & IEC 62056-5-3**:
    - Pada setiap proses boot/reboot atau pemulihan brown-out, sistem secara otomatis melompat di atas batas lease sesi sebelumnya (`persisted_tx_limit = old_persisted_limit + tx_window_step`) dan langsung meng-commit record baru ke NVM sebelum APDU pertama ditransmisikan.
    - Menghilangkan secara mutlak risiko tabrakan nonce (*AES-128-GCM nonce collision / nonce reuse*) meskipun smart meter mengalami mati daya mendadak di tengah transmisi data.
  - **Mitigasi Keausan Memori Flash (*Flash Wear Mitigation / Lease Window Allocation*)**:
    - Menerapkan alokasi lease window terkonfigurasi (`tx_window_step`, default 100).
    - TX Invocation Counter bertambah di memori volatile tanpa memicu operasi write Flash pada setiap paket APDU.
    - Penulisan ke NVM hanya dilakukan saat counter runtime menyentuh batas lease yang dialokasikan atau saat pemanggilan eksplisit `ami_nvm_counter_flush()`.
  - **Penegakan Anti-Replay Monotonik Keras (*Strict Anti-Replay Enforcement*)**:
    - Memvalidasi APDU masuk (`in_rx_ic > last_rx_counter`) dan menolak keras frame replay (counter duplikat, counter mundur, atau nilai 0).
    - Menyimpan high-water mark penerimaan secara berkala (`rx_window_step`) dan mempertahankan status tersebut melintasi siklus reboot.
  - **Abstraksi Driver Penyimpanan Fleksibel (*Storage Driver Abstraction*)**:
    - `ami_nvm_create_ram_driver`: Driver berbasis RAM untuk mock test simulasi host dan CI/CD pipeline.
    - `ami_nvm_create_tamp_backup_driver`: Driver perangkat keras STM32 TAMP Backup Registers (`TAMP->BKP0R` s.d. `BKP4R`) berdaya baterai VBAT, beroperasi tanpa batasan siklus keausan Flash (zero wear endurance) dan waktu akses single-cycle.
- **Integrasi DLMS Security Suite 0 (`NonSecure/AMI/Inc/ami_dlms_cipher.h`, `NonSecure/AMI/Src/ami_dlms_cipher.c`)**:
  - Penambahan pointer kontekstual `AmiNvmCounterContext *nvm_counter` pada struct `AmiDlmsSecuritySuite0Context`.
  - Fungsi pengikatan `ami_dlms_cipher_suite0_bind_nvm()`.
  - Callback `suite0_accept_rx_counter_cb` dan `suite0_next_tx_counter_cb` mendelegasikan verifikasi anti-replay dan alokasi nonce TX langsung ke subsystem NVM saat context dihubungkan.
- **Penambahan Pengujian Unit & Verifikasi CTest (`tests/test_ami_nvm_counter.c`, `tests/CMakeLists.txt`)**:
  - `test_nvm_counter_fresh_init`: Memvalidasi inisialisasi pada memori kosong/terkorupsi, format record baru, kalkulasi CRC-32, dan commit initial lease.
  - `test_nvm_counter_monotonic_and_lease_window`: Memvalidasi kenaikan counter TX yang strictly monotonic, nol penulisan storage dalam rentang lease, dan ekspansi lease otomatis saat batas tercapai.
  - `test_nvm_counter_reboot_and_nonce_uniqueness`: Mensimulasikan pemadaman listrik mendadak dan reboot, membuktikan counter TX baru selalu strictly greater dari counter manapun pada sesi sebelumnya.
  - `test_nvm_counter_anti_replay`: Memvalidasi penolakan tegas APDU duplikat, mundur, atau 0, serta threshold commit RX.
  - `test_nvm_counter_reboot_preserves_anti_replay`: Memvalidasi kekebalan anti-replay yang bertahan melintasi reboot.
  - `test_nvm_suite0_cipher_binding`: Memvalidasi integrasi end-to-end antara Suite 0 cipher backend dan subsystem NVM counter.
  - Hasil CTest: **10/10 tests PASSing (100%)**.
  - Hasil Cross-Compilation Firmware ARM Cortex-M33 TrustZone: Berhasil dibangun dengan **0 error dan 0 warning**.
    - NonSecure image (`STM32U585VIT6_SM_NS.elf`): RAM 17,224 B (3.29%), ROM 43,568 B (4.15%).


## [v2.10.0] - 2026-11-02

### Added
- **Provisi Pabrik Master Key (KEK) Unik Per-Device di Secure Storage (`Secure_nsclib/secure_nsc.h`, `Secure/Core/Src/secure_nsc.c`)**:
  - **Penyimpanan Terisolasi Secure World TrustZone**: Struktur data internal `SECURE_KeyStorageTypeDef` (`master_kek`, `guek`, `gbek`, `ak`, `is_provisioned`) diisolasi secara permanen di domain Secure World tanpa adanya getter fungsi eksposur plaintext ke NonSecure World.
  - **Derivasi Master Key Hardware-Unik Otomatis**: Ketika perangkat beroperasi sebelum provisi pabrik eksplisit (`!is_provisioned`), Secure World secara deterministik membaca 96-bit Unique Device ID (UID) STM32U585 dari register `UID_BASE` (`0x0BFA0700`), mendiversifikasikannya dengan `FACTORY_MASTER_SALT`, dan menghasilkan 128-bit Master KEK yang unik per silikon chip meter.
  - **Gateway Provisi Pabrik Terproteksi (`SECURE_ProvisionMasterKey`)**: Provisi Master Key pabrik terlindungi oleh token otentikasi `SECURE_FACTORY_PROVISION_TOKEN` (`0x504C4E31` 'PLN1'). Melakukan validasi keamanan pointer NonSecure menggunakan ARM CMSE (`validate_ns_buffer`), menolak kunci lemah (all-zero), dan menandai status terprovisi secara permanen.
  - **Interface Query Status & Pembacaan UID**: Menyediakan NSC API `SECURE_IsMasterKeyProvisioned()` dan `SECURE_GetDeviceUniqueId()`.
- **Remote Key Transfer RFC 3394 AES Key Wrap Atomik di Secure World (`Secure/Core/Src/secure_nsc.c`, `NonSecure/AMI/Inc/ami_security.h`, `NonSecure/AMI/Src/ami_security.c`, `NonSecure/AMI/Src/ami_dlms_cipher.c`)**:
  - **Engine Kriptografi RFC 3394 / NIST SP 800-38F Key Wrap & Key Unwrap (KW)**:
    - Implementasi pembalikan cipher AES-128 standar (*Inverse Cipher*: `InvSubBytes`, `InvShiftRows`, `InvMixColumns`, `InvAddRoundKey`) pada domain Secure dan NonSecure host test.
    - Verifikasi integritas waktu konstan terhadap Initial Value standar (`0xA6A6A6A6A6A6A6A6`).
    - Lulus 100% verifikasi Known Answer Test (KAT) NIST SP 800-38F Appendix 4.1.
  - **Penegakan Atomisitas Penuh (*Strict Atomic Key Transfer* via `SECURE_DLMS_TransferKeys_Atomic`)**:
    - Menerima batch kunci terbungkus 24-byte (GUEK, GBEK, AK, atau Master KEK baru) dari HES/NonSecure.
    - Setiap kunci di-unwrap ke staging buffer internal Secure World menggunakan KEK perangkat unik.
    - **All-or-Nothing Atomicity**: Jika terdapat satu saja kunci dalam batch yang mengalami kegagalan verifikasi integritas RFC 3394 (data rusak, salah kunci KEK, atau manipulasi bit), seluruh proses transfer dibatalkan seketika (*immediate rollback*), memori staging segera di-zeroize, dan kunci operasional aktif dipertahankan 100% tanpa perubahan.
    - Kunci operasional baru hanya di-commit ke Secure Storage bila SELURUH kunci dalam batch terverifikasi valid.
    - Plaintext kunci rahasia tidak pernah keluar atau terekspos ke domain NonSecure.
- **Pengujian Unit & Verifikasi Holistik (`tests/test_ami_key_transfer.c`, `tests/CMakeLists.txt`)**:
  - `test_rfc3394_kat`: Memverifikasi kesesuaian enkripsi pembungkusan dan pembongkaran terhadap vektor uji resmi NIST SP 800-38F Appendix 4.1.
  - `test_rfc3394_tamper_integrity`: Memvalidasi penolakan mutlak dan kegagalan integritas pada setiap mutasi bit dari 24-byte wrapped key.
  - `test_suite0_atomic_key_transfer_success`: Memvalidasi keberhasilan pembaruan batch GUEK dan AK.
  - `test_suite0_atomic_key_transfer_rollback_on_failure`: Memvalidasi kegagalan salah satu kunci membatalkan pembaruan seluruh batch secara atomik tanpa merusak kunci lama.
  - CTest Host: **11/11 tests PASSing (100%)**.
  - Target Firmware Cortex-M33 TrustZone (`cmake --build --preset Debug`): Berhasil dibangun dengan **0 error dan 0 warning**:
    - Secure Image (`STM32U585VIT6_SM_S.elf`): RAM 12,064 B (4.60%), ROM 17,296 B (1.66%), ROM_NSC 128 B (1.56%).
    - NonSecure Image (`STM32U585VIT6_SM_NS.elf`): RAM 17,224 B (3.29%), ROM 43,568 B (4.15%).


## [v2.11.0] - 2026-11-02

### Remediated & Security Hardened
- **Audit & Remediasi Keamanan Komprehensif Berdasarkan `SECURITY_REVIEW.md`**:
  - Mereview seluruh 6 temuan pada `SECURITY_REVIEW.md` dan mengonfirmasi validitas teknisnya pada arsitektur TrustZone dan DLMS. Seluruh celah kritis telah diperbaiki secara tuntas pada Secure World dan NonSecure layer.
- **Implementasi Penuh Kriptografi NIST SP 800-38D AES-128-GCM & GMAC di Secure World (`Secure/Core/Src/secure_nsc.c`) [Finding 5]**:
  - Mengeliminasi total rutinitas placeholder XOR dan derivation tag dummy di `SECURE_DLMS_AES_GCM_Process()`.
  - Mengimplementasikan block cipher AES-128 standar (10 round, key expansion), perkalian Galois Field $GF(2^{128})$ dengan irreducible polynomial $R = 11100001 \parallel 0^{120}$ (`gf128_mul_sec`), fungsi GHASH (`ghash_sec`), dan increment counter 32-bit big-endian (`inc32_sec`).
  - Pada mode dekripsi (`is_encrypt == 0`), verifikasi tag otentikasi 16-byte dilakukan secara constant-time. Jika tag tidak valid atau terjadi manipulasi payload/AAD, plaintext buffer segera didekontaminasi (`secure_memzero`) dan mengembalikan error fail-secure `-4`.
  - Pada `SECURE_DLMS_HLS5_Verify()`, verifikasi GMAC otentik dihitung menggunakan Authentication Key (AK) internal Secure World, dan mengaktifkan status sesi otentik `s_hls_authenticated_session = 1` saat berhasil.
- **Penguatan Siklus Hidup Otorisasi Manajemen Kunci Secure World [Finding 6]**:
  - **One-Time Provisioning (OTP) Lock**: Pada `SECURE_ProvisionMasterKey()`, jika Master KEK telah diprovisi (`s_secure_keys.is_provisioned != 0`), panggilan ulang dari NonSecure ditolak secara permanen dengan kode kesalahan `-5` (anti-overwrite lock).
  - **Enforce Otentikasi Sesi HLS untuk Remote Key Transfer**: Pada `SECURE_DLMS_TransferKeys_Atomic()`, panggilan unwrap remote key mewajibkan sesi HLS yang terotentikasi (`s_hls_authenticated_session == 1`). NonSecure code tidak dapat memicu transfer kunci tanpa menyelesaikan handshake HLS terlebih dahulu.
- **Entropi Dinamis Hardware TRNG Fallback [Finding 2]**:
  - Menghilangkan seed statis (`0xA5A55A5A`) pada fallback PRNG di `SECURE_TRNG_GetBytes()`. Entropi dicampur secara dinamis dari register hardware Unique ID STM32 (`UID_BASE`) dan register pewaktu sistem (`SysTick->VAL`).
- **Verifikasi & Status Build**:
  - Target Firmware Cortex-M33 TrustZone (`cmake --build build/Debug`): **0 errors, 0 warnings**.
    - Secure Image (`STM32U585VIT6_SM_S.elf`): RAM 12,072 B (4.61%), ROM 20,512 B (1.97%), ROM_NSC 128 B (1.56%).
    - NonSecure Image (`STM32U585VIT6_SM_NS.elf`): RAM 17,224 B (3.29%), ROM 43,568 B (4.15%).
  - CTest Host Suite: **11/11 tests PASS (100%)** dalam 0.29 detik.
  - Memperbarui dokumentasi `SECURITY_REVIEW.md` dengan status **Fixed / Mitigated** untuk seluruh temuan.


## [v2.12.0] - 2026-11-03

### Hardened & Remediated (CIA Triad Findings)
- **Atribusi Keamanan SRAM3 MPCBB TrustZone (`Secure/Core/Src/main.c`) [Finding C1]**:
  - Memperbaiki konfigurasi MPCBB SRAM3 (`0x30000000`). Array `MPCBB_SecConfig_array[0..31]` dan `MPCBB_PrivConfig_array[0..31]` kini dikonfigurasi `0xFFFFFFFF` (Secure & Privileged) dan dikunci melalui `HAL_GTZC_MPCBB_Lock(SRAM3_BASE)`.
  - Mengeliminasi celah di mana data, BSS, dan stack Secure World sebelumnya berstatus NonSecure secara hardware.
- **Eliminasi Kerentanan Double-Fetch / TOCTOU di Boundary NSC (`Secure/Core/Src/secure_nsc.c`) [Finding I4]**:
  - `SECURE_DLMS_AES_GCM_Process()` dan `SECURE_DLMS_TransferKeys_Atomic()` menerapkan *atomic single-fetch copy* struktur parameter ke stack memori Secure sebelum validasi pointer CMSE dilakukan.
- **Fail-Secure Hardware TRNG Error Handling (`Secure/Core/Src/secure_nsc.c`) [Finding I3]**:
  - Jika hardware TRNG mengalami timeout atau terdeteksi flag SECS/CECS pada target STM32, `SECURE_TRNG_GetBytes()` segera me-zeroize output buffer dan mengembalikan error `-3`, menolak mode fail-open.
- **Otorisasi Siklus Hidup Transfer Kunci Terproteksi (`Secure/Core/Src/secure_nsc.c`) [Finding I1]**:
  - `SECURE_DLMS_TransferKeys_Atomic()` menolak transfer jika perangkat belum diprovisi (`is_provisioned == 0`) atau jika sesi HLS belum terverifikasi (`s_hls_authenticated_session == 0`).
  - Menginvalidasi token sesi (`s_hls_authenticated_session = 0`) seketika setelah transfer kunci selesai untuk mencegah session reuse.
- **Proteksi Overflow Invocation Counter & Fail-Closed NVM (`NonSecure/AMI/Src/ami_nvm_counter.c`) [Finding I2]**:
  - Penambahan guard counter 32-bit (`>= 0xFFFFFFFEu`) untuk mencegah nonce exhaustion wrap-around per NIST SP 800-38D.
  - Penegakan fail-closed pada kegagalan penulisan record NVM high-water mark.
- **Integrasi Runtime Task Firmware DLMS (`NonSecure/AMI/Src/ami_dlms_port.c`)**:
  - Mengintegrasikan persistent NVM counter driver dan Suite 0 cipher backend langsung ke dalam runtime task FreeRTOS `AmiDlmsTaskEntry`.


---

# 🏭 SPIN 2 — Pelengkapan Fitur Produksi, Kesiapan Mainboard & Sertifikasi
*(Acuan: 26-Week Work Plan Rev D, Weeks 11–26: Disconnect Control Breaker Class 70, Anti-Brute-Force Lockout SPLN D5.002:2024, General Block Transfer GBT, 49 Objek PLN, Load Profile 23-Kanal, dan Kesiapan Driver Target)*

## [v2.13.0] - 2026-11-03

### Added
- **Anti-Brute-Force: Progressive Exponential Backoff & Lockout per SPLN D5.002:2024 (`NonSecure/AMI/Inc/ami_anti_bruteforce.h`, `NonSecure/AMI/Src/ami_anti_bruteforce.c`)**:
  - **Logika Tracking Kegagalan Otentikasi HLS5 (`failed_attempts`)**:
    - Melacak percobaan gagal berturut-turut pada handshake otentikasi DLMS.
    - Menghitung delay penalti progresif eksponensial ($delay = initial \times multiplier^{attempts - 1}$), dimulai dari 5 detik, 10 detik, 20 detik, 40 detik, hingga batas maksimum 60 detik.
  - **Penegakan Hard Lockout Permanen Setelah 5x Kegagalan**:
    - Sesuai standar SPLN D5.002:2024, setelah mencapai ambang batas 5 kali kegagalan (`max_failed_attempts = 5`), sistem mengunci akses secara permanen (`AMI_ABF_STATE_LOCKED_OUT` / `0xFFFFFFFF`).
    - Selama masa lockout, meter secara fail-closed menolak semua permintaan asosiasi AARQ dan mengembalikan frame penolakan AARE (*Association-Result: rejected-permanent, Diagnostic: authentication-failure*).
  - **Mekanisme Reset & Pemulihan Administratif**:
    - Keberhasilan otentikasi HLS (`ami_anti_bruteforce_record_success`) seketika me-reset counter kegagalan ke 0.
    - Tersedia fungsi pembukaan kunci administratif (`ami_anti_bruteforce_unlock`).
- **Integrasi DLMS Port & Association LN Method 1 (`NonSecure/AMI/Inc/ami_dlms_port.h`, `NonSecure/AMI/Src/ami_dlms_port.c`)**:
  - Penambahan pointer `AmiAntiBruteforceContext` dan pelacak waktu pada `AmiDlmsPortContext`.
  - Penolakan AARQ otomatis saat lockout aktif dengan encoding AARE rejection standar Green Book (`encode_aare_lockout_rejection`).
  - Pemrosesan Action-Request (Class 15 Association LN, OBIS `0.0.40.0.0.255`, Method 1 `reply_to_HLS_authentication`), menghubungkan langsung verifikasi bukti HLS (`ami_dlms_process_hls_reply`) ke pencatatan kegagalan/keberhasilan Anti-Brute-Force.
- **Pengujian Unit & Verifikasi Host (`tests/test_ami_anti_bruteforce.c`, `tests/CMakeLists.txt`)**:
  - `test_abf_default_initialization`: Memvalidasi inisialisasi default parameter SPLN.
  - `test_abf_progressive_exponential_backoff`: Memvalidasi kenaikan eksponensial delay penalti (5s, 10s, 20s, 40s) hingga hard lockout pada kegagalan ke-5.
  - `test_abf_administrative_unlock`: Memvalidasi pemulihan status dari lockout permanen.
  - `test_abf_success_resets_backoff`: Memvalidasi reset counter kegagalan saat otentikasi berhasil.
  - `test_abf_dlms_port_aarq_lockout_rejection`: Memvalidasi penolakan AARQ end-to-end melalui wire frame HDLC port.
  - `test_abf_dlms_port_action_hls_reply`: Memvalidasi integrasi Action-Request Association LN Method 1 dan transisi status penalti/keberhasilan HLS.
  - Hasil CTest: **12/12 tests PASS (100% Lulus)**.

## [v2.14.0] - 2026-11-03

### Added
- **Protokol Lanjutan: DLMS/COSEM xDLMS Block Transfer & General Block Transfer (GBT) (`NonSecure/AMI/Inc/ami_dlms_block_transfer.h`, `NonSecure/AMI/Src/ami_dlms_block_transfer.c`)**:
  - **Mekanisme Transfer Blok Data Sesuai IEC 62056-5-3 / DLMS Green Book**:
    - Mendukung transfer data berukuran besar (Load Profile 15-menit, Event Log, Billing Profile, dan Association View Object List) yang melebihi kapasitas Max PDU / batas frame HDLC (512/1024 bytes).
    - Parsing request blok lanjutan: `GET-Request-For-Next-Datablock` (`0xC0 0x02 <invoke-id> <block-number: 4B BE>`).
    - Pembangkitan respons blok: `GET-Response-With-Datablock` (`0xC4 0x02 <invoke-id> <last-block: 1B> <block-number: 4B BE> <result-choice: 0x00 raw-data> <A-XDR length> <payload chunk>`).
    - Pembangkitan respons error blok: `DataBlock-Result: data-access-result` (`0x01` choice, misal `temporary-failure = 2`).
    - Helper pengkodean A-XDR length berstandar (`ami_bt_encode_axdr_length`).
  - **State Machine Manajemen Buffer & Chunking (`AmiBlockTransferContext`)**:
    - Cache respons data berukuran hingga 2048 bytes (`AMI_BT_MAX_CACHE_BUFFER_SIZE`).
    - Konfigurasi ukuran potongan blok (`block_size`, default 128 bytes, clamped 32..512 bytes).
    - State tracking: `IDLE`, `TRANSFERRING`, `COMPLETED`, `ERROR`.
    - Dukungan retransmisi blok identik jika client meminta ulang (`requested_block == current_block`).
    - Deteksi kesalahan urutan (*out-of-sequence request*) dan pelaporan error yang sesuai.
- **Integrasi DLMS Port & Otomatisasi Transisi Block Transfer (`NonSecure/AMI/Inc/ami_dlms_port.h`, `NonSecure/AMI/Src/ami_dlms_port.c`)**:
  - Penambahan context `block_transfer` dan parameter `block_transfer_threshold` pada `AmiDlmsPortContext`.
  - Fungsi konfigurasi: `ami_dlms_port_configure_block_transfer(ctx, block_size, threshold)`.
  - Pada pemrosesan `dispatch_i_frame()`:
    - Jika menerima `0xC0 0x02` (`GET-Request-For-Next-Datablock`), port memanggil engine block transfer untuk mengirim blok data berikutnya atau merespons error jika out of order.
    - Jika menerima `0xC0 0x01` (`GET-Request-Normal`) dan ukuran respons melebihi ambang batas (`block_transfer_threshold`), port secara otomatis memulai sesi block transfer (`ami_bt_start_get_transfer`) dan mengembalikan blok 1 dalam format `GET-Response-With-Datablock` (`0xC4 0x02`).
    - Jika asosiasi dilepas (`RLRQ` 0x62), state machine block transfer secara otomatis di-reset (`ami_bt_reset`).
- **Integrasi Gurux DLMS Simulator (`tests/gurux_simulator.c`)**:
  - Mengintegrasikan `AmiBlockTransferContext s_sim_bt` pada simulator TCP host (port 4059).
  - Simulator secara otomatis memecah respons yang melebihi 128 byte menjadi blok-blok datablock dan melayani `GET-Request-For-Next-Datablock` dari Gurux DLMS Director.
- **Pengujian Unit & Verifikasi Host (`tests/test_ami_block_transfer.c`, `tests/CMakeLists.txt`)**:
  - `test_bt_init_and_reset`: Memvalidasi inisialisasi, clamping min/max ukuran blok, dan reset state machine.
  - `test_bt_start_transfer_and_chunking`: Memvalidasi pemotongan payload 350 byte menjadi 3 blok berurutan (128B + 128B + 94B) dengan flag `last_block == 1` pada blok ke-3, serta verifikasi reassembly payload 100% bit-for-bit identik.
  - `test_bt_retransmission_and_errors`: Memvalidasi pengiriman ulang blok saat ini, penolakan permintaan blok yang melompati urutan (*out-of-sequence*), dan pembangkitan respons error DataBlock.
  - `test_bt_parse_get_next_block_request`: Memvalidasi parsing request `0xC0 0x02`.
  - `test_bt_port_integration`: Memvalidasi transisi otomatis port dari pembacaan buffer besar (Association Object List) menjadi stream `GET-Response-With-Datablock` (Block 1) dan pemrosesan `GET-Request-For-Next-Datablock` (Block 2) melalui wire frame HDLC lengkap.
  - Hasil CTest: **13/13 tests PASS (100% Lulus)** dalam 0.30 detik.


## [v2.15.0] - 2026-11-03

### Added
- **Data Model Lanjutan: Ekspansi Objek Association View (Katalog OBIS PLN) (`NonSecure/AMI/Inc/ami_cosem_model.h`, `NonSecure/AMI/Src/ami_cosem_model.c`, `NonSecure/AMI/Src/ami_dlms_apdu.c`)**:
  - **Ekspansi Katalog Association View dari 13 Menjadi 49 Objek Standar PLN**:
    - Sesuai standar SPLN D3.002:2021 dan IEC 62056-6-2 untuk Smart Meter 3-Fasa Pascabayar.
    - **Manajemen & Device Info (Class 1 & 15)**: Association LN (`0.0.40.0.0.255`), Clock (`0.0.1.0.0.255`), Logical Device Name (`0.0.42.0.0.255`), Firmware Version (`1.0.0.2.0.255`), Meter ID/Serial Number (`0.0.96.1.0.255`), Meter Type (`0.0.96.1.1.255`), Security Setup (`0.0.43.0.0.255`).
    - **Profil Riwayat & Log (Class 7)**: Event Log Profile (`0.0.99.98.0.255`), Monthly Billing Profile (`1.0.98.1.0.255`), Load Profile 15-menit (`1.0.99.1.0.255`).
    - **Tegangan & Arus Sesaat Tiap Fasa (Class 3)**: Voltage L1/L2/L3 (`1.0.32/52/72.7.0.255`), Current L1/L2/L3 (`1.0.31/51/71.7.0.255`).
    - **Energi Kumulatif Total (Class 3)**: Active Import (+A, `1.0.1.8.0.255`), Active Export (-A, `1.0.2.8.0.255`), Reactive Import (+R, `1.0.3.8.0.255`), Reactive Export (-R, `1.0.4.8.0.255`).
    - **Multi-Tarif Register T1 s.d. T4 (Class 3)**:
      - Active Energy Import Tarif 1..4 (LWBP 1, WBP, LWBP 2, T4): `1.0.1.8.1..4.255`.
      - Active Energy Export Tarif 1..4: `1.0.2.8.1..4.255`.
      - Reactive Energy Import Tarif 1..4: `1.0.3.8.1..4.255`.
    - **Power Quality (Class 3)**:
      - Frekuensi Jaringan: `1.0.14.7.0.255` (satuan Hz, scaler $10^{-3}$).
      - Power Factor Total & Tiap Fasa L1/L2/L3: `1.0.13/33/53/73.7.0.255` (scaler $10^{-3}$, dimensionless).
      - Total Harmonic Distortion Tegangan (THD V Total, L1, L2, L3): `1.0.12/32/52/72.7.124.255` (scaler $10^{-1}$, %).
      - Total Harmonic Distortion Arus (THD I Total, L1, L2, L3): `1.0.11/31/51/71.7.124.255` (scaler $10^{-1}$, %).
    - **Daya Sesaat (Class 3)**: Active Power Total (+P, `1.0.1.7.0.255`), Reactive Power Total (+Q, `1.0.3.7.0.255`).
    - **Parameter Batas Daya (Demand Limiter Class 71)**:
      - OBIS `0.0.17.0.3.255`, Atribut 1 s.d. 10 lengkap (Monitored Value mengarah ke Active Power `1.0.1.7.0.255`, threshold active/normal, min duration over/under, emergency profile, action script linkage).
    - **Kendali Pemutusan Beban (Disconnect Control Class 70)**:
      - OBIS `0.0.96.3.10.255`, Atribut 1 s.d. 4 lengkap (Output State, Control State, Control Mode).
  - **Ekspansi Cosem Snapshot & Parser (`ami_cosem_model.h`, `ami_cosem_model.c`)**:
    - Penambahan definisi Class ID: `AMI_COSEM_DATA_CLASS_ID` (1u), `AMI_COSEM_DISCONNECT_CONTROL_CLASS_ID` (70u), `AMI_COSEM_LIMITER_CLASS_ID` (71u).
    - Penambahan valid mask flags: `AMI_COSEM_VALID_TARIFF_REGISTERS`, `AMI_COSEM_VALID_POWER_QUALITY`, `AMI_COSEM_VALID_LIMITER`, `AMI_COSEM_VALID_DISCONNECT_CTRL`, `AMI_COSEM_VALID_DEVICE_INFO`.
    - Pemetaan array snapshot untuk multi-tarif `active_import_tariff_wh[4]`, `active_export_tariff_wh[4]`, `reactive_import_tariff_varh[4]`, power quality `frequency_mhz`, `power_factor[4]`, `thd_voltage_percent_x10[4]`, `thd_current_percent_x10[4]`, parameter limiter, disconnect control state, dan device info string.
    - Pembacaan attribute komprehensif untuk register, extended register, data, limiter, dan disconnect control.
  - **Dukungan Buffer Transmisi Skala Penuh & Block Transfer Segmentation**:
    - Association View Object List (49 objek) menghasilkan APDU sebesar 1427 bytes.
    - Penyesuaian buffer penampung sementara pada `ami_dlms_port.c` dan `gurux_simulator.c` menggunakan `AMI_BT_MAX_CACHE_BUFFER_SIZE` (2048 bytes), sehingga payload besar secara otomatis dan mulus disegmentasi oleh engine xDLMS DataBlock Transfer (`[v2.14.0]`).
  - **Simulasi Gurux DLMS & Pengujian Unit Host (`tests/test_ami_cosem.c`, `tests/test_ami_dlms_apdu.c`, `tests/gurux_simulator.c`)**:
    - Penambahan pengujian unit komprehensif untuk multi-tarif, power quality, limiter, disconnect control, dan keterbacaan 49 objek association view.
    - Snapshot development simulator diisi penuh dengan nilai-nilai realistik untuk pengetesan interaktif menggunakan Gurux DLMSDirector.
    - CTest Suite: **13/13 tests PASS (100% Lulus)**.


## [v2.16.0] - 2026-11-03

### Added
- **Kontrol Beban: Logika State Machine Disconnect Control Breaker (Class 70) (`NonSecure/AMI/Inc/ami_disconnect_control.h`, `NonSecure/AMI/Src/ami_disconnect_control.c`)**:
  - **State Machine Deterministik Sesuai IEC 62056-6-2 / DLMS Blue Book & SPLN D3.002:2021**:
    - **Control States (Atribut 3)**: `Disconnected (0)`, `Connected (1)`, `Ready_for_Reconnection (2)`.
    - **Output State (Atribut 2)**: `0` (kontak relay terbuka / beban terputus), `1` (kontak relay tertutup / beban tersambung).
    - **Control Modes (Atribut 4)**: Dukungan Mode 1 s.d. 6. Default ke **Mode 2** (*Direct Remote Reconnect* untuk AMI PLN), dan mendukung konfigurasi ke **Mode 1** (*Manual Consumer Confirmation*).
    - **Method 1 (`remote_disconnect`)**:
      - Memeriksa izin mode (Mode 3 & 4 menolak remote disconnect).
      - Memicu perintah aktuasi `AMI_BREAKER_CMD_OPEN`.
      - Mengubah status ke `Disconnected (0)` dan `output_state = 0`.
    - **Method 2 (`remote_reconnect`)**:
      - Memeriksa izin mode (Mode 3 menolak remote reconnect).
      - Pada Mode 1 & 5: Melakukan transisi ke `Ready_for_Reconnection (2)`, kontak tetap terbuka (`output_state = 0`) menunggu konfirmasi tombol manual.
      - Pada Mode 2, 4, 6: Melakukan transisi langsung ke `Connected (1)`, memicu perintah aktuasi `AMI_BREAKER_CMD_CLOSE`, dan mengubah `output_state = 1`.
    - **Trigger Manual Push Button (`ami_disconnect_control_manual_reconnect`)**:
      - Mensimulasikan/menangani penekanan tombol fisik pada cover meter.
      - Pada Mode 1 & 5 saat berada di state `Ready_for_Reconnection`, menutup relay (`CMD_CLOSE`) dan bertransisi ke `Connected (1)`.
  - **Hardware Abstraction Layer (HAL Breaker Actuator)**:
    - Antarmuka callback aktuasi koil relay bistable latching (`AmiBreakerActuatorFn`).
    - Driver hardware referensi `ami_disconnect_hal_stm32_actuate()` untuk pulse GPIO 100ms (SET/RESET) pada target STM32U585.
    - Penanganan fail-closed: Jika aktuator fisik melaporkan error/timeout, status meter tidak berubah dan mengembalikan `AMI_DISCONNECT_ERR_ACTUATOR_FAILED`.
- **Integrasi DLMS Port & APDU Action-Request (`NonSecure/AMI/Inc/ami_dlms_port.h`, `NonSecure/AMI/Src/ami_dlms_port.c`, `NonSecure/AMI/Src/ami_dlms_apdu.c`)**:
  - Penambahan pointer `disconnect_control` pada `AmiDlmsPortContext` beserta fungsi `ami_dlms_port_set_disconnect_control()`.
  - Pemrosesan Action-Request plain (`0xC3`) pada Class 70 (`0.0.96.3.10.255`), memicu method remote disconnect / remote reconnect, mengembalikan `Action-Response-Normal` (`0xC7 0x01 <invoke_id> 0x00 0x00`), dan langsung menyinkronkan feeder snapshot.
  - Pemrosesan Ciphered Action-Request (`GLO-ACTION-REQUEST`, `0xCB`) terenkripsi/terotentikasi penuh dengan AES-GCM Suite 0, mengembalikan respons terenkripsi `GLO-ACTION-RESPONSE` (`0xCE`).
- **Integrasi Gurux DLMS Simulator (`tests/gurux_simulator.c`)**:
  - Simulator TCP host menginstansiasi `s_sim_dc` dengan mock relay driver.
  - Gurux DLMSDirector dapat mengirim perintah Action Disconnect & Reconnect langsung via TCP port 4059 dan melihat status breaker berubah secara real time.
- **Pengujian Unit & Verifikasi Host (`tests/test_ami_disconnect_control.c`, `tests/CMakeLists.txt`)**:
  - `test_dc_init_defaults`: Memvalidasi status awal power-on (`Connected`, `Output=1`, `Mode=2`).
  - `test_dc_mode_2_remote_disconnect_reconnect`: Memvalidasi siklus pemutusan dan penyambungan langsung Mode 2.
  - `test_dc_mode_1_manual_reconnect`: Memvalidasi transisi Mode 1 (`Disconnected` -> `Ready_for_Reconnection` -> `Connected` via manual button).
  - `test_dc_mode_3_remote_prohibition`: Memvalidasi penolakan remote disconnect/reconnect pada Mode 3.
  - `test_dc_actuator_hardware_failure`: Memvalidasi fail-closed saat aktuator relay mengalami kegagalan.
  - `test_dc_dlms_port_wire_action_request`: Memvalidasi framing HDLC Action-Request Method 1 dan Method 2 end-to-end via DLMS port.
  - `test_dc_ciphered_action_request`: Memvalidasi eksekusi Action-Request terenkripsi AES-GCM HLS.
  - Hasil CTest: **14/14 tests PASS (100% Lulus)**.

## [v2.17.0] - 2026-11-04

### Changed & Fixed
- **Remediasi Kesiapan Produksi Hardware I/O, Metrologi, dan Kriptografi**:
  - **Aktivasi Driver Hardware HAL (`NonSecure/Core/Inc/stm32u5xx_hal_conf.h`)**:
    - Mengaktifkan `#define HAL_UART_MODULE_ENABLED`, `#define HAL_USART_MODULE_ENABLED`, dan `#define HAL_SPI_MODULE_ENABLED`.
  - **Driver Komunikasi UART/DMA Non-Blocking (`NonSecure/AMI/Inc/ami_bsp_uart.h`, `NonSecure/AMI/Src/ami_bsp_uart.c`)**:
    - Implementasi circular DMA receive buffer 512 byte dengan deteksi IDLE line interrupt (`HAL_UARTEx_ReceiveToIdle_DMA`).
    - Integrasi interrupt vector di `NonSecure/Core/Src/stm32u5xx_it.c` melalui `HAL_UARTEx_RxEventCallback()` langsung ke FreeRTOS StreamBuffer.
  - **Kalkulasi Proof HLS 5 Otentik (`Secure/Core/Src/secure_nsc.c`, `Secure_nsclib/secure_nsc.h`, `NonSecure/AMI/Src/ami_dlms_port.c`)**:
    - Penambahan fungsi Secure Gateway `SECURE_DLMS_HLS5_CalculateProof()` yang menghitung 16-byte GMAC Server Proof tag secara kriptografis riil.
    - Perbaikan `ami_dlms_port.c` untuk secara eksplisit mendaftarkan cipher backend (`ami_dlms_port_set_cipher_backend()`).
  - **Key Management & TRNG Fail-Closed (`Secure/Core/Src/secure_nsc.c`)**:
    - Menghapus algoritma pseudo-random fallback LCG; kegagalan hardware TRNG (CECS/SECS/timeout) langsung menghentikan operasi secara fail-closed (return code `-2`).
    - Menghapus array statis kunci default publik (`DEFAULT_GUEK`, `DEFAULT_GBEK`, `DEFAULT_AK`); perangkat yang belum diprovisi akan menolak akses enkripsi dengan `SECURE_ERROR_UNPROVISIONED` (-8).
    - Menghapus token provisi statis publik dari header `secure_nsc.h`.
  - **Keandalan Metrologi ADE9000 (`NonSecure/AMI/Src/ami_ade9000.c`, `tests/test_ami_ade9000.c`)**:
    - Seluruh pembacaan register SPI kini memeriksa nilai balik (`return code`) secara ketat tanpa abaian `(void)`.
    - Penambahan flag `is_valid` dan `last_error_code`. Jika terjadi kesalahan transfer SPI, `ami_ade9000_feed_cosem_snapshot` menolak mengisi data (`valid_mask = 0`) untuk mencegah pembacaan data kotor.
  - **Verifikasi Pra-Jump Secure Boot (`Secure/Core/Src/main.c`, `tests/test_ami_firmware_auth_policy.c`)**:
    - Mengimplementasikan `SECURE_VerifyNonSecureImage()` yang memvalidasi Magic Header `0x504C4E21` ('PLN!'), ukuran image, batas stack MSP_NS di RAM NonSecure, alignment pointer ResetHandler dengan bit Thumb aktif, serta pencegahan downgrade versi anti-rollback counter.

## [v2.18.0] - 2026-11-04

### Added
- **Pemodelan Buffer Load Profile 23-Kanal 15-Menit & Tamper Event Logbook (Class 7)**:
  - **Load Profile 23-Kanal 15-Menit (`1.0.99.1.0.255`, Class 7) (`NonSecure/AMI/Inc/ami_cosem_model.h`, `NonSecure/AMI/Src/ami_cosem_model.c`)**:
    - Sesuai standar SPLN D3.006-2:2021 dan PRD Smart Meter 3-Fasa Pascabayar.
    - **23 Capture Objects Lengkap**:
      1. Clock (`0.0.1.0.0.255`, Attr 2)
      2. Status Register (`0.0.96.5.0.255`, Attr 2)
      3. Energi Aktif Impor +A (`1.0.1.8.0.255`, Attr 2)
      4. Energi Aktif Ekspor -A (`1.0.2.8.0.255`, Attr 2)
      5. Energi Reaktif Impor +R (`1.0.3.8.0.255`, Attr 2)
      6. Energi Reaktif Ekspor -R (`1.0.4.8.0.255`, Attr 2)
      7. Energi Kuadran I Q1 (`1.0.5.8.0.255`, Attr 2)
      8. Energi Kuadran II Q2 (`1.0.6.8.0.255`, Attr 2)
      9. Energi Kuadran III Q3 (`1.0.7.8.0.255`, Attr 2)
      10. Energi Kuadran IV Q4 (`1.0.8.8.0.255`, Attr 2)
      11. Tegangan Fasa L1 (`1.0.32.7.0.255`, Attr 2)
      12. Tegangan Fasa L2 (`1.0.52.7.0.255`, Attr 2)
      13. Tegangan Fasa L3 (`1.0.72.7.0.255`, Attr 2)
      14. Arus Fasa L1 (`1.0.31.7.0.255`, Attr 2)
      15. Arus Fasa L2 (`1.0.51.7.0.255`, Attr 2)
      16. Arus Fasa L3 (`1.0.71.7.0.255`, Attr 2)
      17. Arus Netral In (`1.0.91.7.0.255`, Attr 2)
      18. Daya Aktif Sesaat L1 (`1.0.21.7.0.255`, Attr 2)
      19. Daya Aktif Sesaat L2 (`1.0.41.7.0.255`, Attr 2)
      20. Daya Aktif Sesaat L3 (`1.0.61.7.0.255`, Attr 2)
      21. Power Factor Total (`1.0.13.7.0.255`, Attr 2)
      22. Frekuensi Jaringan (`1.0.14.7.0.255`, Attr 2)
      23. THD Tegangan L1 (`1.0.32.7.124.255`, Attr 2)
    - Peningkatan kapasitas buffer A-XDR encoding (`AMI_COSEM_AXDR_ENCODED_MAX`) dari 1024 menjadi 2048 bytes.
    - Struktur entry load profile `AmiCosemLoadProfileEntry` diperluas menampung 23 kanal secara native.
  - **Tamper Event Logbook (Class 7) (`NonSecure/AMI/Inc/ami_cosem_model.h`, `NonSecure/AMI/Src/ami_cosem_model.c`, `NonSecure/AMI/Src/ami_ade9000.c`)**:
    - **3 Capture Objects Standar**: Clock (`0.0.1.0.0.255`), Event Code (`0.0.96.11.0.255`), Status Register (`0.0.96.5.0.255`).
    - Dukungan profil umum (`0.0.99.98.0.255` & `0.0.99.98.1.255`) dan OBIS spesifik per modus tamper (`1.97.99.98.20..28.255`).
    - Pemetaan 9 Modus Tamper Resmi PLN:
      - Event 20: Cover Open (`1.97.99.98.20.255`)
      - Event 21: Terminal Cover Open (`1.97.99.98.21.255`)
      - Event 22: Phase Sequence Reverse (`1.97.99.98.22.255`)
      - Event 23: Neutral Disturbance (`1.97.99.98.23.255`)
      - Event 24: Magnetic Field Disturbance (`1.97.99.98.24.255`)
      - Event 25: Loss of Phase (`1.97.99.98.25.255`)
      - Event 26: Neutral Current Bypass (`1.97.99.98.26.255`)
      - Event 27: Reverse Power Flow (`1.97.99.98.27.255`)
      - Event 28: Over Voltage / Current Tamper (`1.97.99.98.28.255`)
    - Integrasi otomatis pemicu tamper metrologi (Reverse Power, Phase Sequence Reverse, Loss of Phase) dari driver ADE9000 ke event log.
    - Selective access dan perhitungan atribut 7 (`entries_in_use`) terfilter secara dinamis per OBIS spesifik tamper.
  - **Pengujian Unit & Verifikasi Host (`tests/test_ami_cosem.c`, `tests/test_ami_dlms_apdu.c`)**:
    - Verifikasi pembacaan 23 capture objects load profile dan 3 capture objects event logbook.
    - Verifikasi selective access range descriptor dan specific tamper filter query.
    - CTest Suite: **14/14 tests PASS (100% Lulus)**.

## [v2.18.1] - 2026-11-04

### Changed & Fixed
- **Anti-Brute-Force & Lockout Resilience (SIGFPE Remediation)**:
  - **Sanitasi Aritmetika Pembagian (`NonSecure/AMI/Src/ami_anti_bruteforce.c`)**:
    - Memperbaiki potensi integer division by zero (`SIGFPE`) pada runtime x86_64 Alpine Linux saat dieksekusi di bawah flag compiler `-O3 -DNDEBUG`.
    - Menambahkan batas bawah defensif untuk `backoff_multiplier >= 2u` dan `initial_delay_sec > 0u`.
    - Mengamankan ceiling delay eksponensial (`safe_ceiling = max_delay / multiplier`) sebelum kalkulasi delay penalti, mencegah pembagian dengan nilai nol pada konfigurasi default atau uninitialized.
  - **Unit Testing Resilience (`tests/test_ami_anti_bruteforce.c`)**:
    - Menambahkan test case `test_abf_zero_config_resilience()` guna memverifikasi ketahanan modul saat menerima parameter konfigurasi serba-nol.
- **Docker Environment & Pipeline**:
  - **Penyelarasan Runner Dockerfile (`Dockerfile`)**:
    - Menambahkan paket `cmake` pada image runtime container agar executable `ctest` tersedia saat menjalankan `run_all_docker_tests.sh`.
    - Verifikasi hasil CTest di dalam container Docker: **14/14 suites PASS (100% Lulus)** serta pengujian integrasi E2E simulator DLMS Gurux lulus 100%.
- **Pembersihan Repositori GitHub**:
  - Menghapus folder template `.github/ISSUE_TEMPLATE` (`01_feature_request_pln.yml`, `02_security_or_bug.yml`, `03_hardware_integration.yml`, `config.yml`) dan template pull request `.github/PULL_REQUEST_TEMPLATE.md` sesuai kebutuhan proyek.

## [v2.19.0] - 2026-11-06

### Added & Enhanced
- **Implementasi Profil Kualitas Daya Tegangan R/S/T (Power Quality Voltage Log Profile) (`1.0.99.14.2..4.255`)**:
  - Menyelesaikan masalah *"Capture objects not read."* pada Gurux DLMS Director.
  - Mengimplementasikan objek Profile Generic (Class 7) untuk pemantauan kualitas tegangan per-fasa:
    - Fase R: `1.0.99.14.2.255` (Capture: Clock `0.0.1.0.0.255`, Event Code `0.0.96.11.0.255`, Tegangan R `1.0.32.7.0.255`).
    - Fase S: `1.0.99.14.3.255` (Capture: Clock `0.0.1.0.0.255`, Event Code `0.0.96.11.0.255`, Tegangan S `1.0.52.7.0.255`).
    - Fase T: `1.0.99.14.4.255` (Capture: Clock `0.0.1.0.0.255`, Event Code `0.0.96.11.0.255`, Tegangan T `1.0.72.7.0.255`).
  - Mendaftarkan ketiga profil dan register statusnya (`1.135.32.0.0.255`, `1.135.52.0.0.255`, `1.135.72.0.0.255`) ke `association_view` pada `NonSecure/AMI/Src/ami_dlms_apdu.c`.
- **Implementasi Instant Profile Class 7 (`1.96.98.128.0.255`)**:
  - Mengimplementasikan 11 Capture Objects: Clock, Tegangan sesaat 3-fasa (L1, L2, L3), Arus sesaat 3-fasa (L1, L2, L3), Daya Aktif Total, Daya Reaktif Total, Power Factor Total, dan Frekuensi Jaringan.
  - Serialisasi baris data dinamis pada `read_profile()` dengan proteksi range filtering.
- **Dukungan Interface Class COSEM Baru**:
  - **Class 6 (Register Activation)**: Objek `0.0.14.0.0.255` (Tariff register activation).
  - **Class 11 (Special Days Table)**: Objek `0.0.11.0.0.255` (Special days table).
  - **Class 20 (Activity Calendar)**: Objek `0.0.13.0.0.255` (Activity calendar).
  - **Class 22 (Single Action Schedule)**: Objek `0.1.15.0.4.255` (Historical scheduler), `0.0.15.0.2.255` (Billing scheduler), `0.1.15.0.2.255`, dan `0.2.15.0.2.255`.
- **Dukungan Extended Registers & Generic Data Fallback**:
  - Register batas kualitas daya: Low voltage limit (`1.0.12.31.0.255`), Low voltage delay (`1.0.12.33.0.255`), Faktor daya batas kompensasi (`1.0.13.31.0.255`), dan Energi aktif total (`1.0.15.8.0.255`).
  - Penambahan generic fallback pada model untuk Class 1 (Data) guna mencegah status `OBJECT_NOT_FOUND` pada objek metadata.

### Changed & Fixed
- **Pemisahan Kolom Event Log #1 vs Tamper Log (`0.0.99.98.0.255`)**:
  - Menyelesaikan error *"The number of columns does not match"* pada Gurux DLMS Director saat membaca buffer `0.0.99.98.0.255`.
  - Memisahkan definisi capture objects:
    - **Event Log #1 Standar (`0.0.99.98.0.255`)**: Dikonfigurasi dengan **2 kolom** (Clock `0.0.1.0.0.255` & Event Code `0.0.96.11.0.255`) sesuai standar IEC 62056 dan template Gurux GXC.
    - **Tamper/Alarm Logbook (`1.97.99.98.x.255`)**: Tetap menggunakan struktur **3 kolom** (Clock, Event Code, Status Register).
  - Memperbarui serialisasi baris buffer `read_profile()` pada `NonSecure/AMI/Src/ami_cosem_model.c` agar baris `0.0.99.98.0.255` menghasilkan struktur 2 elemen.
- **Penyelarasan Jam Simulator & Filter Range**:
  - Mengupdate waktu RTC internal simulator pada `tests/gurux_simulator.c` ke waktu aktif pengujian (`2026-11-06 10:10:00`) agar lolos evaluasi rentang filter tanggal (*Selective Access Range*).
  - Menghindari eliminasi record instan profile dan event log pada query selective range.
- **Perbaikan HDLC Framing pada Test Client E2E (`docker_test/test_dlms_e2e_client.py`)**:
  - Memperbaiki parser frame streaming `read_hdlc_frame()` dalam menangani flag transparan `0x7E` ganda (*back-to-back flags*) antar-frame HDLC.
  - Menambahkan pengujian integrasi E2E otomatis untuk Instant Profile (Capture Objects & Buffer), Event Log #1 (verifikasi 2 kolom), dan PQ Voltage Log Profile R.
- **Verifikasi Unit Test**:
  - Memperbarui assert pada `tests/test_ami_cosem.c` untuk 2 kolom pada `obis_event_log` serta menambahkan pengujian `obis_pq_voltage_r`.
  - Hasil CTest Suite: **14/14 test suites lulus 100%**.

## [v2.20.0] - 2026-11-09

### Added
- **Pilar 1: Integrasi Metrik Code Coverage (`ENABLE_COVERAGE`) & CI Workflow**:
  - Menambahkan opsi konfigurasi `-DENABLE_COVERAGE=ON` pada `tests/CMakeLists.txt` untuk instrumen gcov/lcov (`--coverage -O0 -g`).
  - Menambahkan step penarikan dan pelaporan coverage otomatis via `lcov` pada `.github/workflows/ci.yml`.
  - Berdasarkan pengukuran `lcov`, modul inti kriptografi dan protokol keamanan mencapai cakupan tinggi:
    - `ami_security.c`: **91.9% Line Coverage** (431/469 lines) & **93.9% Function Coverage**.
    - `ami_dlms_hdlc.c`: **91.4% Line Coverage** (235/257 lines) & **100% Function Coverage**.
    - `ami_dlms_cipher.c`: **89.8% Line Coverage** (150/167 lines) & **100% Function Coverage**.
    - Total firmware source line coverage: **79.2%** (2.600 dari 3.281 baris).
- **Pilar 2: Penguatan Negative Testing & Tamper Invalidation**:
  - Menambahkan `test_gmac_exhaustive_bit_flip_tampering` pada `tests/test_ami_security.c`:
    - Pengujian exhaustive 1-bit flip pada setiap byte dari 16-byte GMAC tag.
    - Pengujian modifikasi bit pada Additional Authenticated Data (AAD) dan Nonce/IV.
    - Memverifikasi penolakan seketika (`AMI_SEC_ERR_AUTH_FAILED`) dan perbandingan waktu konstan (*constant-time*).
- **Pilar 3: Pengerasan Volatile Zeroization terhadap Compiler Optimization**:
  - Menyematkan memory barrier inline assembly `__asm__ __volatile__("" : : "r"(ptr) : "memory")` pada fungsi `secure_zeroize()` di `NonSecure/AMI/Src/ami_security.c` dan `NonSecure/AMI/Src/ami_dlms_apdu.c`.
  - Menyematkan memory barrier yang sama pada `secure_memzero()` di `Secure/Core/Src/secure_nsc.c`.
  - Mencegah *dead-store elimination* oleh compiler GCC/Clang pada tingkat optimasi `-O2` dan `-O3`.
- **Pilar 4: Validasi Batas Keamanan TrustZone CMSE**:
  - Mengimplementasikan test suite baru `tests/test_ami_trustzone_boundary.c` (`AmiTrustZoneBoundaryTest`) sebagai test suite CTest ke-15.
  - Memvalidasi logika `validate_ns_buffer` terhadap: pointer NULL, integer overflow pointer (`uptr + size < uptr`), buffer yang melintasi perbatasan memori Secure World, dan penolakan izin tulis ke region flash NonSecure.
  - Memperkuat implementasi `validate_ns_buffer()` pada `Secure/Core/Src/secure_nsc.c` dengan pengecekan integer overflow.

### Changed & Fixed
- Memperbaiki penanganan objek Class 1 Data pada `NonSecure/AMI/Src/ami_cosem_model.c` agar objek yang tidak terdaftar mengembalikan `AMI_COSEM_OBJECT_NOT_FOUND` secara konsisten.
- Menyelaraskan ukuran buffer respons dan ekspektasi kolom baris Event Log #1 (2 kolom) pada `tests/test_ami_dlms_apdu.c`.
- Seluruh 15 suite CTest (**15/15, 100% PASS**) dan automated E2E Simulator Python terverifikasi sukses penuh di container Docker hardened.

---

## [v2.21.0] - 2026-11-16

### Added & Security Hardening (Remediasi F1/I2, F2/I6, C1, I5, A1 & RDP Litbang — 100% Pure C)
- **Secure Boot Kriptografis FIPS 180-4 SHA-256 + FIPS 186-4 ECDSA P-256 (`F2 / I6`)**:
  - Implementasi mandiri *zero-heap* dalam bahasa C11 murni pada `Secure/Core/Inc/ami_boot_crypto.h` dan `Secure/Core/Src/ami_boot_crypto.c`.
  - Penambahan tool host pure C `tests/ami_sign_tool.c` untuk men-generate pasangan kunci ECDSA P-256 (*Litbang prototype*), menanamkan kunci publik ke `Secure/Core/Inc/ami_boot_pubkey.h`, serta menandatangani biner `STM32U585VIT6_SM_NS.bin` pada offset `0x400`.
  - `SECURE_VerifyNonSecureImage()` pada `Secure/Core/Src/main.c` kini memverifikasi SHA-256 kanonikal, tanda tangan ECDSA P-256, dan *anti-rollback* (`TAMP->BKP0R`), serta memperbaiki batas `AMI_NS_SRAM_START/END` (`0x20040000U`–`0x200C0000U`).
- **Persistensi Kunci Terenkripsi di Secure Flash (`C1`)**:
  - Alokasi region `FLASH_KEYSTORE` (8 KB, Page 126 di `0x0C0FC000`) pada `Secure/STM32U585xx_FLASH_s.ld` (tanpa mengganggu `ROM_NSC` di `0x0C0FE000`).
  - `secure_flash_load_keys()` dan `secure_flash_save_keys()` pada `Secure/Core/Src/secure_nsc.c` mengenkripsi `SECURE_KeyStorageTypeDef` dengan AES-128-GCM terikat `UID_BASE` dan TRNG IV.
- **Otorisasi Key-Management (`F1 / I2`) & Anti-TOCTOU (`I5`)**:
  - Penambahan `SECURE_DLMS_ResetSession()` dan `SECURE_DLMS_SetSessionSap()` pada `Secure_nsclib/secure_nsc.h` & `Secure/Core/Src/secure_nsc.c`, terintegrasi dengan `NonSecure/AMI/Src/ami_dlms_port.c`.
  - Penerapan **copy-then-validate** penuh pada `SECURE_DLMS_AES_GCM_Process()` dan `SECURE_DLMS_TransferKeys_Atomic()` menggunakan buffer Secure SRAM lokal yang di-zeroize pada setiap jalur keluar.
- **Independent Watchdog (`A1`) & Opt-In RDP Level 1 Litbang**:
  - Konfigurasi `MX_IWDG_Init()` (~6 detik, *debug-freeze* aktif saat *breakpoint* ST-LINK) dan `SECURE_Watchdog_Kick()` di `NonSecure/Core/Src/app_freertos.c`.
  - `Error_Handler()` pada Secure & NonSecure kini mencatat fault (`TAMP->BKP1R = 0xFA017001U`) dan memicu `NVIC_SystemReset()`.
  - `SECURE_CheckAndApplyLitbangRdp()` menjaga RDP Level 0 secara default dan menyediakan RDP Level 1 *opt-in* (`-DAMI_LITBANG_RDP_LEVEL1=1`).
- **Pengujian Otomatis (16/16 CTest PASS & ARM Cortex-M33 Build)**:
  - Penambahan test suite ke-16 `tests/test_ami_boot_crypto.c` (`AmiBootCryptoTest`) dan skenario uji TOCTOU pada `tests/test_ami_trustzone_boundary.c`.
  - Verifikasi silang `arm-none-eabi-gcc` untuk `STM32U585VIT6_SM_S` dan `STM32U585VIT6_SM_NS` lulus dengan **0 error**.
