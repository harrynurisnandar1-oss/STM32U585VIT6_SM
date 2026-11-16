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
