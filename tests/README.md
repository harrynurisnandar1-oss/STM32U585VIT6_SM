# Unit Testing AMI Smart Meter

Dokumen ini menjelaskan pengujian host-native untuk model DLMS/COSEM dan batas keamanan pada proyek STM32U585. Test dikompilasi untuk PC host melalui CMake, lalu dijalankan dengan CTest. Test ini tidak membutuhkan board, FreeRTOS runtime, Docker, atau CMock.

## Cakupan test

| Suite CTest | File test | Yang diverifikasi |
|---|---|---|
| `AmiCosemTest` | `test_ami_cosem.c` | Pembacaan Class 3, Clock Class 8, Event Log dan Billing Profile Class 7, buffer/capture metadata, serta metadata Security Setup Class 64. |
| `AmiSecurityTest` | `test_ami_security.c` | Validasi backend, RNG dan pembersihan buffer, batas nonce/tag AES-GCM, kegagalan backend, serta zeroization saat autentikasi gagal. |
| `AmiDlmsAssociationTest` | `test_ami_dlms_association.c` | Penerimaan AARQ logical-name no-auth, pembentukan AARE, penolakan AARQ malformed/ber-auth, state transition, dan reset association. |
| `AmiDlmsApduTest` | `test_ami_dlms_apdu.c` | GET response, Class 7 selective access berdasarkan entry/date-time, penolakan plaintext setelah HLS, global-ciphered GET/SET/Action, key-transfer SAL callback, tag rusak, dan anti-replay. |

Test model dan protokol menggunakan data snapshot development yang diberikan langsung oleh test. Test tidak membuktikan perilaku metrologi hardware maupun komunikasi UART/HDLC.

## Menjalankan test

Dari PowerShell pada root proyek:

```powershell
cd D:\STM32U585VIT6_SM
cmake -S tests -B tests/build -G Ninja
cmake --build tests/build --parallel
ctest --test-dir tests/build --output-on-failure
```

Untuk menjalankan satu suite saja:

```powershell
ctest --test-dir tests/build -R AmiDlmsAssociationTest --verbose
ctest --test-dir tests/build -R AmiDlmsApduTest --verbose
ctest --test-dir tests/build -R AmiCosemTest --verbose
ctest --test-dir tests/build -R AmiSecurityTest --verbose
```

Test host terpisah dari build firmware CubeMX. Build firmware tetap dijalankan dari preset/folder build STM32, bukan dari `tests/build`.

## Batas cakupan protokol Sprint 1

Implementasi saat ini mencakup AARQ/AARE, LLS/HLS, GET, dan selective access Class 7 selector 1 (rentang Clock) serta selector 2 (rentang entry, semua kolom). Project belum mencakup framing/alamat HDLC, negosiasi InitiateRequest/InitiateResponse, selective column projection, block transfer untuk balasan panjang, atau integrasi task FreeRTOS/UART. Security Setup Class 64 mengekspos metadata dan menyediakan jalur ciphered Action untuk key transfer Suite 0; implementasi produk wajib menyuplai callback Secure-world yang unwrap RFC 3394 dengan KEK dan menyimpan batch key secara atomik. Host memakai fake callback, bukan validasi AES-GCM atau penyimpanan Secure.

## Memakai CMake Tools di VS Code

Root proyek memiliki `CMakeLists.txt` CubeMX untuk firmware STM32 dan toolchain ARM; host tests adalah proyek CMake terpisah di `tests/`. Karena itu, saat VS Code membuka root tanpa pengaturan source directory, CMake Tools memilih proyek firmware, bukan CTest host. Workspace settings mengarahkan extension ke `tests/`, sedangkan `tests/CMakePresets.json` memilih compiler host dan folder output terpisah.

1. Buka folder `D:\STM32U585VIT6_SM` di VS Code dan pastikan extension **CMake Tools** aktif.
2. Jalankan `CMake: Configure`, pilih preset **AMI host unit tests** bila diminta, lalu `CMake: Build`.
3. Jalankan `CMake: Run Tests` atau pilih tes melalui Test Explorer.

Jika preset tidak muncul, pastikan versi CMake dapat membaca Presets versi 3 (CMake 3.21+), extension CMake Tools sudah aktif, lalu jalankan `CMake: Delete Cache and Reconfigure`. `tests/CMakePresets.json` menggunakan Ninja; pasang Ninja atau ubah preset ke generator host yang tersedia. Untuk kembali membangun firmware CubeMX dari extension, ubah `cmake.sourceDirectory` ke `${workspaceFolder}` dan pilih preset firmware `Debug`.

## GitHub Actions

Workflow `../.github/workflows/ci.yml` berjalan otomatis saat ada push atau pull request, dan dapat dijalankan manual melalui `workflow_dispatch`. Workflow menggunakan Ubuntu runner, membangun target test host, lalu menjalankan seluruh suite CTest. Workflow ini tidak membangun firmware ARM dan tidak memerlukan Docker.



Baik, jika kita menyaring jadwal **Spin 1 (Weeks 1–10)** *hanya* khusus untuk *scope* pekerjaan **E2 (Protocol & Data Model) + Security**, maka tampilannya akan menjadi jauh lebih fokus pada standar komunikasi dan keamanan data.

Berikut adalah rincian tugas untuk lajur E2 + Security beserta statusnya:

---

### Phase 1: Blind Firmware & Protocol Design (Weeks 1–5)

#### **Week 1: Inti DLMS & Lapisan Keamanan (Security Layer)**
Fokus pada infrastruktur protokol pertukaran pesan yang kebal sadap (*ciphered*) dan terautentikasi.
*   ✅ **SELESAI** - **Association State Machine:** Pemrosesan AARQ dan perakitan AARE (Tag `0x60` dan `0x61`).
*   ✅ **SELESAI** - **LLS & HLS Authentication:** Implementasi mekanisme *Low-Level Security* (Password) dan *High-Level Security* (GMAC/AES-128 Challenge-Response).
*   ✅ **SELESAI** - **APDU Ciphering:** Dukungan dekripsi/enkripsi `Glo-Get-Request` dan `Glo-Set-Request` menggunakan *AES-GCM Authenticated Encryption*.
*   ✅ **SELESAI** - **Replay Attack Prevention:** Penolakan *request* berdasarkan *Invocation Counter* palsu/kedaluwarsa.

#### **Week 2: Pemodelan Objek (OBIS & COSEM Model)**
Menerjemahkan data fisis menjadi objek logis yang dikenali oleh protokol meteran pintar.
*   ✅ **SELESAI** - **Standard OBIS Registers:** Implementasi objek dasar tegangan, arus, dan energi aktif/reaktif beserta formatnya (Class 3 & 4), serta objek *Clock* (Class 8).
*   ✅ **SELESAI (host model)** - **Profile Generic (Class 7):** Buffer bounded untuk data riwayat dan selective access melalui rentang entry/timestamp.
*   ✅ **SELESAI (host model)** - **Event Log & Billing Data:** Profile Generic read-only untuk event dan billing dengan contoh row yang disuplai board layer.

#### **Week 3 & 4: Security Management & Integrasi Lanjutan**
*Walaupun jadwal aslinya memuat tugas tim E3 (Display & Console) pada minggu ini, dari kacamata E2+Security, minggu ini difokuskan pada penyempurnaan manajemen objek keamanan.*
*   ✅ **SELESAI (model + SAL hook)** - **Security Setup Object (Class 64):** Class 15 adalah Association LN; Security Setup merupakan Class 64. Metadata object dan ciphered key-transfer Action untuk GUEK/Auth key sudah ada; unwrap KEK dan secure storage harus diimplementasikan di backend Secure-world.
*   ✅ **SELESAI (subset)** - **Selective Access:** Selector 1 untuk rentang Clock dan selector 2 untuk rentang entry (semua kolom); column projection dan block transfer belum termasuk.

#### **Week 5: CI & Testing Assurance**
*   ✅ **SELESAI** - **Host-Side Tests Green:** Pengujian *unit-test* lokal untuk logika E2+Security tanpa menggunakan *hardware* fisik (`test_ami_dlms_association.c` & `test_ami_dlms_apdu.c` sudah hijau/sukses berjalan).

---

### Phase 2: Board Bring-Up & Uji Fisik (Weeks 6–10)
Fase ini dilakukan setelah pembuatan kode selesai dan kita menunggu PCB fisik (Spin 1) tiba.

*   ⏳ **BELUM** - **Week 6–7 (Maturation):** Menyempurnakan dan menambal *bug* logika DLMS dan struktur OBIS melalui simulasi mendalam di *host* PC.
*   ⏳ **BELUM** - **Week 8–9 (Hardware Integration):** Mem-porting kode *protocol stack* ini ke papan STM32U585 sungguhan. Tugas tambahan E2 di sini adalah menyambungkan *parser APDU* dengan *Data Link Layer* (biasanya HDLC via UART fisik).
*   ⏳ **BELUM** - **Week 10 (End-to-End Validation):** Aplikasi eksternal seperti DLMS Client (mis. Gurux/DLMSDirector) terkoneksi sukses ke *meter* kita untuk pertama kalinya dan membaca log serta register secara terenkripsi.

---

### Apa Langkah Anda Selanjutnya?
Tugas model host untuk Profile Generic, Security Setup, dan selective access kini tersedia. Langkah lanjutan adalah mengintegrasikan penyimpanan profil serta key transfer ke driver/FreeRTOS dan Secure-world backend, menambahkan HDLC/UART serta block transfer, lalu menguji terhadap DLMS client dan hardware.
