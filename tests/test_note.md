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

Daftar suite ditentukan di `tests/CMakeLists.txt`. Suite host mencakup model COSEM, security, association, firmware authentication policy, APDU, HDLC, port, driver metrologi ADE9000, serta enkripsi ciphered APDU Security Suite 0 (`AmiDlmsCipherTest`). Hasil CTest hanya membuktikan perilaku yang dicakup test tersebut pada build host; hasilnya bukan validasi hardware, UART, metrologi, sertifikasi, atau seluruh conformance IEC 62056.

`AmiDlmsApduTest` juga memeriksa bahwa seluruh objek yang diumumkan melalui Association View saat ini berhasil menjawab GET attribute 2 menggunakan snapshot pengujian. Baseline objek/protokol yang dites didokumentasikan di [docs/PROTOCOL_PROFILE.md](../docs/security-rules/PROTOCOL_PROFILE.md).

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

Firmware policy test juga memastikan hanya OID HLS5-GMAC dengan challenge client 16-byte yang diterima; No-Auth, LLS, HLS3/MD5, HLS4/SHA-1, dan challenge dengan panjang lain ditolak. Test ini memverifikasi policy, bukan implementasi crypto Secure-world, provisioning key, atau keamanan produksi. Profil target dan status gate tercatat di [docs/PROTOCOL_PROFILE.md](../docs/security-rules/PROTOCOL_PROFILE.md).

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

## Referensi Docker

Instruksi Docker dan cakupan test container tersedia di [docker_test/README.md](../docker_test/README.md).
