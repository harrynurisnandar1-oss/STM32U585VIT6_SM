# Unit Testing AMI Smart Meter

## Gurux raw HDLC host harness

Build and run the development TCP server:

```powershell
cmake --build build --target gurux_simulator
.\build\host-tools\gurux_simulator.exe
```

The server listens only on `127.0.0.1:4059`. It supports no-auth AARQ and read-only GETs using sample three-phase values. The HDLC decoder removes incoming LLC (`E6 E6 00` / `E6 E7 00`); outgoing I-frames include server LLC (`E6 E7 00`). SNRM negotiation is answered with UA parameters capped to 512-byte information fields and window size 1.

This harness is raw HDLC over TCP, not the DLMS Wrapper protocol commonly used by Gurux TCP/IP connections. It is for host development and does not enable HLS or ciphered APDUs.

Dokumen ini menjelaskan pengujian host-native untuk model DLMS/COSEM dan batas keamanan pada proyek STM32U585. Test dikompilasi untuk PC host melalui CMake, lalu dijalankan dengan CTest. Test ini tidak membutuhkan board, FreeRTOS runtime, Docker, atau CMock.

## Cakupan test

| Suite CTest | File test | Yang diverifikasi |
|---|---|---|
| `AmiCosemTest` | `test_ami_cosem.c` | Pembacaan Class 3, Clock Class 8, Event Log dan Billing Profile Class 7, buffer/capture metadata, serta metadata Security Setup Class 64. |
| `AmiSecurityTest` | `test_ami_security.c` | Validasi backend, RNG dan pembersihan buffer, batas nonce/tag AES-GCM, kegagalan backend, serta zeroization saat autentikasi gagal. |
| `AmiDlmsAssociationTest` | `test_ami_dlms_association.c` | Penerimaan AARQ logical-name no-auth, pembentukan AARE, penolakan AARQ malformed/ber-auth, state transition, dan reset association. |
| `AmiDlmsApduTest` | `test_ami_dlms_apdu.c` | GET response, Class 7 selective access berdasarkan entry/date-time, penolakan plaintext setelah HLS, global-ciphered GET/SET/Action, key-transfer SAL callback, tag rusak, dan anti-replay. |
| `AmiDlmsHdlcTest` | `test_ami_dlms_hdlc.c` | Byte-stream parsing buffer tetap, vektor referensi CRC-16 (FCS/HCS), batas alamat SAP, deteksi frame terpangkas/error, serta pembentukan frame SNRM/UA/DISC dan I-Frame. |

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
ctest --test-dir tests/build -R AmiDlmsHdlcTest --verbose
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

## GitHub Actions (Continuous Integration)

Workflow CI (`.github/workflows/ci.yml`) berfungsi sebagai **gerbang verifikasi otomatis (Quality Gate)** untuk memastikan integritas logika protokol dan keamanan tidak rusak saat ada perubahan kode:

1. **Trigger Otomatis**:
   - Berjalan otomatis setiap kali ada `git push` ke cabang utama atau pembukaan `pull_request`, serta dapat dipicu manual melalui `workflow_dispatch`.
2. **Pengujian Multi-Platform Paralel**:
   - **`host-tests-linux` (`ubuntu-latest`)**: Memverifikasi kompilasi GCC/Clang dan eksekusi CTest di lingkungan Linux.
   - **`host-tests-windows` (`windows-latest`)**: Memverifikasi kompilasi MSVC/Windows runtime untuk memastikan kompatibilitas lingkungan PC developer.
3. **Fungsi & Manfaat Utama Pengujian di GitHub**:
   - **Mencegah Regresi (Regression Guard)**: Memastikan perubahan logika parser HDLC atau APDU tidak merusak suite pengujian yang sudah lulus.
   - **Verifikasi Tanpa Hardware**: Memungkinkan tim memvalidasi logika DLMS/COSEM, ciphering Suite 0, anti-replay, dan framing HDLC tanpa memerlukan board STM32 fisik.
   - **Standar Kolaborasi Tim**: Setiap pull request wajib berstatus hijau (*passed 5/5 suite test*) sebelum di-merge ke branch utama.
   - **Deteksi Dini Memory Leak / Buffer Overflow**: Menangkap kegagalan boundary data, CRC error, dan eksploitasi framing langsung di pipeline cloud.



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

*   ✅ **SELESAI (~95%)** - **Week 6–7 (Maturation):**
    *  ✅ Logika inti DLMS (HLS GMAC, APDU Ciphering Suite 0, Class 64 Security Setup, Selective Access Class 7) sudah selesai dan lulus simulasi host.
    *  ✅ Pembungkus **HDLC Framing Engine** (`ami_dlms_hdlc.c/.h`) sudah selesai, lulus pengujian CTest, dan dimitigasi dari celah keamanan framing/DoS.
    *  ⏳ *Opsional/Lanjutan*: Block Transfer (`GET-Request-Next`) jika payload Profil Melebihi PDU 512-byte (saat ini kapasitas baris dibatasi aman di dalam PDU).
*   ⏳ **BELUM** - **Week 8–9 (Hardware Integration):** Mem-porting kode *protocol stack* ini ke papan STM32U585 sungguhan. Tugas tambahan E2 di sini adalah menyambungkan *parser APDU* dengan *Data Link Layer* (biasanya HDLC via UART fisik).
*   ⏳ **BELUM** - **Week 10 (End-to-End Validation):** Aplikasi eksternal seperti DLMS Client (mis. Gurux/DLMSDirector) terkoneksi sukses ke *meter* kita untuk pertama kalinya dan membaca log serta register secara terenkripsi.

---

### Spin 2: Full Mains Prototype & Compliance (Weeks 11–26) — Khusus Lajur E2 (Protocol & Security)

Pada Spin 2, prototipe berpindah ke papan tegangan penuh (*mains board* 230/400V) dengan fitur lengkap sesuai mandat SPLN D3.006:2021 dan SPLN D5.002:

*   ⏳ **Week 11–12 (Spin 1 to Spin 2 Porting & Consolidation):**
    *   Porting stack DLMS/COSEM yang stabil dari Spin 1 ke board prototipe Spin 2.
    *   Pengujian integrasi FreeRTOS Queue/Task antara lapisan UART/HDLC, APDU Dispatcher, dan buffer metrologi.
*   ⏳ **Week 13–14 (DLMS Read Set Freeze & Tamper Event Integration):**
    *   *Freeze* seluruh daftar OBIS pembacaan tiga fasa (Voltase, Arus, Daya Aktif/Reaktif/Semu, Power Factor, THD).
    *   Menghubungkan *Tamper Event Manager* dengan register deteksi tamper chip AFE 9000 (reverse energy, hilang tegangan/netral, magnet) ke dalam Event Log Class 7 (Tabel 18 SPLN D3.006:2021).
*   ⏳ **Week 15–16 (Block Transfer & Extended Coverage):**
    *   Implementasi xDLMS Block Transfer (`GET-Request-For-Next` dan `GET-Response-With-Datablock`) untuk kapasitas Event Log penuh (minimal 30 rekaman per SPLN) dan Load Profile 23 kanal.
    *   Peningkatan coverage unit test dan penutupan defect register Spin 1.
*   ⏳ **Week 17–20 (TrustZone Secure Integration):**
    *   Menghubungkan *Secure-world SAL callbacks* dengan hardware crypto accelerator STM32U585 (SAES / PKA) dan Secure Storage TrustZone untuk proteksi KEK/Master Key.
    *   Durable invocation counter storage di non-volatile memory (FRAM) untuk pencegahan *replay attack* permanen pasca power-cycle (*last gasp*).
*   ⏳ **Week 21–24 (End-to-End Validation pada Mains 3-Phase):**
    *   Uji baca jarak jauh *end-to-end* via HES / DLMS Client pihak ketiga (Gurux DLMSDirector) melalui optical probe dan RS-232 pada beban aktif 3 fasa.
    *   Verifikasi pembacaan Event Log, Load Profile, dan eksekusi Key Transfer Class 64.
*   ⏳ **Week 25–26 (Soak Test & Handoff):**
    *   *Feature freeze*, soak test stabilitas komunikasi non-stop, dokumentasi final protokol, dan persiapan uji sertifikasi PLN Pusertif.

---

### Apa Langkah Anda Selanjutnya?
Tugas model host untuk Profile Generic, Security Setup, selective access, dan HDLC framing kini tersedia. Langkah lanjutan adalah mengintegrasikan penyimpanan profil serta key transfer ke driver/FreeRTOS dan Secure-world backend, menambahkan UART driver fisik, lalu menguji terhadap DLMS client dan hardware.

### HDLC host tests

AmiDlmsHdlcTest exercises fixed-buffer byte-stream parsing, a CRC reference vector, SAP address boundaries, HCS/FCS errors, truncated frames, and SNRM/UA/DISC plus I-frame building. The codec has no UART or MCU-register dependency. These host tests do not validate serial timing, FreeRTOS integration, or external DLMS client interoperability.

---

## Gurux DLMSDirector TCP Host Simulator

Simulator host lokal untuk memvalidasi *stack* DLMS/COSEM dan framing HDLC tanpa board fisik menggunakan aplikasi client standar industri (Gurux DLMSDirector / GXDLMSDirector). Simulator mendengarkan transmisi HDLC-over-TCP pada port standar DLMS `4059`.

### Fitur Simulator
- **HDLC-over-TCP Server**: Melayani request connection-oriented (`SNRM` / `DISC` / `I-Frame`) dari software eksternal secara real-time.
- **LLC Sublayer Auto-handling**: Menerima request ber-header LLC (`0xE6 0xE6 0x00`) dan otomatis menyisipkan LLC response (`0xE6 0xE7 0x00`) pada balasan I-Frame.
- **SNRM Parameter Negotiation**: Menegosiasikan ukuran informasi frame (Max Info Length TX/RX = 128 byte, Window Size = 1).
- **End-to-End COSEM Reading**: Melayani pembacaan instan tegangan 3-fasa (Class 3), Clock (Class 8), serta Profile Generic (Class 7).

### Build dan Menjalankan Simulator

1. **Kompilasi executable simulator via CMake:**
   ```powershell
   cmake -S tests -B tests/build -G Ninja
   cmake --build tests/build --target gurux_simulator
   ```

2. **Jalankan simulator:**
   ```powershell
   .\tests\build\host-tools\gurux_simulator.exe
   ```
   *Terminal akan menampilkan:* `Listening for raw HDLC-over-TCP on 127.0.0.1:4059...`

### Panduan Koneksi Gurux DLMSDirector

Buka aplikasi **GXDLMSDirector**, buat koneksi device baru dengan konfigurasi berikut:

| Parameter | Nilai Konfigurasi | Keterangan |
|---|---|---|
| **Media** | `Network` | Menggunakan koneksi TCP socket lokal |
| **Host Name / IP** | `127.0.0.1` | Loopback adapter |
| **Port** | `4059` | Port standar IEC 62056-47 / DLMS |
| **Interface Type** | `HDLC` | Wajib HDLC (bukan WRAPPER) |
| **Client Address** | `16` (`0x10`) | Management / Operator Client SAP |
| **Physical Server** | `1` | Alamat fisik meter (MAC Destination) |
| **Logical Server** | `1` | Alamat logis COSEM Management Device |
| **Authentication** | `None` | Profil development no-auth |

Klik **Connect**. Setelah status terkoneksi (**Green / Connected**), pilih menu **Read Objects** untuk membaca data metrologi dan event log.
