# Panduan Host Test dan Gurux Conformance

Dokumen ini menjelaskan apa yang diuji pada host, cara menjalankannya, serta cara menafsirkan hasil Gurux. Host test memakai data sintetis/snapshot dan tidak membutuhkan board.

## Menjalankan CTest

Dari root repository pada PowerShell:

```powershell
cmake -S tests -B tests/build -G Ninja
cmake --build tests/build --parallel
ctest --test-dir tests/build --output-on-failure
```

Untuk menjalankan satu suite:

```powershell
ctest --test-dir tests/build -R AmiDlmsApduTest --output-on-failure
```

Daftar 15 suite ditentukan di `tests/CMakeLists.txt`:
1. `AmiCosemTest`: Validasi data model COSEM: Load Profile 23-kanal 15-menit, Instant Profile 11-kanal (`1.96.98.128.0.255`), Event Log #1 standar 2 capture objects (`0.0.99.98.0.255`), Tamper Logbook 3 capture objects (9 Modus Tamper), Power Quality Voltage Log Profiles R/S/T (`1.0.99.14.2..4.255`), Class 6/11/20/22 objects, Multi-tarif, Power Quality, Limiter (Class 71), Disconnect Control (Class 70).
2. `AmiSecurityTest`: Validasi algoritma AES-128 NIST SP 800-38D KAT, GMAC, constant-time compare, dan exhaustive 1-bit flip tampering test (GMAC tag, AAD, IV).
3. `AmiDlmsAssociationTest`: State machine asosiasi AARQ/AARE, negosiasi conformance, dan handling disconnect.
4. `AmiFirmwareAuthPolicyTest`: Penegakan kebijakan keamanan fail-closed firmware target (blokir No-Auth/LLS, wajib HLS5 16-byte challenge, secure boot manifest check).
5. `AmiDlmsApduTest`: xDLMS GET/SET/ACTION normal, ciphered APDU, dan verifikasi keterbacaan seluruh 66 objek aktif Association View (termasuk Instant Profile dan PQ Voltage Log Profiles).
6. `AmiDlmsHdlcTest`: Parser streaming HDLC zero-heap, CRC-16 HCS/FCS, resiliensi flag transparent 0x7E, dan LLC framing.
7. `AmiDlmsPortTest`: Layer transport port, buffer circular UART, dan integrasi cipher backend.
8. `AmiAde9000Test`: Driver metrologi SPI ADE9000, register scaling, penanganan error fail-closed SPI, dan feeder snapshot.
9. `AmiDlmsCipherTest`: Engine ciphering Suite 0 (AES-128-GCM), autentikasi tag, dan penolakan replay/tampering.
10. `AmiNvmCounterTest`: Wear-leveling NVM/TAMP anti-replay Invocation Counter dan proteksi batas saturasi 0xFFFFFFFF.
11. `AmiKeyTransferTest`: Remote Key Transfer RFC 3394 AES Key Wrap atomik di Secure World.
12. `AmiAntiBruteforceTest`: Mitigasi brute-force per SPLN D5.002:2024 (penalti progresif 5s, 10s, 20s, 40s, lockout permanen 5x gagal).
13. `AmiBlockTransferTest`: General Block Transfer (GBT) xDLMS Datablock untuk transmisi payload profil/buffer besar.
14. `AmiDisconnectControlTest`: State machine Disconnect Control Class 70 (Disconnected, Connected, Ready) dan Action method.
15. `AmiTrustZoneBoundaryTest`: Validasi batas keamanan ARM Cortex-M33 CMSE NonSecure Callable (NSC), penolakan pointer NULL, pencegahan integer pointer overflow (`uptr + size < uptr`), buffer melintasi batas Secure World, dan penolakan izin tulis ke NonSecure Flash.

`AmiDlmsApduTest` juga memeriksa bahwa seluruh objek yang diumumkan melalui Association View berhasil menjawab GET attribute 2 menggunakan snapshot pengujian. Baseline objek/protokol yang dites didokumentasikan di [docs/security-rules/PROTOCOL_PROFILE.md](../docs/security-rules/PROTOCOL_PROFILE.md).

## Simulator Gurux

Build dan jalankan simulator:

```powershell
cmake -S tests -B tests/build -G Ninja
cmake --build tests/build --target gurux_simulator
.\tests\build\host-tools\gurux_simulator.exe
```

Simulator menerima raw HDLC-over-TCP di `127.0.0.1:4059`. Di Gurux DLMSDirector gunakan:

| Parameter | Nilai untuk simulator |
|---|---|
| Media | Network / TCP |
| Host | `127.0.0.1` |
| Port | `4059` |
| Interface | HDLC, bukan Wrapper |
| Referencing | Logical Name (LN) |
| Authentication | None, hanya untuk profil simulator pengembangan |

Alamat client/server harus mengikuti profil koneksi simulator. Jika koneksi gagal, periksa log simulator dan trace HDLC sebelum mengubah parameter lain.

**Penting:** simulator host yang memakai No-Auth berbeda dari firmware. Firmware dikonfigurasi untuk menolak No-Auth dan LLS. Hasil CTT dengan `Authentication level: None` terhadap simulator tidak membuktikan kebijakan autentikasi firmware. Uji firmware memerlukan profil autentikasi yang didukung oleh integrasi target.

Firmware policy test juga memastikan hanya OID HLS5-GMAC dengan challenge client 16-byte yang diterima; No-Auth, LLS, HLS3/MD5, HLS4/SHA-1, dan challenge dengan panjang lain ditolak. Test ini memverifikasi policy, bukan implementasi crypto Secure-world, provisioning key, atau keamanan produksi. Profil target dan status gate tercatat di [docs/security-rules/PROTOCOL_PROFILE.md](../docs/security-rules/PROTOCOL_PROFILE.md).

## OBIS: katalog, model, dan Association View

Ada tiga hal yang jangan disamakan:

1. **Katalog OBIS** (`NonSecure/AMI/Inc/ami_obis_catalog.h`) mencatat kode yang dirujuk atau direncanakan proyek.
2. **Model COSEM** menentukan objek/atribut dan nilai yang dapat diproses oleh implementasi.
3. **Association View** (`association_view` di `ami_dlms_apdu.c`) mengumumkan objek yang tersedia kepada klien DLMS.

Sebuah kode yang ada di katalog belum tentu sudah tersedia melalui association atau dapat dibaca dengan GET. Saat menilai conformance, cocokkan class ID, logical name/OBIS, versi class, attribute, tipe data, skala/unit, dan hak akses untuk setiap objek yang dijanjikan profil produk.

## Menjalankan Gurux Conformance Tests (CTT)

Mulai dengan profil yang cocok dengan endpoint yang diuji:

- Pilih Logical Name referencing jika itu mode yang diimplementasikan dan digunakan pada koneksi.
- Atur autentikasi sesuai endpoint: No-Auth hanya untuk simulator pengembangan; jangan menurunkan kebijakan keamanan firmware untuk menghilangkan kegagalan CTT.
- Pastikan media/interface dan alamat cocok dengan koneksi aktual, dan simpan laporan serta trace request/respons.
- Jalankan tes HDLC jika endpoint menggunakan HDLC dan koneksi sudah terbentuk.
- Jalankan tes aplikasi/GET untuk objek yang memang termasuk Association View dan profil produk.

`ExcludeBasicTests = True` melewati Basic Tests; ini cocok untuk run rutin terbatas pada cakupan yang telah dipilih, tetapi **bukan** bukti bahwa Basic Tests lulus. Mengubahnya menjadi `False` berguna untuk gap analysis. CTT dapat meminta banyak class/OBIS/attribute standar yang tidak disediakan implementasi terbatas; kegagalan seperti `UndefinedObject` menunjukkan objek yang diminta tidak ditemukan pada endpoint, bukan otomatis bug security. Namun, bila objek tersebut dijanjikan produk, catat sebagai gap implementasi dan investigasi.

Jangan mengaktifkan atau menafsirkan tes SET, Action, image transfer, SN, maupun layanan lain sebagai persyaratan produk kecuali fitur tersebut memang ada di profil dan implementasi. Jangan pula mengecualikan tes yang berkaitan dengan fitur yang dinyatakan didukung.

### Cara membaca hasil

- **Passed:** tes yang dilaporkan lulus pada endpoint dan konfigurasi tersebut saja.
- **Failed / Access Error / UndefinedObject:** simpan detail request, response, class, OBIS, attribute, dan endpoint. Bandingkan dengan objek yang diumumkan serta akses yang diharapkan.
- **Ignored / Excluded:** tes tidak dijalankan; jangan dihitung sebagai lulus.
- **Authentication None:** identifikasi apakah endpoint adalah simulator atau firmware sebelum membuat kesimpulan keamanan.

Contoh: `Attribute0SupportedWithGet` yang gagal perlu ditelusuri melalui trace APDU yang berisi request aktual. Ringkasan CTT saja belum cukup untuk menentukan objek dan perbaikan yang benar.

## Batas pengujian

- Snapshot/sample data digunakan untuk menguji pemetaan dan pengiriman nilai. Test ini tidak menghitung atau membuktikan akurasi metrologi.
- Host test dan TCP simulator tidak membuktikan komunikasi UART fisik, timing, integrasi FreeRTOS pada board, atau ketahanan perangkat di lapangan.
- Keberhasilan CTT untuk satu konfigurasi tidak membuktikan seluruh profil produk atau sertifikasi.
- Pengujian autentikasi HLS produksi memerlukan layanan kriptografi, provisioning, penyimpanan kunci, dan konfigurasi target yang disetujui. Host test tidak menggantikan validasi integrasi Secure-world.

## Analisis Code Coverage (LCOV)

Untuk mengukur dan memverifikasi ketercakupan kode (*coverage*) pada host:

```powershell
# Jalankan via Docker / Linux
cmake -S tests -B tests/build_cov -G Ninja -DENABLE_COVERAGE=ON
cmake --build tests/build_cov
ctest --test-dir tests/build_cov
lcov --capture --directory tests/build_cov --output-file tests/build_cov/coverage_raw.info --ignore-errors inconsistent,gcov
lcov --extract tests/build_cov/coverage_raw.info '*/NonSecure/AMI/*' '*/Secure/*' --output-file tests/build_cov/coverage_filtered.info --ignore-errors unused,inconsistent
lcov --list tests/build_cov/coverage_filtered.info
python3 tests/scripts/lcov_to_markdown.py tests/build_cov/coverage_filtered.info docs/COVERAGE.md
```

Hasil coverage firmware saat ini tersimpan otomatis di [docs/COVERAGE.md](../docs/COVERAGE.md):
- **Total Line Coverage**: **79.2%** (2.600 / 3.281 baris).
- **Total Function Coverage**: **95.7%** (154 / 161 fungsi).
- Modul keamanan kritis (`ami_security.c`, `ami_dlms_hdlc.c`, `ami_dlms_cipher.c`) mencapai line coverage **> 89%**.

## Referensi Docker

Instruksi Docker dan cakupan test container tersedia di [docker_test/README.md](../docker_test/README.md).
