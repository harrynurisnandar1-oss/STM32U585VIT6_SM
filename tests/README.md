# Unit Testing AMI Smart Meter

Dokumen ini menjelaskan pengujian host-native untuk model DLMS/COSEM dan batas keamanan pada proyek STM32U585. Test dikompilasi untuk PC host melalui CMake, lalu dijalankan dengan CTest. Test ini tidak membutuhkan board, FreeRTOS runtime, Docker, atau CMock.

## Cakupan test

| Suite CTest | File test | Yang diverifikasi |
|---|---|---|
| `AmiCosemTest` | `test_ami_cosem.c` | Pembacaan tegangan/arus/energi Class 3, OBIS, scaler dan unit, validity mask, error object/attribute, serta Clock Class 8. |
| `AmiSecurityTest` | `test_ami_security.c` | Validasi backend, RNG dan pembersihan buffer, batas nonce/tag AES-GCM, kegagalan backend, serta zeroization saat autentikasi gagal. |
| `AmiDlmsAssociationTest` | `test_ami_dlms_association.c` | Penerimaan AARQ logical-name no-auth, pembentukan AARE, penolakan AARQ malformed/ber-auth, state transition, dan reset association. |
| `AmiDlmsApduTest` | `test_ami_dlms_apdu.c` | Get-Request-Normal untuk tegangan OBIS `1.0.32.7.0.255`, encoding Get-Response-Normal dengan nilai `230000` mV, access error untuk OBIS tidak dikenal, penolakan plaintext setelah HLS, pemrosesan global-ciphered GET/SET, tag rusak, dan anti-replay. |

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

Implementasi saat ini mencakup AARQ/AARE no-auth, LLS, configurable HLS, dan xDLMS Get-Request-Normal tanpa selective access. Belum mencakup framing/alamat HDLC, negosiasi InitiateRequest/InitiateResponse, atau integrasi ke task FreeRTOS/UART. Ciphering APDU bergantung pada callback AES-GCM Secure-world dan kebijakan invocation counter yang harus disediakan oleh profil produk; test host memakai fake backend dan bukan validasi kriptografi produksi. Karena itu hasil test adalah verifikasi unit host, bukan klaim kesesuaian penuh IEC 62056 atau validasi perangkat akhir.

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
*   ❌ **BELUM** - **Profile Generic (Class 7):** Merancang arsitektur penyimpanan riwayat data secara *array* (seperti profil beban/kurva beban harian).
*   ❌ **BELUM** - **Event Log & Billing Data:** Membuat model OBIS khusus untuk log *tampering* (pelanggaran/gangguan) dan register pencatatan tagihan periodik.

#### **Week 3 & 4: Security Management & Integrasi Lanjutan**
*Walaupun jadwal aslinya memuat tugas tim E3 (Display & Console) pada minggu ini, dari kacamata E2+Security, minggu ini difokuskan pada penyempurnaan manajemen objek keamanan.*
*   ❌ **BELUM** - **Security Setup Object (Class 15):** Menambahkan objek kelas 15 ke dalam `ami_cosem_model.c` untuk mengatur pertukaran atau pembaharuan *Global Unicast Key* dan *Authentication Key* secara standar.
*   ❌ **BELUM** - **Selective Access:** Memperbarui parser APDU agar mendukung *Selective Access* (memfilter balasan data *Profile Generic* hanya pada rentang waktu atau baris data tertentu, bukan men-*download* seluruh log dari awal).

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
Melihat rincian di atas, tugas E2 + Security di **Week 1** sudah rampung seluruhnya. 
Fokus Anda berikutnya harus tertuju pada sisa tugas **Week 2 & 3**, yaitu:
1.  Merancang **Profile Generic (Class 7)** untuk *Event Log* dan *Billing Registers*.
2.  Mengimplementasikan **Security Setup Object (Class 15)**.

Bagian mana yang ingin Anda kerjakan terlebih dahulu?