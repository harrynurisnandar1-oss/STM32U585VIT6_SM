# ⚡ STM32U585VIT6 + ADE9000 3-Phase AMI Smart Meter

[![Tests](https://img.shields.io/badge/CTest%20Host-16%2F16%20Passing-brightgreen?style=flat-square&logo=cmake)](tests/)
[![Docker Tests](https://img.shields.io/badge/Docker%20E2E-100%25%20PASS-blue?style=flat-square&logo=docker)](Dockerfile)
[![Coverage](https://img.shields.io/badge/Code%20Coverage-79.2%25%20(LCOV)-informational?style=flat-square&logo=codecov)](docs/COVERAGE.md)
[![Standard](https://img.shields.io/badge/Standard-SPLN%20D3.006%3A2021-orange?style=flat-square)](dokumen_PLN/)
[![Security Suite](https://img.shields.io/badge/Security-Suite%200%20%7C%20HLS5%20GMAC-red?style=flat-square)](docs/security-rules/)
[![Architecture](https://img.shields.io/badge/Target-ARM%20Cortex--M33%20TrustZone-purple?style=flat-square)](Secure/)

Firmware kelas produksi untuk meter listrik pintar (*smart meter*) 3-fasa pascabayar berbasis mikrokontroler **STM32U585VIT6** (ARM Cortex-M33 TrustZone) dan metering analog front-end **ADE9000**. Repositori ini mengintegrasikan tumpukan protokol lengkap **DLMS/COSEM** (IEC 62056 / SPLN D3.006:2021), cipher suite keamanan tinggi **SPLN D5.002:2024**, driver metrologi ADE9000, host unit testing, serta simulator terintegrasi untuk verifikasi end-to-end.

---

## 📌 Ringkasan Eksekutif Pekerjaan E2: Protokol & Data Model

Tujuan utama paket pekerjaan E2 adalah menjamin interoperabilitas meter terhadap sistem pembacaan jarak jauh (**Head-End System / HES PLN**) dengan data model COSEM yang deterministik, performa streaming zero-heap, dan keamanan kriptografis *fail-closed*.

```
┌────────────────────────────────────────────────────────────────────────┐
│                   HEAD-END SYSTEM (HES) / DLMS CLIENT                  │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ Optical Probe / RS485 (HDLC Stream)
┌───────────────────────────────────▼────────────────────────────────────┐
│                       NON-SECURE WORLD (Cortex-M33)                    │
│  ┌───────────────────────┐  ┌─────────────────┐  ┌──────────────────┐  │
│  │   HDLC / LLC Parser   │  │   ACSE Engine   │  │ xDLMS APDU Engine│  │
│  │   Framing Type 3      │  │   AARQ / AARE   │  │ Normal & GBT     │  │
│  └───────────┬───────────┘  └────────┬────────┘  └────────┬─────────┘  │
│              │                       │                    │            │
│  ┌───────────▼───────────────────────▼────────────────────▼─────────┐  │
│  │                    COSEM Object Model Layer                       │  │
│  │  - 66 Registered PLN Objects       - Tamper Logbook (9 Modus)    │  │
│  │  - Load Profile (23 Channels)      - Disconnect Control (Cl. 70) │  │
│  │  - Instant Profile (11 Channels)   - PQ Voltage Profiles (R/S/T) │  │
│  │  - Billing Profile (Class 7)       - Multi-Tariff WBP/LWBP (1..4)│  │
│  │  - Classes 6, 11, 20, 22 Supported - Event Log #1 (2 Columns)    │  │
│  └───────────────────────────────────┬───────────────────────────────┘  │
│                                      │ Non-Secure Callable (NSC)       │
├──────────────────────────────────────┼─────────────────────────────────┤
│                                      ▼ Secure Gateway Entry            │
│                         SECURE WORLD (TrustZone)                       │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │  - AES-128-GCM Suite 0 (glo-get / glo-set / glo-action)          │  │
│  │  - HLS 5 GMAC Mutual Authentication (4-Pass)                     │  │
│  │  - RFC 3394 AES Key Wrap Unwrapping (Master Key / KEK Isolation) │  │
│  │  - Hardware TRNG & Strictly Monotonic Anti-Replay Counter        │  │
│  └──────────────────────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────────────────────┘
```

### 🎯 Pilar Utama Implementasi
* **Protokol Komunikasi DLMS/COSEM & ACSE**: State machine asosiasi AARQ/AARE, negosiasi dinamis conformance block (`Get` + `SelectiveAccess`), Max PDU negotiation, serta silent-drop frame anomali sesuai uji CTT.
* **HDLC Data Link Layer Deterministik**: Parser streaming zero-heap $O(1)$, Framing Type 3 (`0xA0`), dual-stage CRC-16 (HCS & FCS) dengan ketahanan transparent flag `0x7E`, serta auto-wrap/stripping LLC sublayer (`0xE6 0xE6 0x00` / `0xE6 0xE7 0x00`).
* **Data Model 66 Objek Standar PLN**: 66 objek COSEM aktif siap baca via `GET` dan tersegmentasi otomatis menggunakan **General Block Transfer (GBT)** jika melebihi ukuran PDU.
* **Histori Profiling Class 7 & Selective Access**: 
  * **Load Profile 15-Menit (`1.0.99.1.0.255`)**: 23 kanal komprehensif (energi aktif/reaktif impor/ekspor, 4 kuadran Q1..Q4, tegangan & arus per-fasa, arus netral, daya aktif per-fasa, PF total, frekuensi, serta THD tegangan).
  * **Instant Profile (`1.96.98.128.0.255`)**: 11 kanal pengukuran sesaat (Waktu, Tegangan 3-Fasa, Arus 3-Fasa, Daya Aktif/Reaktif Total, PF Total, Frekuensi).
  * **Power Quality Voltage Log Profiles (`1.0.99.14.2..4.255`)**: Profil kualitas tegangan fase R, S, dan T (Capture: Clock, Event Code, Phase Voltage).
  * **Event Log #1 (`0.0.99.98.0.255`)**: Logbook kejadian operasional standar 2 kolom (Clock & Event Code).
  * **Tamper Event Logbook (`1.97.99.98.x.255`)**: 3 kolom (Clock, Event Code, Status) mendukung 9 modus tamper PLN (Cover Open, Terminal Open, Reverse Phase, Neutral Disturbance, Magnetic Tamper, Loss of Phase, Neutral Bypass, Reverse Power, dan Over Voltage/Current).
  * **Dukungan Selector**: Selector 1 (Rentang Waktu Clock) dan Selector 2 (Rentang Indeks Baris).
* **Interface Classes Tambahan**: Register Activation (Class 6), Special Days Table (Class 11), Activity Calendar (Class 20), dan Single Action Schedule (Class 22).
* **Keamanan Kriptografi Berlapis (SPLN D5.002:2024)**:
  * DLMS Security Suite 0 (`glo-get`, `glo-set`, `glo-action`) dengan AES-128-GCM dan Nonce 12-byte.
  * 4-Pass Mutual Authentication **HLS 5 (GMAC)** OID `2.16.756.5.8.2.5` dengan constant-time comparison dan memory zeroization.
  * Perlindungan Anti-Replay RX Invocation Counter monotonik & alokasi aman TX Counter.
  * Perlindungan **Anti-Brute-Force & Lockout**: Delay penalti eksponensial progresif (5s, 10s, 20s, 40s) dan hard lockout permanen setelah 5x gagal.
* **Pemutus Arus Jarak Jauh (Disconnect Control Class 70)**: Kontrol relay beban (`0.0.96.3.10.255`) dengan state machine *Disconnected*, *Connected*, dan *Ready_for_Reconnection*.
* **Driver Metrologi ADE9000**: Pemetaan register SPI 3-fasa terkalibrasi ke snapshot register DLMS/COSEM secara fail-closed.

---

## 📊 Matriks Status Kesiapan Sistem

Implementasi diklasifikasikan secara transparan berdasarkan kesiapan software dan ketergantungan perangkat keras target:

> **Ringkasan Status:**
> * 🟢 **Software, Protokol, dan Data Model (Host & Docker)**: **100% SELESAI & TERVERIFIKASI** (16/16 CTest PASS, 79.2% LCOV Coverage).
> * 🟡 **Driver Firmware Target (STM32U585 & ADE9000)**: **KODE SELESAI** (Siap uji flashing pada hardware fisik).
> * ⚪ **Sertifikasi Formal Laboratorium**: **MENUNGGU UJI LAB** (DLMS UA CTT Tool & Uji Interop HES).

| Kategori | Fitur / Komponen | Status | Catatan Verifikasi |
|---|---|:---:|---|
| **Data Link** | HDLC Framing Type 3, Multi-octet Address, Stream Parser | 🟢 **SUDAH** | Terverifikasi di `ami_dlms_hdlc.c` & `test_ami_dlms_hdlc.c` |
| **Data Link** | Dual-stage CRC-16 (HCS/FCS) & Resiliensi Transparent Flag `0x7E` | 🟢 **SUDAH** | Lolos uji frame riil checksum `0x7E 0x0A 0x7E` |
| **Data Link** | LLC Sublayer Handling (`0xE6 0xE6 0x00` / `0xE6 0xE7 0x00`) | 🟢 **SUDAH** | Sesuai kepatuhan IEC 62056-46 |
| **Aplikasi** | ACSE Association Engine (AARQ/AARE, Max PDU, Conformance) | 🟢 **SUDAH** | Terverifikasi di `ami_dlms_association.c` |
| **Data Model** | 276 Kode OBIS Standar PLN (`ami_obis_catalog.h`) | 🟢 **SUDAH** | Terpetakan dalam 22 kategori fungsional |
| **Data Model** | Association View 66 Objek Aktif Standar PLN via `GET` | 🟢 **SUDAH** | 66 objek terdaftar dan teruji aksesnya |
| **Data Model** | Load, Instant, & PQ Voltage Log Profiles (Class 7) | 🟢 **SUDAH** | Load (23-ch), Instant (11-ch), PQ Voltage R/S/T, Event Log (2-col) |
| **Data Model** | Tamper Event Logbook (9 Modus Tamper Resmi PLN) | 🟢 **SUDAH** | Snapshot otomatis via trigger metrologi ADE9000 |
| **Data Model** | General Block Transfer (GBT) untuk APDU Besar | 🟢 **SUDAH** | Segmentasi & rekonstruksi blok di `ami_dlms_block_transfer.c` |
| **Beban & Tarif** | Disconnect Control Breaker (Class 70 / `0.0.96.3.10.255`) | 🟢 **SUDAH** | State machine, remote disconnect/reconnect di `ami_disconnect_control.c` |
| **Beban & Tarif** | Multi-Tarif WBP/LWBP (Tarif 1 s.d. 4) & Demand Limiter (Class 71) | 🟢 **SUDAH** | Terverifikasi di `ami_cosem_model.c` & `ami_dlms_apdu.c` |
| **Keamanan** | Core NIST AES-128 & GHASH GF($2^{128}$) Standalone | 🟢 **SUDAH** | 100% Lolos NIST SP 800-38D KAT Vector Case 1, 2, 4 |
| **Keamanan** | 4-Pass Mutual Authentication HLS 5 (GMAC) OID `2.16.756.5.8.2.5` | 🟢 **SUDAH** | Constant-time check & zeroization di `ami_security.c` |
| **Keamanan** | DLMS Security Suite 0 Ciphered APDU (`0xC8`, `0xC9`, `0xCB`) | 🟢 **SUDAH** | Terverifikasi di `ami_dlms_cipher.c` |
| **Keamanan** | Anti-Replay Invocation Counter & Wear-Leveling NVM Engine | 🟢 **SUDAH** | Penanganan saturasi `0xFFFFFFFF` fail-closed (`ami_nvm_counter.c`) |
| **Keamanan** | Anti-Brute-Force Lockout (Backoff Eksponensial Progresif) | 🟢 **SUDAH** | Penalti 5s, 10s, 20s, 40s & lockout 5x (`ami_anti_bruteforce.c`) |
| **TrustZone** | Partisi SAU, GTZC (TZSC/MPCBB/TZIC), & NonSecure Callable (NSC) | 🟢 **SUDAH** | Range check `cmse_check_address_range` & *copy-then-validate* anti-TOCTOU di `secure_nsc.c` |
| **Key Management** | Master Key (KEK) per-device, Flash Keystore (`0x0C0FC000`), & RFC 3394 | 🟢 **SUDAH** | Persistensi terenkripsi AES-128-GCM di Page 126 Secure Flash + update atomik |
| **Secure Boot** | Verifikasi SHA-256 + ECDSA P-256 & Anti-Rollback (`TAMP->BKP0R`) | 🟢 **SUDAH** | Pure C FIPS 180-4 / 186-4 (`ami_boot_crypto.c`, `ami_sign_tool.c`) |
| **Watchdog & RDP** | IWDG (~6s, *debug-freeze* ST-LINK) & Opt-In RDP Level 1 Litbang | 🟢 **SUDAH** | `MX_IWDG_Init()`, `SECURE_Watchdog_Kick()`, & `AMI_LITBANG_RDP_LEVEL1` |
| **Testing** | CTest Host Native Unit Testing (16 Test Suites) | 🟢 **SUDAH** | 16/16 tests PASS (100% Lulus, termasuk `AmiBootCryptoTest`) |
| **Testing** | 4 Kriteria Verifikasi Pengujian Keamanan | 🟢 **SUDAH** | Coverage (79.2%), Negative/1-bit flip, Zeroize Barrier, TrustZone CMSE |
| **Testing** | Single Unified Docker Test Container & E2E DLMS Simulator | 🟢 **SUDAH** | Otomasi pengujian Linux x86_64 Alpine + Python client |
| **Target Driver** | Driver UART DMA Circular Buffer & IDLE line interrupt | 🟡 **KODE SELESAI** | Menunggu verifikasi osiloskop / kabel serial board |
| **Target Driver** | Driver Metrologi ADE9000 SPI Fail-Closed & Scaling 3-Fasa | 🟡 **KODE SELESAI** | Menunggu uji kalibrasi analog IC ADE9000 |
| **Target Driver** | Generator Acak Fisik Hardware TRNG STM32U5 | 🟡 **KODE SELESAI** | Menunggu running pada register silikon STM32U5 |
| **Target Driver** | Secure Boot Image Manifest Verification (Magic `PLN!`, SHA-256) | 🟡 **KODE SELESAI** | Menunggu flashing dual-image ke Flash target |
| **Produksi** | Penguncian Readout Protection (RDP Level 1/2) & Disable SWD | ⚪ **BELUM** | Dilakukan saat fase flashing produksi massal |
| **Sertifikasi** | Uji Kepatuhan DLMS UA CTT & Uji Lapangan HES PLN | ⚪ **BELUM** | Memerlukan lab sertifikasi resmi PLN |

---

## 🗺️ Roadmap & Fase Pengembangan (Fase Awal s.d. Tape-Out)

Rencana kerja 26-minggu paket E2 (Protokol & Data Model Smart Meter AMI 3-Fasa) mengadopsi rilis bertahap dari tahap *blind development* hingga penyerahan produksi (*Final Tape-Out & Handoff*):

| Fase & Durasi | Target Hardware | Fokus & Aktivitas Utama | Pintu Gerbang (*Milestone*) | Status Kesiapan |
| :--- | :--- | :--- | :--- | :---: |
| **Fase 1: Inisiasi & Fondasi**<br>*(Minggu 1–5)* | Host PC & Dev Board STM32U575VGT6 | Implementasi tumpukan dasar DLMS/COSEM, ACSE (AARQ/AARE), HDLC streaming zero-heap, partisi TrustZone SAU/GTZC, AES-128-GCM Suite 0, HLS 5 GMAC, serta mock register metrologi AFE ADE9000. | **Gate G0 (W1)**<br>**Gate G1 (W5)** | 🟢 **100% SELESAI** |
| **Fase 2: Bring-Up Spin 1**<br>*(Minggu 6–10)* | Hardware Spin 1 (*Low Voltage*, 8–10 unit) | Porting HAL UART/SPI ke target riil STM32U585, ekspansi 66 objek COSEM aktif, xDLMS General Block Transfer (GBT), proteksi anti-brute-force lockout (SPLN D5.002:2024), Disconnect Control Breaker Class 70, dan verifikasi Docker 15/15 CTest PASS (coverage 79.2%). | **Gate G2 (W10)**<br>*(Spin 1 Alive)* | 🟢 **100% SELESAI** |
| **Fase 3: Integrasi & Freeze**<br>*(Minggu 11–16)* | Hardware Spin 1 & Modul Sensor | Integrasi Tamper Manager 9-modus (SPLN D3.006:2021 Tabel 6), realisasi engine dinamis Time-of-Use ToU (minimal 8 tarif, 8 jadwal, 4 tipe kalender), Flash NVM wear-leveling, dan pembekuan set pembacaan DLMS (*DLMS Read-Set Freeze*). | **Gate G3 (W16)**<br>*(Release Spin 2)* | 🟡 **SEDANG BERJALAN** *(Transisi W10–W11)* |
| **Fase 4: Mains & Tape-Out**<br>*(Minggu 17–26)* | Hardware Spin 2 (*Full Mains* 230/400V) | Eksekusi Safety Gate G-S, kalibrasi metrologi akurasi Kelas 0.5S/0.2S (Fluke 6100B), verifikasi End-to-End dengan HES/MDMS PLN, uji resmi DLMS CTT v3.x 100% PASS, Soak Test 24/7 ($-25^\circ\text{C}$ s.d. $+70^\circ\text{C}$), dan serah terima akhir (*Final Tape-Out / Handoff*). | **Safety Gate G-S (W19)**<br>**Gate G4 (W21)**<br>**Gate G5 (W26 - Tape-Out)** | ⚪ **MENUNGGU SPIN 2** |

---

Repositori ini dirancang *reproducible* dan dapat langsung diverifikasi di lingkungan host Windows maupun Linux/Docker container.

### 1. Uji Unit Host Native (CTest)
Menjalankan seluruh 15 test suite secara deterministik:
```powershell
# Konfigurasi dan kompilasi menggunakan Ninja
cmake -S tests -B tests/build -G Ninja
cmake --build tests/build --parallel

# Jalankan seluruh test suite
ctest --test-dir tests/build --output-on-failure
```
*Hasil: Seluruh 15 test suite lulus 100% (Passed).*

Untuk menjalankan analisis **Code Coverage LCOV**:
```powershell
cmake -S tests -B tests/build_cov -G Ninja -DENABLE_COVERAGE=ON
cmake --build tests/build_cov
ctest --test-dir tests/build_cov
lcov --capture --directory tests/build_cov --output-file tests/build_cov/coverage_raw.info --ignore-errors inconsistent,gcov
lcov --extract tests/build_cov/coverage_raw.info '*/NonSecure/AMI/*' '*/Secure/*' --output-file tests/build_cov/coverage_filtered.info --ignore-errors unused,inconsistent
lcov --list tests/build_cov/coverage_filtered.info
```
*Hasil: Line Coverage 79.2% (2.600/3.281 baris) & Function Coverage 95.7% (154/161 fungsi).*

### 2. Simulator DLMS Host untuk Gurux DLMSDirector
Untuk menguji konektivitas visual dengan software industri **GXDLMSDirector**:
```powershell
# Kompilasi target simulator
cmake --build tests/build --target gurux_simulator

# Jalankan simulator pada port TCP 4059
.\tests\build\host-tools\gurux_simulator.exe
```
1. Buka aplikasi **GXDLMSDirector**.
2. Muat file profil workspace yang sudah disinkronkan: [`STM32U585VIT6_SM.gxc`](STM32U585VIT6_SM.gxc).
3. Klik tombol **Connect** untuk membaca objek COSEM secara interaktif.

### 3. Uji Otomatis via Docker (Single Unified Test Container)
Container Docker terpadu menjalankan kompilasi clean, 14 suite CTest, serta simulasi client Python E2E:
```powershell
# Build image docker
docker build -t stm32u585vit6sm:latest .

# Jalankan pengujian terpadu
docker run --rm stm32u585vit6sm:latest ./run_all_docker_tests.sh
```

### 4. Tool Penandatanganan Image Secure Boot (Pure C)
Untuk menandatangani biner NonSecure (`STM32U585VIT6_SM_NS.bin`) dengan kunci ECDSA P-256 sebelum di-flash ke papan target:
```powershell
# Generate ulang header kunci publik (opsional)
.\tests\build_host\host-tools\ami_sign_tool.exe --gen-header Secure/Core/Inc/ami_boot_pubkey.h

# Tandatangani image NonSecure (epoch = 1)
.\tests\build_host\host-tools\ami_sign_tool.exe --sign NonSecure/build/STM32U585VIT6_SM_NS.bin 1
```

---

## 📂 Struktur Repositori

```text
├── NonSecure/                      # Ruang eksekusi NonSecure (Cortex-M33)
│   ├── AMI/                        # Implementasi Inti DLMS/COSEM & Metrologi
│   │   ├── Inc/                    # Header file (ami_cosem_model.h, dll.)
│   │   └── Src/                    # Source code (HDLC, APDU, Security, Cipher, ADE9000)
│   └── Core/                       # Startup, FreeRTOS, dan konfigurasi HAL
├── Secure/                         # Ruang eksekusi Secure World (TrustZone)
│   └── Core/Src/secure_nsc.c       # Gerbang NonSecure Callable (NSC) & Kriptografi
├── Secure_nsclib/                  # Header publik antarmuka NSC
├── tests/                          # 15 Test Suite Host CTest & Simulator
│   ├── test_ami_cosem.c            # Verifikasi 66 objek COSEM & Load Profile
│   ├── test_ami_anti_bruteforce.c  # Pengujian lockout & backoff eksponensial
│   ├── test_ami_trustzone_boundary.c # Pengujian batas memori CMSE NonSecure/Secure
│   └── host-tools/gurux_simulator.c# Simulator HDLC-over-TCP port 4059
├── docker_test/                    # Lingkungan pengujian terisolasi Docker
├── docs/                           # Dokumentasi teknis mendalam
│   └── security-rules/             # Spesifikasi protokol, arsitektur, ancaman
├── CHANGELOG.md                    # Riwayat lengkap perubahan dan rilis versi
├── CODE_OF_CONDUCT.md              # Pedoman perilaku dan etika kontributor
└── STM32U585VIT6_SM.gxc            # Profil workspace Gurux DLMSDirector
```

---

## 📚 Dokumen Acuan & Standar

* 📄 [`docs/security-rules/PROTOCOL_PROFILE.md`](docs/security-rules/PROTOCOL_PROFILE.md) — Spesifikasi baseline protokol dan asosiasi DLMS/COSEM.
* 🛡️ [`docs/security-rules/SECURITY_ARCHITECTURE.md`](docs/security-rules/SECURITY_ARCHITECTURE.md) — Arsitektur pemisahan TrustZone Secure vs NonSecure.
* 📋 [`docs/security-rules/SECURITY_CHECKLIST.md`](docs/security-rules/SECURITY_CHECKLIST.md) — Daftar uji kepatuhan keamanan rilis firmware.
* 🔍 [`docs/security-rules/Threat_MODEL.md`](docs/security-rules/Threat_MODEL.md) — Analisis model mitigasi ancaman (T1 s.d. T12).
* 📝 [`CHANGELOG.md`](CHANGELOG.md) — Catatan riwayat versi pengembangan proyek.
* 🤝 [`CODE_OF_CONDUCT.md`](CODE_OF_CONDUCT.md) — Pedoman perilaku dan etika kontributor.
