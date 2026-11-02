# STM32U585VIT6 + ADE9000 3-Phase AMI Smart Meter

Firmware proyek meter listrik pintar pascabayar 3-fasa dengan target mikrokontroler STM32U585VIT6 (ARM Cortex-M33 TrustZone) dan metering IC ADE9000. Repository berisi implementasi lengkap protokol komunikasi DLMS/COSEM (IEC 62056 / SPLN D3.006:2021), cipher suite keamanan (SPLN D5.002:2024), driver metrologi ADE9000, host tests, serta simulator terintegrasi untuk pengujian protokol.

## Ringkasan Pekerjaan E2: Protokol dan Data Model

Fokus pekerjaan E2 adalah menyediakan komunikasi meter melalui DLMS/COSEM dan menyajikan data meter sebagai objek COSEM yang konsisten, aman, dan mematuhi regulasi PLN:

### Cakupan E2

- **Protokol DLMS/COSEM & ACSE:** Pemrosesan asosiasi AARQ/AARE, negosiasi conformance dinamis (`Get` + `SelectiveAccess`), Max PDU size negotiation, penanganan status disconnect, dan silent drop sesuai standar CTT.
- **HDLC Data Link Layer:** Parser streaming deterministik zero-heap $O(1)$, framing Frame Format Type 3 (`0xA0`), dual-stage CRC-16 CCITT (HCS & FCS) dengan resiliensi transparent flag `0x7E`, stripping/auto-wrapping LLC sublayer (`0xE6 0xE6 0x00` / `0xE6 0xE7 0x00`), dan negosiasi parameter SNRM/UA.
- **Data Model COSEM & Katalog PLN:** Pemetaan katalog lengkap 276 kode OBIS standar PLN pascabayar (`ami_obis_catalog.h`) ke dalam 22 kategori fungsional, serta implementasi aktif 13 objek esensial dalam Association View yang siap diakses via xDLMS `GET`.
- **Profile Generic & Selective Access:** Implementasi buffer histori Class 7 untuk Event Log, Billing Profile, dan Load Profile 15-menit (12 kanal daya/tegangan/arus) dengan dukungan Selector 1 (DateTime Range) dan Selector 2 (Entry Index Range).
- **Keamanan Protokol & APDU Ciphering:**
  - Penegakan fail-closed policy: Firmware menolak No-Auth, LLS, HLS3/MD5, HLS4/SHA-1, dan challenge bukan 16 byte.
  - DLMS Security Suite 0 (`glo-get`, `glo-set`, `glo-action`) dengan enkripsi terotentikasi AES-128-GCM, Security Control `0x30`, dan 12-byte Nonce/IV.
  - Perlindungan anti-replay: Invocation Counter strictly monotonic (`rx_counter > last_rx_counter`) dan penolakan otomatis counter duplikat/stale.
  - Mutual 4-pass authentication HLS 5 (GMAC) OID `2.16.756.5.8.2.5` dengan constant-time comparison dan zeroization memori kerja (`secure_zeroize`).
  - Isolasi API raw-key: Firmware NonSecure hanya mengenal referensi aman `AmiKeyHandle`.
- **Driver Metrologi ADE9000:** Pemetaan register SPI 3-Fasa (RMS, Daya Aktif/Reaktif, Energi, Periode/Frekuensi, Status) dan feeder snapshot otomatis ke register DLMS/COSEM.
- **Verifikasi Host & Docker:** 9/9 suite CTest host lulus 100% serta automated Docker containerized test runner (Alpine 23 MB, 0 CVE) dengan simulasi client Python E2E.

### Batas Tanggung Jawab dan Status Integrasi

E2 menangani kontrak data antara sumber data meter dan protokol: objek/OBIS yang disediakan, format nilai, akses, dan perilaku respons. Pembacaan register fisik SPI ADE9000 pada PCB nyata, kalkulasi akurasi metrologi Kelas 0.2S/0.5S, kalibrasi, serta driver UART fisik/optik menjadi tanggung jawab integrasi hardware/metrologi.

## Matriks Status Pekerjaan Proyek (Sudah vs Belum Dikerjakan)

Tabel berikut merangkum status implementasi sistem Smart Meter 3-Fasa berdasarkan pembagian fase pengembangan **Spin 1 (Prototipe / Purwarupa)** dan **Spin 2 (Target Hardware Produksi & Sertifikasi)** sesuai standar **SPLN D3.006:2021**, **SPLN D5.002:2024**, dan **IEC 62056**:

| Area / Komponen | Item Pekerjaan | Status | Catatan / Lokasi Implementasi |
|---|---|:---:|---|
| **Protokol HDLC** | HDLC Framing Type 3 (`0xA0`), parsing streaming O(1), addressing multi-octet | **SUDAH** | Terverifikasi di `NonSecure/AMI/Src/ami_dlms_hdlc.c` & `test_ami_dlms_hdlc.c` |
| **Protokol HDLC** | Dual-stage CRC-16 (HCS & FCS) dan resiliensi transparent flag `0x7E` | **SUDAH** | Teruji via frame riil Gurux dengan checksum berakhiran `0x7E 0x0A 0x7E` |
| **Protokol HDLC** | Stripping/auto-wrapping LLC sublayer (`0xE6 0xE6 0x00` / `0xE6 0xE7 0x00`) | **SUDAH** | Memenuhi kepatuhan IEC 62056-46 pada I-Frame APDU |
| **Protokol ACSE** | State machine AARQ/AARE, negosiasi conformance dinamis, Max PDU | **SUDAH** | Terverifikasi di `NonSecure/AMI/Src/ami_dlms_association.c` |
| **Data Model COSEM** | Pemetaan katalog lengkap 276 kode OBIS standar PLN pascabayar | **SUDAH** | Didefinisikan dalam 22 kategori di `NonSecure/AMI/Inc/ami_obis_catalog.h` |
| **Data Model COSEM** | Implementasi Association View terdaftar (13 objek esensial) via `GET` | **SUDAH** | 13 objek terverifikasi (termasuk Load Profile 15-menit `1.0.99.1.0.255`) |
| **Data Model COSEM** | Selective Access Class 7 (rentang waktu Clock & rentang entri baris) | **SUDAH** | Selector 1 & 2 terimplementasi untuk Load Profile, Billing, dan Event Log |
| **Keamanan Host** | Core Kriptografi mandiri AES-128 NIST FIPS 197 & GHASH GF(2^128) | **SUDAH** | Lolos 100% Known Answer Test (KAT) NIST SP 800-38D Case 1, 2, 4 |
| **Keamanan Host** | 4-Pass Mutual Authentication HLS 5 (GMAC) OID `2.16.756.5.8.2.5` | **SUDAH** | Constant-time verification & zeroization di `ami_security.c` |
| **Keamanan Port** | Penegakan fail-closed policy (blokir No-Auth & LLS pada build firmware) | **SUDAH** | `#if !defined(STM32U585xx)` mengisolasi helper development |
| **Keamanan Port** | Validasi alamat peer HDLC firmware (Client SAP 16, Server Address 1) | **SUDAH** | Menolak frame asing sebelum masuk ke stack APDU (`ami_dlms_port.c`) |
| **Keamanan Port** | Isolasi API raw-key kriptografi dari kompilasi NonSecure firmware | **SUDAH** | NonSecure firmware hanya mengenal opaque reference `AmiKeyHandle` |
| **Keamanan APDU** | DLMS Security Suite 0 Ciphered APDU Engine (`0xC8`, `0xC9`, `0xCB`) | **SUDAH** | Terverifikasi di `NonSecure/AMI/Src/ami_dlms_cipher.c` & `test_ami_dlms_cipher.c` |
| **Keamanan APDU** | Anti-Replay RX Counter Monotonik & Monotonic TX Counter Allocation | **SUDAH** | Menolak replay frame seketika (`AMI_DLMS_APDU_COUNTER_REJECTED`) |
| **Metrologi Driver** | Driver Metrologi ADE9000 SPI Register Map & Feeder Snapshot | **SUDAH** | Terverifikasi di `NonSecure/AMI/Src/ami_ade9000.c` & `test_ami_ade9000.c` |
| **TrustZone Gateway** | Gerbang NonSecure Callable (NSC) `CMSE_NS_ENTRY` stubs di Secure World | **SUDAH** | Dideklarasikan di `Secure_nsclib/secure_nsc.h` & `Secure/Core/Src/secure_nsc.c` |
| **Testing & Simulasi** | CTest Host Native Unit Tests (10/10 suite 100% Passed) | **SUDAH** | Menjalankan pengujian regresi deterministik via Ninja |
| **Testing & Simulasi** | Simulator TCP Host (`gurux_simulator.exe` port 4059) & E2E Docker Runner | **SUDAH** | Docker Compose terpusat di `docker_test/` (Alpine 23 MB, 0 CVE) |
| **Testing & Simulasi** | Profil Workspace Gurux DLMSDirector XML (`STM32U585VIT6_SM.gxc`) | **SUDAH** | Tersinkronisasi dengan seluruh parameter objek PLN |
|---|---|---|---|
| **Hardware TrustZone** | Implementasi driver hardware SAES/AES-GCM di Secure World (`Secure/`) | **BELUM** | Menghubungkan fungsi NSC ke peripheral SAES hardware STM32U5 |
| **Key Management** | Provisi pabrik Master Key (KEK) unik per-device di Secure Storage/OTP | **BELUM** | Standar SPLN D5.002:2024 mewajibkan KEK terlindungi di Secure World |
| **Key Management** | Remote Key Transfer RFC 3394 AES Key Wrap atomik di Secure World | **BELUM** | Eksekusi unwrap dan rotasi kunci GUEK/AK di dalam TrustZone Secure |
| **Hardware Security** | Integrasi Hardware TRNG fisik STM32U5 (NIST SP 800-90B / AIS-31) | **BELUM** | Menghasilkan Server Challenge 16-byte berbasis entropi acak fisik |
| **Anti-Replay NVM** | Monotonic Invocation Counter persisten & wear-leveling NVM/TAMP Backup | **SUDAH** | Terverifikasi di NonSecure/AMI/Src/ami_nvm_counter.c & 	est_ami_nvm_counter.c |
| **Anti-Replay NVM** | Penanganan saturasi Invocation Counter (`0xFFFFFFFF` fail-closed) | **BELUM** | Wajib menghentikan enkripsi & memicu rotasi kunci bila IC habis |
| **Anti-Brute-Force** | Progressive exponential backoff / lockout setelah N kali gagal HLS | **BELUM** | Mencegah serangan brute-force dictionary attack pada port komunikasi |
| **Hardware Driver** | Integrasi physical UART/Optical Probe DMA Circular Buffer | **BELUM** | `ami_dlms_hardware_uart_tx()` masih berupa stub non-hardware |
| **Metrologi ADE9000** | Task FreeRTOS akuisisi register energi/tegangan riil via SPI ADE9000 | **BELUM** | Saat ini data tegangan/arus masih diisi oleh test sample bank |
| **Data Model Lanjutan** | Ekspansi objek Association View (seluruh 276 OBIS katalog PLN) | **BELUM** | Memetakan sisa OBIS (multi-tarif, THD, daya semu, limiter) ke snapshot |
| **Kontrol Beban** | Eksekusi fisik Disconnect Control Breaker (Class 70) via GPIO Latching Relay | **BELUM** | Method `remote_disconnect/reconnect` belum memicu GPIO koil relay |
| **Protokol Lanjutan** | General Block Transfer untuk unduhan data profil historis berukuran besar | **BELUM** | Diperlukan jika paket data melebihi Max PDU buffer (Class 7 log) |
| **Keamanan Boot** | Secure Boot (SBSFU/MCUboot) & Verifikasi Digital Signature FOTA | **BELUM** | Memvalidasi integritas dan rollback counter sebelum eksekusi image |
| **Keamanan Fisik** | Penguncian Readout Protection (RDP Level 1/2) & penonaktifan JTAG/SWD | **BELUM** | Mencegah dumping firmware dan memori kunci melalui debugger fisik |
| **Sertifikasi** | Uji formal DLMS User Association Conformance Test Tool (CTT) & HES PLN | **BELUM** | Sertifikasi kepatuhan penuh standar utilitas sebelum komersialisasi |

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
- Gerbang NonSecure Callable (NSC) TrustZone ARM Cortex-M33 disiapkan di `secure_nsc.c`.
- Penegakan kebijakan fail-closed pada firmware target (blokir No-Auth dan LLS).

---

### Spin 2 — Pelengkapan Fitur Produksi & Kesiapan Sertifikasi (Tahap Selanjutnya)

#### Phase 3 — Integrasi Hardware TrustZone & Keamanan Produksi
- Menghubungkan fungsi NSC ke akselerator hardware SAES/AES-GCM dan TRNG STM32U5 di Secure World.
- Provisi Master Key (KEK) unik per-device pada Secure Flash / OTP.
- Implementasi unwrap RFC 3394 AES Key Wrap untuk remote key transfer atomik di Secure World.
- Implementasi algoritma wear-leveling flash internal untuk persistensi Invocation Counter anti-replay.
- Integrasi driver fisik UART/Optical Probe DMA dengan FreeRTOS worker task.

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

*Seluruh 9 test suite (`AmiCosemTest`, `AmiSecurityTest`, `AmiDlmsAssociationTest`, `AmiFirmwareAuthPolicyTest`, `AmiDlmsApduTest`, `AmiDlmsHdlcTest`, `AmiDlmsPortTest`, `AmiAde9000Test`, `AmiDlmsCipherTest`) akan dieksekusi.*

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
