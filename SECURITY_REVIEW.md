<<<<<<< HEAD
# Tinjauan Keamanan Komprehensif — STM32U585VIT6_SM (`v2.21.0`)

| Metadata | Keterangan |
|---|---|
| **Target Proyek** | Firmware 3-Phase AMI Smart Meter (STM32U585VIT6 Cortex-M33 + ADE9000) |
| **Versi Baseline** | `v2.21.0` (100% Pure C11, Prototype Litbang PLN) |
| **Tanggal Audit Awal** | 3 November 2026 |
| **Pembaruan & Adendum 1** | 4 November 2026 (Mitigasi awal C2, C3, I1, I3, I4) |
| **Remediasi Tahap 1** | 16 November 2026 (`v2.21.0`, commit `64e52c7` — F1/F2/C1/I5/A1/RDP) |
| **Adendum Audit 2** | 16 November 2026 (`v2.21.0`, commit `e1923bd` — Temuan A2, A3, A4, C4, I7, P1) |
| **Perbaikan & Traceability** | 16 November 2026 (`v2.21.0` — Penutupan A2, A3, A4, C4, I7, P1 & sinkronisasi dokumen) |
| **Cakupan** | Tinjauan statis source code (Secure/NonSecure), konfigurasi build, proteksi TrustZone-M, dan uji host CTest |
| **Di Luar Cakupan** | Pengujian penetrasi jaringan fisik/remote live, injeksi kesalahan fisik (fault injection/side-channel), dan validasi register hardware di board target |

---

## 1. Jejak Kronologis Audit & Pembaruan (*Audit Traceability Timeline*)

| Tanggal | Tahap / Commit | Ringkasan Aktivitas & Hasil |
|---|---|---|
| **3 Nov 2026** | Audit Awal (`b4f6702` baseline) | Identifikasi 3 *Key Findings* (**F1/I2**, **F2/I6**, **F3/I3**) serta pemetaan risiko CIA (**C1–C3**, **I1–I6**, **A1**). |
| **4 Nov 2026** | Pembaruan & Adendum 1 | Penambahan guard awal pada `C2` (`AmiKeyHandle`), `C3` (MPCBB SRAM3), `I1` (HLS5 NSC), `I3` (counter guard), dan `I4` (TRNG fail-closed tanpa LCG). |
| **16 Nov 2026** | Remediasi `v2.21.0` (`64e52c7`) | Implementasi 100% Pure C untuk `C1` (`FLASH_KEYSTORE` @ `0x0C0FC000`), `I5` (*copy-then-validate* NSC), `I6/F2` (SHA-256 + ECDSA P-256 Secure Boot), `A1` (`IWDG` ~6s + reset), dan opt-in RDP Level 1. |
| **16 Nov 2026** | Adendum Audit 2 (`e1923bd`) | Audit statis pasca-`64e52c7` menemukan 6 celah lanjutan: **A2** (aksi breaker plaintext), **A3** (otorisasi provisi/SAP 0), **A4** (strict boot non-Release), **C4** (KDF keystore berbasis XOR UID), **I7** (`version_epoch` tidak tertandatangani), dan **P1** (jalur HLS normal tidak memanggil `SECURE_DLMS_HLS5_Verify`). |
| **16 Nov 2026** | Perbaikan Lanjutan (`v2.21.0`) | Penutupan seluruh temuan Adendum Audit 2 (**A2, A3, A4, C4, I7, P1**), penambahan uji regresi di `AmiDisconnectControlTest` & `AmiBootCryptoTest` (**16/16 CTest PASS**), dan sinkronisasi nomor baris. |

---

## 2. Matriks Traceability Status Temuan Terpadu

| ID | Tanggal Ditemukan | Area / Kategori | Severity | Status Source Code (`v2.21.0`, 16 Nov 2026) | Lokasi Kode Terbaru | Verifikasi & Catatan Hardware |
|:---:|:---:|---|:---:|---|---|---|
| **C1** | 3 Nov 2026 | Persistensi Kunci Operasional | **HIGH** | **Dimitigasi** (`64e52c7`) | `secure_nsc.c:200–315` | Terenkripsi AES-128-GCM di Page 126 Secure Flash (`0x0C0FC000`). *Perlu uji power-cycle di board.* |
| **C2** | 3 Nov 2026 | Kunci Mentah di NonSecure | **HIGH** | **Dimitigasi** (4 Nov) | `ami_dlms_cipher.c`, `ami_dlms_cipher.h` | NonSecure hanya memakai `AmiKeyHandle` & NSC gateway. |
| **C3** | 3 Nov 2026 | Atribut Keamanan SRAM3 | **HIGH\*** | **Dimitigasi di source** (4 Nov) | `Secure/Core/Src/main.c`, `STM32U585xx_FLASH_s.ld` | Bitmask MPCBB/GTZC SRAM3 `0xFFFFFFFF`. *Perlu verifikasi register di board.* |
| **C4** | 16 Nov 2026 | Wrapping Key Keystore | **MEDIUM** | **Dimitigasi** (16 Nov `v2.21.0`) | `secure_nsc.c:152–177` | KDF ditingkatkan ke **SHA-256** (`UID_BASE \|\| SECURE_INTERNAL_STORAGE_SECRET`). *Produksi: ganti secret unik per-chip + RDP.* |
| **I1** | 3 Nov 2026 | Proof HLS-5 & Disconnect | **HIGH\*** | **Dimitigasi** (4 Nov) | `secure_nsc.c:1123–1177` | `SECURE_DLMS_HLS5_CalculateProof()` & `Verify` menolak proof nol/salah (*constant-time*). |
| **I2 / F1** | 3 Nov 2026 | Otorisasi Key-Management | **HIGH** | **Dimitigasi** (16 Nov `v2.21.0`) | `secure_nsc.c:739–760` | Wajib `s_hls_authenticated_session == 1` & SAP `1` atau `16`; reset via `SECURE_DLMS_ResetSession()`. |
| **I3 / F3** | 3 Nov 2026 | Persistensi Invocation Counter | **MEDIUM** | **Dimitigasi di source** (4 Nov) | `ami_nvm_counter.c:69–127` | Strictly monotonic & fail-closed. **Catatan HW:** Akses TAMP langsung dari NonSecure perlu uji GTZC di board. |
| **I4** | 3 Nov 2026 | RNG Fail-Closed | **MEDIUM** | **Dimitigasi** (4 Nov) | `secure_nsc.c:179–210, 385–425` | Fallback LCG dihapus; error TRNG mengembalikan `-2` dan men-zeroize buffer. |
| **I5** | 4 Nov 2026 | Descriptor NSC / TOCTOU | **MEDIUM** | **Dimitigasi** (`64e52c7`) | `secure_nsc.c:722–755, 1185–1285` | *Copy-then-validate* penuh ke Secure SRAM lokal; teruji di `AmiTrustZoneBoundaryTest`. |
| **I6 / F2** | 3 Nov 2026 | Verifikasi Image & Rollback | **HIGH\*** | **Dimitigasi** (`64e52c7`) | `Secure/Core/Src/main.c:135–218` | Verifikasi SHA-256 + **ECDSA P-256** (`ami_boot_crypto.c`) + `TAMP->BKP0R`; teruji di `AmiBootCryptoTest`. |
| **I7** | 16 Nov 2026 | Cakupan Tanda Tangan Epoch | **MEDIUM** | **Dimitigasi** (16 Nov `v2.21.0`) | `Secure/Core/Src/main.c:169–177`, `ami_sign_tool.c:112–118` | 16-byte header manifest (`magic, version_epoch, image_size, flags`) ikut di-hash & ditandatangani ECDSA P-256. |
| **A1** | 3 Nov 2026 | Pemulihan Error & Watchdog | **MEDIUM** | **Dimitigasi** (`64e52c7`) | `Secure/Core/Src/main.c:329–386`, `app_freertos.c:110–116` | `MX_IWDG_Init()` (~6s, *debug-freeze* ST-LINK) + `SECURE_Watchdog_Kick()`; `Error_Handler()` mencatat `TAMP->BKP1R` & reset. |
| **A2** | 16 Nov 2026 | Aksi Breaker Plaintext | **HIGH** | **Dimitigasi** (16 Nov `v2.21.0`) | `ami_dlms_port.c:205–213` | `Class 70` plaintext (`0xC3`) ditolak (`0x03`) saat `mechanism == AMI_DLMS_AUTH_HLS`; teruji di `AmiDisconnectControlTest`. |
| **A3** | 16 Nov 2026 | Provisi Token & SAP `0` | **HIGH** | **Dimitigasi** (16 Nov `v2.21.0`) | `secure_nsc.c:415–470, 745–755` | SAP `0` kini ditolak pada `SECURE_DLMS_TransferKeys_Atomic()`; token provisi tetap hanya untuk tahap prototype/Litbang. |
| **A4** | 16 Nov 2026 | Strict Boot pada Build Rilis | **HIGH** | **Dimitigasi** (16 Nov `v2.21.0`) | `Secure/CMakeLists.txt:47–50` | `AMI_STRICT_SECURE_BOOT=1` aktif otomatis pada konfigurasi `Release` (`Debug` tetap *permissive* untuk ST-LINK Litbang). |
| **P1** | 16 Nov 2026 | Jalur Verify HLS di `ami_dlms_port` | **MEDIUM** | **Dimitigasi** (16 Nov `v2.21.0`) | `ami_dlms_port.c:143–165` | `ami_dlms_port.c` kini memanggil `SECURE_DLMS_SetSessionSap(16U)` & `SECURE_DLMS_HLS5_Verify()` saat asosiasi HLS5 sukses. |

*\* Severity berlaku jika jalur komunikasi atau fitur terkait aktif dan dapat dicapai dari antarmuka luar.*

---

## 3. Ringkasan Detail Implementasi Keamanan (`v2.21.0`)

### A. Confidentiality (Kerahasiaan)
* **Secure Flash Keystore (`C1`, `C4`)**: `SECURE_KeyStorageTypeDef` disimpan terenkripsi AES-128-GCM pada Page 126 Secure Flash (`0x0C0FC000`–`0x0C0FDFFF`, terpisah dari `ROM_NSC` `0x0C0FE000`). Kunci pembungkus diturunkan lewat `SHA-256(UID_BASE || SECURE_INTERNAL_STORAGE_SECRET)`.
* **Isolasi TrustZone (`C2`, `C3`)**: Kunci mentah tidak pernah berada di NonSecure RAM; SRAM3 dikunci sebagai memori Secure/Privileged.

### B. Integrity & Authentication (Integritas dan Autentikasi)
* **Anti-Bypass Breaker & Key-Transfer (`A2`, `A3`, `F1/I2`, `P1`)**:
  * Perintah *Disconnect Control* (Class 70) plaintext (`0xC3`) ditolak ketika sesi memakai `AMI_DLMS_AUTH_HLS`, memaksa penggunaan `GLO-ACTION-Request` (`0xCB`) terenkripsi AES-128-GCM.
  * Alur HLS5 target memanggil `SECURE_DLMS_SetSessionSap(16U)` dan `SECURE_DLMS_HLS5_Verify()`, dan `SECURE_DLMS_TransferKeys_Atomic()` menolak SAP selain `1` atau `16`.
* **Anti-TOCTOU (`I5`)**: `SECURE_DLMS_AES_GCM_Process()` dan `SECURE_DLMS_TransferKeys_Atomic()` menyalin seluruh struct dan input NonSecure ke Secure SRAM lokal terlebih dahulu sebelum validasi dan pemrosesan, diikuti `secure_memzero()`.
* **Cryptographic Secure Boot (`F2/I6`, `I7`, `A4`)**: `SECURE_VerifyNonSecureImage()` memvalidasi batas SRAM NonSecure (`0x20040000`–`0x200C0000`), menghitung SHA-256 kanonikal yang mencakup 16-byte header manifest (`magic`, `version_epoch`, `image_size`, `flags`) + kode biner, memverifikasi tanda tangan **ECDSA P-256** (`ami_boot_crypto.c`), dan menegakkan *anti-rollback* via `TAMP->BKP0R`.

### C. Availability (Ketersediaan)
* **Watchdog & Fault Recovery (`A1`)**: `MX_IWDG_Init()` (timeout ~6 detik, *freeze* saat *breakpoint* debugger ST-LINK aktif) di-refresh oleh `SECURE_Watchdog_Kick()` pada `StartDefaultTask()`, sedangkan `Error_Handler()` mencatat fault ke `TAMP->BKP1R` dan memicu `NVIC_SystemReset()`.

---

## 4. Catatan Validasi & Batas Pengujian Hardware

1. **Verifikasi Otomatis Host & Cross-Compile (`16 November 2026`, `v2.21.0`)**:
   * **Host CTest**: **16/16 test suites PASS (100%)** (`AmiBootCryptoTest`, `AmiTrustZoneBoundaryTest`, `AmiDisconnectControlTest`, dan 13 suite lainnya).
   * **ARM Cortex-M33 Cross-Compile (`arm-none-eabi-gcc`)**: `STM32U585VIT6_SM_S` dan `STM32U585VIT6_SM_NS` terkompilasi bersih (**0 error**).
2. **Item yang Tetap Memerlukan Uji Langsung pada Board Fisik STM32U585**:
   * **Akses `TAMP` untuk Invocation Counter (`ami_nvm_counter.c`)**: `Secure/Core/Src/main.c` menandai `TAMP` sebagai peripheral Secure di GTZC (`HAL_GTZC_TZIC_EnableIT(GTZC_PERIPH_TAMP)`), sementara driver counter NonSecure mengakses register `TAMP` langsung. Wajib diuji di board target atau dipindahkan lewat NSC gateway jika memicu `SecureFault`.
   * **Retensi Daya & Cold-Boot Keystore**: Verifikasi `FLASH_KEYSTORE` (`0x0C0FC000`) setelah *power-loss* fisik.
   * **Opt-In RDP Level 1 (`AMI_LITBANG_RDP_LEVEL1=1`)**: Verifikasi blokir pembacaan ST-LINK saat uji keamanan fisik di Litbang PLN (default `0` selama pengembangan).
=======
# Tinjauan Keamanan — STM32U585VIT6_SM

| Informasi | Keterangan |
|---|---|
| Tinjauan awal | 3 November 2026 |
| Pembaruan status source | 4 November 2026 |
| Cakupan | Tinjauan statis source code, konfigurasi build, dan dokumentasi |
| Di luar cakupan | Pengujian perangkat/jaringan nyata dan eksekusi exploit |

## Ringkasan untuk pembaca

**Status: belum dapat dinyatakan aman atau siap produksi.** Laporan ini
mencatat temuan dari tinjauan awal serta pembaruan source yang dilaporkan
kemudian. Beberapa perbaikan source sudah dicatat, tetapi persistensi kunci
dan counter, otorisasi Secure, pemeriksaan descriptor, serta verifikasi
hardware dan rantai boot masih perlu dituntaskan.

Temuan source-level **bukan bukti bahwa serangan jarak jauh dapat dilakukan**.
Jangkauan serangan bergantung pada integrasi firmware, konfigurasi perangkat,
dan jalur komunikasi yang aktif; hal-hal tersebut belum diverifikasi pada
hardware.

### Status temuan terbaru

Status di bawah merangkum adendum 4 November. Detail dan bukti setiap temuan
ada pada bagian [Temuan audit awal](#temuan-audit-awal-3-november-2026).

| ID | Area | Severity awal | Status setelah pembaruan source |
|---|---|---:|---|
| C1 | Kunci default dan persistensi | HIGH | **Sebagian ditangani.** Kunci default dihapus, tetapi persistensi dan lifecycle kunci belum terjamin. |
| C2 | Representasi kunci di NonSecure | HIGH | **Belum ditangani** menurut adendum. |
| C3 | Atribut keamanan SRAM3 | HIGH* | **Source diperbarui; hardware belum diverifikasi.** |
| I1 | Proof HLS | HIGH* | **Perbaikan source dilaporkan.** Perlu verifikasi integrasi dan fail-closed. |
| I2 | Otorisasi key-management | HIGH | **Sebagian ditangani.** Token publik dihapus; otorisasi sesi masih perlu diperkuat. |
| I3 | Persistensi invocation counter | MEDIUM | **Belum tuntas.** Kegagalan storage dan integrasi end-to-end masih perlu diuji. |
| I4 | RNG fail-closed | MEDIUM | **Perbaikan source dilaporkan;** kualitas dan kegagalan hardware belum diuji. |
| I5 | Descriptor NSC / TOCTOU | MEDIUM | **Belum ditangani** menurut adendum. |
| I6 | Verifikasi image dan rollback | HIGH* | **Sebagian ditangani.** Pemeriksaan manifest ditambahkan; rantai boot dan signature belum diverifikasi. |
| A1 | Error recovery dan ketersediaan | Belum dinilai | **Belum diverifikasi.** DoS remote tidak terkonfirmasi. |

\* Severity berlaku jika jalur atau kondisi terkait dapat dicapai. Confidence
menunjukkan keyakinan terhadap analisis source, bukan kepastian exploitability.

### Daftar isi

- [Temuan audit awal](#temuan-audit-awal-3-november-2026)
  - [Confidentiality](#confidentiality--kerahasiaan)
  - [Integrity](#integrity--integritas-dan-autentikasi)
  - [Availability](#availability--ketersediaan)
- [Batas verifikasi](#status-integrasi-dan-batas-verifikasi)
- [Tindak lanjut yang masih diperlukan](#tindak-lanjut-yang-masih-diperlukan)
- [Pembaruan source 4 November](#pembaruan-source-level-4-november-2026)

Severity menggambarkan dampak potensial jika jalur terkait aktif. Confidence
menggambarkan tingkat keyakinan analisis source, bukan kepastian bahwa serangan
dapat dilakukan dari jaringan.

## Temuan audit awal (3 November 2026)

> Status pada bagian ini merekam kondisi saat tinjauan awal. Gunakan tabel
> **Status temuan terbaru** di atas dan bagian pembaruan di bawah untuk melihat
> perubahan source yang dilaporkan pada 4 November.

### Confidentiality — Kerahasiaan

#### C1. Kunci operasional memiliki default yang diketahui dan tidak persisten

- **Severity:** HIGH — **Confidence:** 9/10
- **Bukti:** [`secure_nsc.c`](./Secure/Core/Src/secure_nsc.c#L43) dan
  [`secure_nsc.h`](./Secure_nsclib/secure_nsc.h#L49).
- **Masalah/dampak:** GUEK, GBEK, dan AK diinisialisasi dengan nilai tetap yang
  ada di source. KEK fallback diturunkan dari UID perangkat dan salt tetap;
  UID juga tersedia melalui API NSC. Kunci serta status provisioning berada
  di RAM, bukan penyimpanan persisten. Reset dapat menghilangkan status
  provisioning dan mengembalikan default. Jika digunakan untuk trafik nyata,
  kerahasiaan dan autentikasi dapat dikompromikan.
- **Status:** Masih ada; klaim bahwa kunci disimpan persisten belum didukung
  implementasi.
- **Validasi aman:** Audit source dan uji lifecycle memakai kunci/UID dummy.
  Jangan membaca atau mengekspor kunci dari perangkat produksi.

#### C2. Kunci mentah masih memiliki representasi di NonSecure

- **Severity:** HIGH — **Confidence:** 9/10
- **Bukti:** [`ami_dlms_cipher.h`](./NonSecure/AMI/Inc/ami_dlms_cipher.h#L16)
  dan [`ami_dlms_cipher.c`](./NonSecure/AMI/Src/ami_dlms_cipher.c#L254).
- **Masalah/dampak:** Konteks Suite 0 mempunyai field kunci mentah dan fungsi
  inisialisasi menyalinnya ke memori NonSecure. Bila kunci produksi dimasukkan
  ke konteks ini, kompromi NonSecure dapat membocorkan kredensial.
- **Status:** Representasi dan jalur salin masih ada. Task saat ini memakai
  nilai dummy; pemakaian kunci operasional tidak terbukti.
- **Validasi aman:** Pastikan API produksi hanya menerima handle opaque dan
  tidak pernah menyalin plaintext key ke NonSecure.

#### C3. SRAM3: konfigurasi sumber diperbaiki, tetapi hardware belum diverifikasi

- **Severity:** HIGH jika atribut aktual berbeda dari konfigurasi yang dimaksud
  — **Confidence:** 9/10
- **Bukti:** [`main.c`](./Secure/Core/Src/main.c#L167) dan
  [`STM32U585xx_FLASH_s.ld`](./Secure/STM32U585xx_FLASH_s.ld#L43).
- **Status:** Perubahan source mengatur security/privilege bitmap SRAM3 ke
  `0xFFFFFFFF`, mengunci MPCBB, dan mengaktifkan deteksi GTZC. Ini menangani
  salah konfigurasi yang dilaporkan sebelumnya pada level source. Belum
  diverifikasi bahwa atribut dan lock efektif pada target STM32U585/board.
- **Validasi aman:** Pada board pengembangan terisolasi, periksa register
  MPCBB/GTZC dan uji akses menggunakan variabel Secure dummy; jangan membaca
  data rahasia.

### Integrity — Integritas dan autentikasi

#### I1. Callback HLS mengembalikan sukses tanpa membuat proof

- **Severity:** HIGH jika HLS/aksi terkait diaktifkan — **Confidence:** 9/10
- **Bukti:** [`ami_dlms_port.c`](./NonSecure/AMI/Src/ami_dlms_port.c#L19)
  dan [`ami_dlms_association.c`](./NonSecure/AMI/Src/ami_dlms_association.c#L423).
- **Masalah/dampak:** Callback memeriksa parameter, mengisi panjang proof,
  lalu mengembalikan sukses tanpa menulis proof. Buffer expected proof
  diinisialisasi nol dan dibandingkan dengan nilai dari peer. Ini membuat
  HLS tidak melakukan verifikasi kriptografi yang semestinya; jika flow dapat
  dicapai, proof nol dapat diterima.
- **Dampak terkait:** Handler kontrol disconnect/reconnect
  ([`ami_disconnect_control.c`](./NonSecure/AMI/Src/ami_disconnect_control.c#L34))
  perlu memastikan otorisasi dan autentikasi pesan, bukan hanya status
  asosiasi, sebelum mengendalikan aktuator.
- **Status/jangkauan:** Cacat source-level. Transport fisik, pemanggilan HLS
  produksi, dan konteks aktuator tidak terhubung/terbukti pada integrasi saat
  ini; ini bukan bukti bypass remote yang sedang dapat dieksploitasi.
- **Validasi aman:** Host test dengan mock proof dan mock aktuator harus
  memastikan proof invalid/zero selalu ditolak dan aksi gagal tertutup.

#### I2. API Secure key-management belum memiliki otorisasi independen yang kuat

- **Severity:** HIGH — **Confidence:** 9/10
- **Bukti:** [`secure_nsc.h`](./Secure_nsclib/secure_nsc.h#L49) dan
  [`secure_nsc.c`](./Secure/Core/Src/secure_nsc.c#L263).
- **Masalah/dampak:** Token provisioning merupakan konstanta publik. State
  provisioning volatil. Transfer kunci memakai flag HLS global, bukan
  otorisasi yang terikat ke asosiasi DLMS tertentu, fresh challenge, dan
  masa berlaku. Verifier menerima challenge dari pemanggil NSC; default AK
  tersedia di source. Pemanggil NonSecure yang dapat memanggil NSC berpotensi
  memanipulasi lifecycle kunci tanpa otorisasi manufaktur/sesi yang kuat.
- **Status:** Sebagian guard ditambahkan (cek provisioned/HLS dan konsumsi flag
  setelah transfer), tetapi kontrol belum aman untuk produksi.
- **Validasi aman:** Uji kontrak otorisasi dengan kunci dummy. Pastikan
  pemanggil NSC tak berwenang ditolak dan sesi tidak dapat dipakai ulang atau
  dibuat dengan challenge pilihan pemanggil.

#### I3. Counter invocation dapat kembali ke nilai lama saat storage bermasalah

- **Severity:** MEDIUM — **Confidence:** 9/10
- **Bukti:** [`ami_nvm_counter.c`](./NonSecure/AMI/Src/ami_nvm_counter.c#L47)
  dan [`ami_dlms_port.c`](./NonSecure/AMI/Src/ami_dlms_port.c#L352).
- **Masalah/dampak:** Record hilang/rusak diperlakukan sebagai storage baru
  dan counter dimulai dari nilai rendah. Kegagalan tulis lease TX terjadi
  setelah state RAM dimajukan; pemanggilan berikutnya bisa menggunakan nilai
  yang belum persisten. Task mengabaikan hasil init/bind. Driver mengakses
  TAMP dari NonSecure sementara TAMP diatribusikan Secure. Bila backend cipher
  diaktifkan, reset/failure dapat menyebabkan replay atau penggunaan ulang
  nonce AES-GCM.
- **Status/jangkauan:** Guard overflow dan sebagian kegagalan tulis sudah
  ditambahkan, tetapi integrasi persistence dan fail-closed end-to-end belum
  terjamin. Task saat ini tidak memasang cipher backend ke port dengan benar.
- **Validasi aman:** Host test dengan storage mock yang menyimulasikan record
  rusak, write failure, reset, dan counter dekat `UINT32_MAX`. Di board
  pengembangan, verifikasi atribusi TAMP dan retensi backup domain.

#### I4. RNG menghasilkan keluaran LCG dan melaporkan sukses saat gagal

- **Severity:** MEDIUM — **Confidence:** 9/10
- **Bukti:** [`secure_nsc.c`](./Secure/Core/Src/secure_nsc.c#L195).
- **Masalah/dampak:** Saat RNG timeout/error, API masih menghasilkan byte dari
  LCG dan mengembalikan sukses. UID/SysTick mixing bukan CSPRNG. Challenge
  autentikasi/nonce yang memakai jalur ini dapat diprediksi.
- **Status:** Masih ada pada source. Kualitas RNG dan error hardware belum
  diuji.
- **Validasi aman:** Mock RNG failure pada host dan pastikan operasi keamanan
  gagal tertutup serta output dibersihkan.

#### I5. Descriptor NonSecure di NSC masih rentan double-fetch/TOCTOU

- **Severity:** MEDIUM — **Confidence:** 8/10
- **Bukti:** [`secure_nsc.c`](./Secure/Core/Src/secure_nsc.c#L933) dan
  [`secure_nsc.c`](./Secure/Core/Src/secure_nsc.c#L535).
- **Masalah/dampak:** Fungsi AES-GCM memvalidasi field melalui pointer
  `params` NonSecure lalu membaca field tersebut lagi untuk memilih key dan
  menjalankan operasi (`secure_nsc.c:933-973`). Transfer kunci juga memeriksa
  rentang descriptor lalu membaca descriptor dan pointer key kembali
  (`secure_nsc.c:535-607`). Jika NonSecure DMA dapat mengubah descriptor di
  antara pemeriksaan dan pemakaian, nilai yang dipakai dapat berbeda dari yang
  divalidasi dan berpotensi melintasi batas Secure/NonSecure.
- **Status:** Masih ada pada source. Eksploitasi bergantung pada kemampuan DMA
  dan atribusi memori aktual; belum dibuktikan di hardware.
- **Perbaikan yang disarankan:** Salin descriptor ke memori Secure terlebih
  dahulu, validasi salinan, lalu gunakan hanya salinan tersebut selama operasi.
- **Validasi aman:** Uji kontrak copy-then-validate melalui review/unit test.
  Jangan membuat race pada perangkat produksi.

#### I6. Pemeriksaan image Secure/NonSecure dan rollback tidak terlihat

- **Severity:** HIGH sebagai kesenjangan kesiapan produksi — **Confidence:** 8/10
- **Bukti:** [`main.c`](./Secure/Core/Src/main.c#L108) membaca vector table
  NonSecure dan berpindah ke reset handler.
- **Masalah/dampak:** Tidak ditemukan pemeriksaan signature atau anti-rollback
  di jalur source tersebut sebelum eksekusi NonSecure. Jika tidak ada
  boot-stage/ROM protection independen, image tidak sah atau versi lama dapat
  berjalan.
- **Status:** Belum diverifikasi. Konfigurasi boot ROM, option bytes, dan
  keseluruhan rantai boot dapat mengubah penilaian.
- **Validasi aman:** Dokumentasikan dan tinjau boot chain serta proses signing;
  verifikasi konfigurasi pada perangkat lab berizin.

### Availability — Ketersediaan (A1)

Belum ada DoS remote yang terkonfirmasi dari tinjauan statis. Namun,
`Error_Handler()` masuk loop tanpa akhir dan watchdog/recovery tidak terlihat
di startup yang ditinjau ([`Secure/Core/Src/main.c`](./Secure/Core/Src/main.c:234),
[`NonSecure/Core/Src/main.c`](./NonSecure/Core/Src/main.c:173)). Kegagalan
inisialisasi dapat membuat layanan tidak pulih sampai reset. Batas resource,
timeout, dan ketahanan terhadap beban/protokol malformed perlu diuji terpisah.

## Status integrasi dan batas verifikasi (tinjauan awal)

Catatan berikut menggambarkan integrasi pada saat tinjauan 3 November 2026.
Untuk pembaruan source yang dilaporkan sesudahnya, lihat bagian
[Pembaruan source-level](#pembaruan-source-level-4-november-2026).

- Firmware menolak no-auth/LLS, tetapi task belum membentuk jalur DLMS
  terautentikasi yang berfungsi penuh. HLS callback bukan implementasi proof.
- Task membuat cipher backend, tetapi tidak memasangnya melalui
  `ami_dlms_port_set_cipher_backend`; hasil init/bind counter juga diabaikan.
- Weak UART transmit callback gagal secara default dan receive hooks belum
  terhubung ke UART fisik. Karena itu, temuan source tidak membuktikan serangan
  jaringan jarak jauh terhadap firmware stock.
- AES/GHASH software tersedia di Secure source; ini tidak membuktikan validasi
  standar, side-channel resistance, atau pemakaian SAES hardware.
- MPCBB/GTZC/TAMP, DMA race, RNG, boot ROM/option bytes, persistensi, dan
  perilaku reset hanya dapat dipastikan pada target. Validasi hardware hanya
  disarankan di board pengembangan terisolasi dengan izin.
- Bagian ChangeLOG/README yang menyatakan semua temuan telah diremediasi,
  kunci persisten/opaque, RNG fail-closed, atau backend telah terintegrasi
  perlu diselaraskan dengan status source di atas.
- Hasil build/test yang tercantum di bagian lama laporan adalah historis;
  tidak dijalankan ulang sebagai bagian audit ini dan bukan bukti keamanan.

## Tindak lanjut yang masih diperlukan

Urutan berikut berfokus pada risiko yang belum selesai atau belum diverifikasi.
Perbaikan yang disebut di bagian pembaruan source tetap memerlukan validasi
integrasi sebelum dianggap tuntas.

1. **Amankan lifecycle kunci (C1, C2, I2).** Hilangkan representasi raw key
   dari build NonSecure produksi; gunakan handle opaque dan operasi Secure.
   Pastikan kunci disimpan secara persisten di storage terlindungi dan operasi
   provisioning terikat pada otorisasi manufaktur/sesi yang tepercaya.
2. **Buat counter fail-closed (I3).** Persistenkan counter; tolak record rusak,
   kegagalan tulis/init/bind, dan overflow tanpa kembali ke nilai rendah atau
   state RAM yang belum tersimpan.
3. **Amankan descriptor NSC (I5).** Salin descriptor ke memori Secure,
   validasi salinan, lalu gunakan hanya salinan tersebut selama operasi.
4. **Verifikasi HLS dan aksi kontrol (I1).** Uji proof valid/tidak valid,
   integrasi cipher backend, serta otorisasi spesifik sebelum aksi kontrol
   beban. Semua kegagalan harus ditolak.
5. **Lengkapi rantai boot (I6).** Tinjau signing, validasi signature,
   anti-rollback, boot ROM, dan option bytes. Pemeriksaan manifest saja belum
   membuktikan image telah diautentikasi secara kriptografis.
6. **Tinjau pemulihan error (A1).** Evaluasi watchdog/recovery, batas resource,
   timeout, dan perilaku terhadap input malformed; DoS remote belum terbukti.
7. **Validasi pada hardware berizin.** Periksa SRAM3, GTZC/MPCBB, TAMP,
   DMA/CMSE, RNG, boot configuration, persistensi, dan pemulihan error pada
   board pengembangan terisolasi.

## Pembaruan source-level (4 November 2026)

Pembaruan berikut dilaporkan setelah tinjauan awal. Dokumen ini mencatat
perubahan pada source dan hasil host test yang dilaporkan (14/14 lulus);
hasil tersebut **bukan** pengganti build firmware target atau validasi
hardware.

| Temuan/area | Perubahan source yang dilaporkan |
|---|---|
| **I1 — HLS dan cipher backend** | Gateway Secure NSC `SECURE_DLMS_HLS5_CalculateProof()` di [`secure_nsc.c`](./Secure/Core/Src/secure_nsc.c) menghitung GMAC server proof 16-byte. Callback `ami_dlms_secure_hls_proof()` di [`ami_dlms_port.c`](./NonSecure/AMI/Src/ami_dlms_port.c) menghasilkan proof; cipher backend dipasang melalui `ami_dlms_port_set_cipher_backend(&s_global_port_ctx, &s_cipher_backend)`. |
| **C1/I2 — Kunci default dan token provisioning** | Konstanta `DEFAULT_GUEK`, `DEFAULT_GBEK`, `DEFAULT_AK`, dan `SECURE_FACTORY_PROVISION_TOKEN` dilaporkan dihapus. Ciphering menolak perangkat belum diprovisi dengan `SECURE_ERROR_UNPROVISIONED` (-8). Ini belum menyelesaikan seluruh masalah persistensi dan otorisasi key-management. |
| **I4 — RNG** | Fallback LCG dihapus. Kegagalan TRNG (CECS, SECS, atau timeout) dilaporkan menolkan output dan mengembalikan error `-2` (*fail-closed*). Kualitas RNG hardware belum diuji. |
| **I6 — Pemeriksaan sebelum jump ke NonSecure** | `SECURE_VerifyNonSecureImage()` di [`main.c`](./Secure/Core/Src/main.c) memeriksa magic header `0x504C4E21` (`PLN!`), rentang MSP di RAM NonSecure, alignment ResetHandler/Thumb bit, dan anti-rollback version counter. Pengujian disebut ada di `test_ami_firmware_auth_policy.c`; validasi signature dan rantai boot tetap perlu ditinjau. |
| **I/O dan metrologi** | Modul HAL UART, USART, dan SPI dilaporkan diaktifkan di `stm32u5xx_hal_conf.h`. Driver UART circular DMA + IDLE dikaitkan dengan `stm32u5xx_it.c`. Driver ADE9000 memeriksa return code SPI dan menolak snapshot jika pembacaan gagal (`valid_mask = 0`). |
| **Data model** | Buffer Load Profile 23 kanal 15 menit (`1.0.99.1.0.255`) dan Tamper Event Logbook dengan 3 capture object serta 9 modus tamper PLN Event 20–28 dilaporkan selesai di `ami_cosem_model.c`. |
>>>>>>> d4ee38296f41d70969e4a6ee09e5214586a74b3b
