# STM32U585VIT6 3-Phase Smart Meter (AMI Firmware)

Proyek firmware meteran listrik pintar (*3-phase postpaid Smart Meter*) berbasis mikrokontroler STM32U585VIT6 dan metrologi AFE 9000, mengimplementasikan tumpukan protokol DLMS/COSEM (IEC 62056 / SPLN D3.006:2021) dan arsitektur keamanan TrustZone.

## Fitur Utama Sprint 1 (Phase 1)

- **COSEM Data Model**: Pembacaan register metrologi 3 fasa (Class 3), Clock (Class 8), Profile Generic untuk Event Log & Billing History (Class 7) dengan selective access.
- **DLMS Association & APDU Engine**: Mendukung AARQ/AARE (No-Auth, LLS, HLS GMAC), otentikasi/dekripsi `GLO-GET-REQUEST` / `GLO-SET-REQUEST`, pencegahan serangan replay, serta isolasi Security Setup (Class 64).
- **HDLC Framing Engine**: ISO/IEC 13239 & IEC 62056-46 Frame Format Type 3 (`0xA0`), addressing multi-octet dinamis, dual-stage CRC-16 CCITT (HCS & FCS), stripping/auto-wrapping LLC sublayer (`0xE6 0xE6 0x00` / `0xE6 0xE7 0x00`), dan negosiasi parameter SNRM/UA.
- **Host Testing & Simulator**: Suite unit-test CTest host-native tanpa hardware fisik serta interactive HDLC-over-TCP host harness/simulator untuk pengujian langsung dengan DLMS client pihak ketiga.

---

## Gurux DLMSDirector TCP Host Simulator

Untuk memvalidasi stack DLMS/COSEM dan framing HDLC tanpa board fisik, tersedia simulator host lokal yang mendengarkan paket raw HDLC-over-TCP pada port standar DLMS `4059`.

### 1. Build Simulator

Kompilasi target simulator menggunakan toolchain host (MSVC/Clang/GCC) via CMake:

<<<<<<< Updated upstream
=======
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
| **Fase 2: Bring-Up Spin 1**<br>*(Minggu 6–10)* | Hardware Spin 1 (*Low Voltage*, 8–10 unit) | Porting HAL UART/SPI ke target riil STM32U585, ekspansi 66 objek COSEM aktif, xDLMS General Block Transfer (GBT), proteksi anti-brute-force lockout (SPLN D5.002:2024), Disconnect Control Breaker Class 70, dan verifikasi Docker 16/16 CTest PASS (coverage 79.2%). | **Gate G2 (W10)**<br>*(Spin 1 Alive)* | 🟢 **100% SELESAI** |
| **Fase 3: Integrasi & Freeze**<br>*(Minggu 11–16)* | Hardware Spin 1 & Modul Sensor | Integrasi Tamper Manager 9-modus (SPLN D3.006:2021 Tabel 6), realisasi engine dinamis Time-of-Use ToU (minimal 8 tarif, 8 jadwal, 4 tipe kalender), Flash NVM wear-leveling, dan pembekuan set pembacaan DLMS (*DLMS Read-Set Freeze*). | **Gate G3 (W16)**<br>*(Release Spin 2)* | 🟡 **SEDANG BERJALAN** *(Transisi W10–W11)* |
| **Fase 4: Mains & Tape-Out**<br>*(Minggu 17–26)* | Hardware Spin 2 (*Full Mains* 230/400V) | Eksekusi Safety Gate G-S, kalibrasi metrologi akurasi Kelas 0.5S/0.2S (Fluke 6100B), verifikasi End-to-End dengan HES/MDMS PLN, uji resmi DLMS CTT v3.x 100% PASS, Soak Test 24/7 ($-25^\circ\text{C}$ s.d. $+70^\circ\text{C}$), dan serah terima akhir (*Final Tape-Out / Handoff*). | **Safety Gate G-S (W19)**<br>**Gate G4 (W21)**<br>**Gate G5 (W26 - Tape-Out)** | ⚪ **MENUNGGU SPIN 2** |

---

## 🚀 Panduan Cepat Menjalankan Pengujian

Repositori ini dirancang *reproducible* dan dapat langsung diverifikasi di lingkungan host Windows maupun Linux/Docker container.

### 1. Uji Unit Host Native (CTest)
Menjalankan seluruh 16 test suite secara deterministik:
>>>>>>> Stashed changes
```powershell
cmake -S tests -B tests/build -G Ninja
<<<<<<< Updated upstream
=======
cmake --build tests/build --parallel

# Jalankan seluruh test suite
ctest --test-dir tests/build --output-on-failure
```
*Hasil: Seluruh 16 test suite lulus 100% (Passed).*
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
>>>>>>> Stashed changes
cmake --build tests/build --target gurux_simulator
```

### 2. Menjalankan Simulator

Jalankan executable simulator dari terminal PowerShell:

```powershell
.\tests\build\host-tools\gurux_simulator.exe
```

*Output saat standby:*
```text
Listening for raw HDLC-over-TCP on 127.0.0.1:4059
```

### 3. Koneksi via Gurux DLMSDirector

Buka aplikasi **GXDLMSDirector**, buat atau konfigurasi device dengan parameter berikut:

| Parameter | Nilai Konfigurasi | Keterangan |
|---|---|---|
| **Media** | `Network` | Menggunakan koneksi TCP socket lokal |
| **Host Name / IP** | `127.0.0.1` | Local loopback address |
| **Port** | `4059` | Port standar IEC 62056-47 / DLMS |
| **Interface Type** | `HDLC` | Wajib HDLC (bukan WRAPPER) |
| **Client Address** | `16` (`0x10`) | Public/Management Client SAP |
| **Physical Server** | `1` | MAC Destination Address |
| **Logical Server** | `1` | COSEM Logical Device ID |
| **Authentication** | `None` | Profil development unauthenticated |

Klik tombol **Connect**. Setelah status terkoneksi (**Connected**), gunakan menu **Read Objects** untuk melakukan pembacaan register tegangan, arus, jam meter, dan profil riwayat.

---

## Menjalankan Unit Test (CTest)

Pengujian logika inti protokol dan model data pada PC host:

```powershell
cmake -S tests -B tests/build -G Ninja
cmake --build tests/build --parallel
ctest --test-dir tests/build --output-on-failure
```

