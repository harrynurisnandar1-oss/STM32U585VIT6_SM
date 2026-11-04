# STM32U585VIT6 + ADE9000 3-Phase AMI Smart Meter

Firmware proyek meter listrik pintar pascabayar 3-fasa dengan target mikrokontroler STM32U585VIT6 (ARM Cortex-M33 TrustZone) dan metering IC ADE9000. Repository berisi implementasi lengkap protokol komunikasi DLMS/COSEM (IEC 62056 / SPLN D3.006:2021), cipher suite keamanan (SPLN D5.002:2024), driver metrologi ADE9000, host tests, serta simulator terintegrasi untuk pengujian protokol.

## Ringkasan Pekerjaan E2: Protokol dan Data Model

Fokus pekerjaan E2 adalah menyediakan komunikasi meter melalui DLMS/COSEM dan menyajikan data meter sebagai objek COSEM yang konsisten, aman, dan mematuhi regulasi PLN:

### Cakupan E2

- **Protokol DLMS/COSEM & ACSE:** Pemrosesan asosiasi AARQ/AARE, negosiasi conformance dinamis (`Get` + `SelectiveAccess`), Max PDU size negotiation, penanganan status disconnect, dan silent drop sesuai standar CTT.
- **HDLC Data Link Layer:** Parser streaming deterministik zero-heap $O(1)$, framing Frame Format Type 3 (`0xA0`), dual-stage CRC-16 CCITT (HCS & FCS) dengan resiliensi transparent flag `0x7E`, stripping/auto-wrapping LLC sublayer (`0xE6 0xE6 0x00` / `0xE6 0xE7 0x00`), dan negosiasi parameter SNRM/UA.
- **Data Model COSEM & Katalog PLN:** Pemetaan katalog lengkap 276 kode OBIS standar PLN pascabayar (`ami_obis_catalog.h`) ke dalam 22 kategori fungsional, serta implementasi aktif 49 objek standar PLN dalam Association View yang siap diakses via xDLMS `GET` dan tersegmentasi rapi via General Block Transfer (GBT).
- **Profile Generic & Selective Access:** Implementasi buffer histori Class 7 untuk Tamper Event Logbook (3 Capture Objects: Clock, Event Code, Status Register, mendukung 9 Modus Tamper PLN), Monthly Billing Profile, dan Load Profile 15-menit (23 kanal lengkap: energi kumulatif, kuadran Q1..Q4, tegangan, arus fasa/netral, daya sesaat, PF, frekuensi, THD) dengan dukungan Selector 1 (DateTime Range) dan Selector 2 (Entry Index Range).
- **Keamanan Protokol & APDU Ciphering:**
  - Penegakan fail-closed policy: Firmware menolak No-Auth, LLS, HLS3/MD5, HLS4/SHA-1, dan challenge bukan 16 byte.
  - DLMS Security Suite 0 (`glo-get`, `glo-set`, `glo-action`) dengan enkripsi terotentikasi AES-128-GCM, Security Control `0x30`, dan 12-byte Nonce/IV.
  - Perlindungan anti-replay: Invocation Counter strictly monotonic (`rx_counter > last_rx_counter`) dan penolakan otomatis counter duplikat/stale.
  - Mutual 4-pass authentication HLS 5 (GMAC) OID `2.16.756.5.8.2.5` dengan constant-time comparison dan zeroization memori kerja (`secure_zeroize`).
  - Isolasi API raw-key: Firmware NonSecure hanya mengenal referensi aman `AmiKeyHandle`.
- **Driver Metrologi ADE9000:** Pemetaan register SPI 3-Fasa (RMS, Daya Aktif/Reaktif, Energi, Periode/Frekuensi, Status) dan feeder snapshot otomatis ke register DLMS/COSEM.
- **Verifikasi Host & Docker:** 14/14 suite CTest host lulus 100% serta automated Docker containerized test runner (Alpine 23 MB, 0 CVE) dengan simulasi client Python E2E.

### Batas Tanggung Jawab dan Status Integrasi

E2 menangani kontrak data antara sumber data meter dan protokol: objek/OBIS yang disediakan, format nilai, akses, dan perilaku respons. Pembacaan register fisik SPI ADE9000 pada PCB nyata, kalkulasi akurasi metrologi Kelas 0.2S/0.5S, kalibrasi, serta driver UART fisik/optik menjadi tanggung jawab integrasi hardware/metrologi.

## Matriks Status Pekerjaan Proyek (Sudah vs Belum Dikerjakan)

Tabel berikut merangkum status implementasi sistem Smart Meter 3-Fasa berdasarkan pembagian fase pengembangan **Spin 1 (Prototipe / Purwarupa)** dan **Spin 2 (Target Hardware Produksi & Sertifikasi)** sesuai standar **SPLN D3.006:2021**, **SPLN D5.002:2024**, dan **IEC 62056**.

Pemisahan status secara tegas dan transparan:
1. **Layer Software, Logika Protokol & Data Model (Simulasi Host)**: **100% Selesai & Terverifikasi** melalui **14/14 suite CTest host (100% Passed)**.
2. **Layer Driver Firmware Target STM32U5 & Metrologi ADE9000**: **Selesai Diimplementasikan di Kode Sumber** (HAL UART/SPI aktif, DMA circular buffer IDLE driver, verifikasi return code ADE9000, secure boot manifest validator, dan TRNG fail-closed), saat ini berstatus **Menunggu Flashing & Uji Bench Perangkat Keras Fisik (Bench Testing Required)**.
3. **Sertifikasi Formal & Uji Lab**: **Belum Dikerjakan (Memerlukan Lab PLN / CTT Tool)**.

| Area / Komponen | Item Pekerjaan | Status | Catatan / Lokasi Implementasi |
|---|---|:---:|---|
| **Protokol HDLC** | HDLC Framing Type 3 (`0xA0`), parsing streaming O(1), addressing multi-octet | **SUDAH** | Terverifikasi di `NonSecure/AMI/Src/ami_dlms_hdlc.c` & `test_ami_dlms_hdlc.c` |
| **Protokol HDLC** | Dual-stage CRC-16 (HCS & FCS) dan resiliensi transparent flag `0x7E` | **SUDAH** | Teruji via frame riil Gurux dengan checksum berakhiran `0x7E 0x0A 0x7E` |
| **Protokol HDLC** | Stripping/auto-wrapping LLC sublayer (`0xE6 0xE6 0x00` / `0xE6 0xE7 0x00`) | **SUDAH** | Memenuhi kepatuhan IEC 62056-46 pada I-Frame APDU |
| **Protokol ACSE** | State machine AARQ/AARE, negosiasi conformance dinamis, Max PDU | **SUDAH** | Terverifikasi di `NonSecure/AMI/Src/ami_dlms_association.c` |
| **Data Model COSEM** | Pemetaan katalog lengkap 276 kode OBIS standar PLN pascabayar | **SUDAH** | Didefinisikan dalam 22 kategori di `NonSecure/AMI/Inc/ami_obis_catalog.h` |
| **Data Model COSEM** | Implementasi Association View terdaftar (49 objek aktif) via `GET` | **SUDAH** | 49 objek terverifikasi (termasuk Load Profile 23-kanal `1.0.99.1.0.255`) |
| **Data Model COSEM** | Selective Access Class 7 (rentang waktu Clock & rentang entri baris) | **SUDAH** | Selector 1 & 2 terimplementasi untuk Load Profile 23-kanal, Billing, dan Tamper Log (9 Modus Tamper) |
| **Keamanan Host** | Core Kriptografi mandiri AES-128 NIST FIPS 197 & GHASH GF(2^128) | **SUDAH** | Lolos 100% Known Answer Test (KAT) NIST SP 800-38D Case 1, 2, 4 |
| **Keamanan Host** | 4-Pass Mutual Authentication HLS 5 (GMAC) OID `2.16.756.5.8.2.5` | **SUDAH** | Constant-time verification & zeroization di `ami_security.c` |
| **Keamanan Port** | Penegakan fail-closed policy (blokir No-Auth & LLS pada build firmware) | **SUDAH** | `#if !defined(STM32U585xx)` mengisolasi helper development |
| **Keamanan Port** | Validasi alamat peer HDLC firmware (Client SAP 16, Server Address 1) | **SUDAH** | Menolak frame asing sebelum masuk ke stack APDU (`ami_dlms_port.c`) |
| **Keamanan Port** | Isolasi API raw-key kriptografi dari kompilasi NonSecure firmware | **SUDAH** | NonSecure firmware hanya mengenal opaque reference `AmiKeyHandle` |
| **Keamanan APDU** | DLMS Security Suite 0 Ciphered APDU Engine (`0xC8`, `0xC9`, `0xCB`) | **SUDAH** | Terverifikasi di `NonSecure/AMI/Src/ami_dlms_cipher.c` & `test_ami_dlms_cipher.c` |
| **Keamanan APDU** | Anti-Replay RX Counter Monotonik & Monotonic TX Counter Allocation | **SUDAH** | Menolak replay frame seketika (`AMI_DLMS_APDU_COUNTER_REJECTED`) |
| **Metrologi Driver** | Driver Metrologi ADE9000 SPI Register Map & Feeder Snapshot | **SUDAH** | Terverifikasi di `NonSecure/AMI/Src/ami_ade9000.c` & `test_ami_ade9000.c` |
| **Hardware TrustZone** | Konfigurasi SAU, GTZC (TZSC/MPCBB/TZIC), dan isolasi peripheral kripto | **SUDAH** | Terkonfigurasi di `partition_stm32u585xx.h`, `Secure/Core/Src/main.c`, `stm32u5xx_it.c` |
| **TrustZone Gateway** | Gerbang NonSecure Callable (NSC) `CMSE_NS_ENTRY` & pointer range check | **SUDAH** | Divalidasi via `cmse_check_address_range` di `Secure/Core/Src/secure_nsc.c` |
| **Key Management** | Provisi pabrik Master Key (KEK) unik per-device (UID) di Secure Storage | **SUDAH** | Terisolasi di Secure World via `SECURE_ProvisionMasterKey` & derivasi UID register |
| **Key Management** | Remote Key Transfer RFC 3394 AES Key Wrap atomik di Secure World | **SUDAH** | Atomic update batch (GUEK, GBEK, AK) all-or-nothing rollback di `secure_nsc.c` |
| **Testing & Simulasi** | CTest Host Native Unit Tests (14/14 suite 100% Passed) | **SUDAH** | Menjalankan pengujian regresi deterministik via Ninja |
| **Testing & Simulasi** | Simulator TCP Host (`gurux_simulator.exe` port 4059) & E2E Docker Runner | **SUDAH** | Docker Compose terpusat di `docker_test/` (Alpine 23 MB, 0 CVE) |
| **Testing & Simulasi** | Profil Workspace Gurux DLMSDirector XML (`STM32U585VIT6_SM.gxc`) | **SUDAH** | Tersinkronisasi dengan seluruh parameter objek PLN |
| **Anti-Replay NVM** | Monotonic Invocation Counter persisten & wear-leveling NVM/TAMP Backup | **SUDAH** | Terverifikasi di `NonSecure/AMI/Src/ami_nvm_counter.c` & `test_ami_nvm_counter.c` |
| **Anti-Replay NVM** | Penanganan saturasi Invocation Counter (`0xFFFFFFFF` fail-closed) | **SUDAH** | Teruji via `test_nvm_counter_saturation_handling` (menolak alokasi saat mencapai batas uint32) |
| **Anti-Brute-Force** | Progressive exponential backoff / lockout setelah N kali gagal HLS | **SUDAH** | Tracking `failed_attempts`, penalti progresif (5s, 10s, 20s, 40s), hard lockout 5x per SPLN D5.002:2024 (`ami_anti_bruteforce.c` & `test_ami_anti_bruteforce.c`) |
| **Kontrol Beban** | Eksekusi Disconnect Control Breaker (Class 70 / OBIS `0.0.96.3.10.255`) | **SUDAH** | State machine (Disconnected, Connected, Ready), method remote_disconnect/reconnect via Action & GPIO HAL (`ami_disconnect_control.c` & `test_ami_disconnect_control.c`) |
| **Data Model Lanjutan** | Ekspansi objek Association View (49 Objek Aktif Standar PLN) | **SUDAH** | Multi-tarif WBP/LWBP (Tarif 1-4), Power Quality (THD V/I, PF, Freq), Demand Limiter Class 71 (`ami_cosem_model.c` & `ami_dlms_apdu.c`) |
| **Protokol Lanjutan** | General Block Transfer (GBT) untuk data profil historis besar | **SUDAH** | Pemotongan APDU melebihi buffer threshold, block numbering, last block flag (`ami_dlms_block_transfer.c` & `test_ami_block_transfer.c`) |
|---|---|---|---|
| *KELOMPOK 2: KESIAPAN TARGET HARDWARE & SERTIFIKASI* | **Status Target Hardware (STM32U585 / ADE9000 / Lab PLN)** | | |
| **Hardware Driver UART** | Driver UART DMA Circular Buffer & IDLE line interrupt (`ami_bsp_uart.c`, `stm32u5xx_it.c`) | **Kode Selesai (Perlu Uji Board)** | HAL UART aktif, stream buffer FreeRTOS terhubung; menunggu uji kabel serial/optik |
| **Metrologi ADE9000** | Pemeriksaan ketat SPI return code, fail-closed snapshot, scaling 3-fasa (`ami_ade9000.c`) | **Kode Selesai (Perlu Uji Board)** | Menolak data kotor saat SPI gagal; menunggu kalibrasi register fisik IC ADE9000 |
| **Hardware TRNG** | Generator acak fisik fail-closed STM32U5 tanpa fallback LCG (`secure_nsc.c`) | **Kode Selesai (Perlu Uji Board)** | Otomatis abort jika CECS/SECS/timeout; menunggu running pada silikon STM32U5 |
| **Keamanan Boot** | Secure Boot Manifest Verification (Magic `0x504C4E21`, size, version epoch, SHA-256) | **Kode Selesai (Perlu Uji Board)** | Validasi pra-jump di `Secure/Core/Src/main.c`; menunggu flashing signed image |
| **Hardware SAES** | Implementasi akselerator SAES hardware STM32U5 di Secure World (`Secure/`) | **BELUM (Perlu HW)** | Menghubungkan fungsi NSC ke register peripheral SAES hardware STM32U5 |
| **Keamanan Fisik** | Penguncian Readout Protection (RDP Level 1/2) & penonaktifan JTAG/SWD | **BELUM (Perlu HW)** | Mencegah dumping firmware dan memori kunci melalui debugger fisik |
| **Sertifikasi** | Uji formal DLMS User Association Conformance Test Tool (CTT) & HES PLN | **BELUM (Perlu Lab)** | Sertifikasi kepatuhan penuh standar utilitas sebelum komersialisasi |

---

## Workflow Pekerjaan E2: Spin 1 dan Spin 2

### Spin 1 — Fondasi Protokol dan Integrasi Awal (Tahap Saat Ini / Selesai)

#### Phase 1 — Desain Protokol dan Implementasi Host
- Baseline protokol DLMS/COSEM: Logical Name (LN) referencing, Dedicated Client SAP `16`, Server Address `1`.
- Pemetaan 276 kode OBIS standar PLN pascabayar dan verifikasi aktif 13 objek Association View via `GET`.
- Parser streaming HDLC zero-heap, validasi CRC-16 HCS/FCS, dan resiliensi transparent flag `0x7E`.
- Implementasi otentikasi HLS 5 (GMAC) dan APDU Ciphering Suite 0 (AES-128-GCM).
- Rangkaian pengujian host: 9/9 CTest suite lulus 100% dan Docker test harness.
- Status dan acuan standar formal tercatat di [docs/security-rules/PROTOCOL_PROFILE.md](docs/security-rules/PROTOCOL_PROFILE.md).

#### Phase 2 — Integrasi Awal Target & Driver Metrologi
- Implementasi layer abstraksi port (`ami_dlms_port.c`) non-blocking untuk UART/DMA buffer.
- Driver metrologi ADE9000 (`ami_ade9000.c`) untuk pembacaan register daya/energi.
- Gerbang NonSecure Callable (NSC) TrustZone ARM Cortex-M33 dengan validasi `cmse_check_address_range()` di `secure_nsc.c`.
- Konfigurasi partisi hardware Security Attribution Unit (SAU) dan GTZC (TZSC/MPCBB/TZIC) di Secure World.
- Implementasi persisten Invocation Counter & Wear-Leveling Anti-Replay NVM Engine (`ami_nvm_counter.c`) dengan dukungan hardware TAMP Backup Register.
- Provisi Master Key (KEK) unik per-device (UID) dan Remote Key Transfer atomik RFC 3394 AES Key Wrap di Secure World.
- Penegakan kebijakan fail-closed pada firmware target (blokir No-Auth, LLS, dan wrap-around counter `0xFFFFFFFF`).

---

### Spin 2 — Pelengkapan Fitur Produksi & Kesiapan Sertifikasi

#### Phase 3A — Pengembangan Fitur Software Lanjutan [SELESAI / 100% SUDAH]
- `[x]` **Anti-Brute-Force Lockout (Status: SUDAH)**: Tracking kegagalan otentikasi HLS5 (`failed_attempts`), penalti waktu progresif eksponensial (5s, 10s, 20s, 40s), dan hard lockout permanen setelah 5x gagal sesuai SPLN D5.002:2024. Terintegrasi pada DLMS Port (`ami_anti_bruteforce.c`) dan lolos pengujian `AmiAntiBruteforceTest`.
- `[x]` **General Block Transfer (GBT) (Status: SUDAH)**: Fragmentasi dan rekonstruksi APDU xDLMS blok data (`GET-Request-For-Next-Datablock` `0xC0 0x02`, `GET-Response-With-Datablock` `0xC4 0x02`) untuk transfer buffer besar (Load Profile 15-menit, Event Log, Billing, dan Object List) melebihi Max PDU buffer. Terintegrasi pada port & simulator (`ami_dlms_block_transfer.c`) dan lolos pengujian `AmiBlockTransferTest`.
- `[x]` **Ekspansi Data Model COSEM (Status: SUDAH)**: Pemetaan dan aktivasi 49 objek standar PLN pascabayar pada Association View (`ami_cosem_model.c`, `ami_dlms_apdu.c`), mencakup Multi-tarif energi (Tarif 1–4 WBP/LWBP untuk Active Import, Active Export, Reactive Import), Power Quality (Frekuensi jaringan, Power Factor total/L1/L2/L3, THD tegangan total/L1/L2/L3, THD arus total/L1/L2/L3), Daya Sesaat (+P, +Q), dan Demand Limiter (Class 71 Atribut 1–10). Lolos pengujian `AmiCosemTest` & `AmiDlmsApduTest`.
- `[x]` **Disconnect Control Breaker (Class 70) (Status: SUDAH)**: Logika state machine lengkap (*Disconnected*, *Connected*, *Ready_for_Reconnection*), konfigurasi `control_mode` 1 s.d. 6 (default Mode 2 direct remote), method `remote_disconnect` & `remote_reconnect` (plain `0xC3` dan ciphered `0xCB`), serta pemicu manual reconnect dengan Hardware Abstraction Layer (`ami_disconnect_control.c`). Lolos pengujian `AmiDisconnectControlTest`.

#### Phase 3B — Integrasi Hardware Kripto & Target Perangkat Fisik (Perlu Hardware Unit / Tahap Selanjutnya)
- Menghubungkan fungsi NSC ke akselerator hardware SAES/AES-GCM dan TRNG STM32U5 di Secure World.
- Integrasi driver fisik UART/Optical Probe DMA dengan FreeRTOS worker task.
- Integrasi task FreeRTOS akuisisi register energi riil melalui komunikasi SPI fisik dengan IC ADE9000.

#### Phase 4 — Sertifikasi, Conformance, dan Handoff
- Pengujian Conformance resmi menggunakan DLMS UA CTT.
- Uji interoperabilitas penuh dengan Head-End System (HES) PLN dan Gurux DLMSDirector.
- Uji stabilitas jangka panjang (*soak test*), pemadaman mendadak (*power-loss resilience*), dan audit tamper.
- Konfigurasi Option Bytes Readout Protection (RDP Level 2) dan Secure Boot.

---

## Panduan Menjalankan Pengujian

### 1. Menjalankan Unit Test (CTest Host Native)

Dari terminal PowerShell di root repositori:

```powershell
cmake -S tests -B tests/build -G Ninja
cmake --build tests/build --parallel
ctest --test-dir tests/build --output-on-failure
```

*Seluruh 14 test suite (`AmiCosemTest`, `AmiSecurityTest`, `AmiDlmsAssociationTest`, `AmiFirmwareAuthPolicyTest`, `AmiDlmsApduTest`, `AmiDlmsHdlcTest`, `AmiDlmsPortTest`, `AmiAde9000Test`, `AmiDlmsCipherTest`, `AmiNvmCounterTest`, `AmiKeyTransferTest`, `AmiAntiBruteforceTest`, `AmiBlockTransferTest`, `AmiDisconnectControlTest`) akan dieksekusi dengan hasil 100% PASS.*

### 2. Menjalankan Simulator Host untuk Gurux DLMSDirector

Kompilasi dan jalankan simulator TCP DLMS:

```powershell
cmake -S tests -B tests/build -G Ninja
cmake --build tests/build --target gurux_simulator
.\tests\build\host-tools\gurux_simulator.exe
```

Simulator akan mendengarkan koneksi raw HDLC-over-TCP pada `127.0.0.1:4059`. Buka software **GXDLMSDirector**, buka workspace [`STM32U585VIT6_SM.gxc`](STM32U585VIT6_SM.gxc), dan klik **Connect**.

### 3. Menjalankan Docker Test Suite Terisolasi

Eksekusi script test otomatis berbasis Docker Compose:

```powershell
.\docker_test\run_docker_tests.bat
```

Atau melalui direktori `docker_test/`:

```powershell
cd docker_test
docker compose up --build --abort-on-container-exit
```

---

## Dokumen Acuan Keamanan dan Standar

Seluruh perancangan protokol dan keamanan sistem smart meter ini merujuk pada dokumen standar yang tersimpan pada folder `docs/security-rules/` dan `dokumen_PLN/STANDAR_PLN/`:
- [`docs/security-rules/PROTOCOL_PROFILE.md`](docs/security-rules/PROTOCOL_PROFILE.md): Spesifikasi baseline protokol dan 13 objek Association View.
- [`docs/security-rules/SECURITY_ARCHITECTURE.md`](docs/security-rules/SECURITY_ARCHITECTURE.md): Arsitektur partisi TrustZone dan tanggung jawab lapisan.
- [`docs/security-rules/SECURITY_CHECKLIST.md`](docs/security-rules/SECURITY_CHECKLIST.md): Daftar periksa kesiapan keamanan rilis.
- [`docs/security-rules/SECURITY_REQUIREMENTS.md`](docs/security-rules/SECURITY_REQUIREMENTS.md): Persyaratan mandatori keamanan sistem.
- [`docs/security-rules/Threat_MODEL.md`](docs/security-rules/Threat_MODEL.md): Analisis model ancaman (T1 s.d. T12).
- [`docs/ChangeLOG.md`](docs/ChangeLOG.md): Riwayat lengkap rilis dan perubahan fitur.
