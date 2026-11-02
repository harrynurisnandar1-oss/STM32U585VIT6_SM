# STM32U585VIT6 + ADE9000 3-Phase AMI Smart Meter

Firmware proyek meter listrik pintar pascabayar 3-fasa dengan target board STM32U585VIT6 dan metering IC ADE9000. Repository berisi kode firmware, model DLMS/COSEM, lapisan keamanan, host tests, dan simulator untuk pengembangan protokol.

## Ringkasan pekerjaan E2: protokol dan data model

Fokus pekerjaan E2 adalah menyediakan komunikasi meter melalui DLMS/COSEM dan menyajikan data meter sebagai objek COSEM yang konsisten. Platform target proyek adalah STM32U585VIT6 dengan ADE9000 sebagai IC metering; integrasi board dan validasi sinyal metering dikerjakan pada jalur hardware terpisah.

### Cakupan E2

- **Protokol DLMS/COSEM:** pemrosesan asosiasi AARQ/AARE, permintaan/response xDLMS yang diimplementasikan, serta pemetaan kesalahan protokol.
- **HDLC:** parser dan pembentuk frame, pemeriksaan HCS/FCS, addressing, serta kontrol link yang tercakup pada implementasi dan host tests. Ini tidak sama dengan driver UART fisik.
- **Data model COSEM:** pemetaan OBIS ke class, logical name, atribut, tipe data, skala/unit, dan hak akses; termasuk objek yang diimplementasikan untuk pengukuran, Clock, serta profile/log yang didukung.
- **Selective access:** dukungan subset yang tersedia pada implementasi, seperti akses profile berdasarkan rentang yang diuji.
- **Keamanan protokol:** penegakan kebijakan autentikasi firmware dan pengujian negatif pada host. Host simulator No-Auth hanya untuk pengembangan; backend Secure-world, provisioning key, dan HLS produksi masih memerlukan integrasi target.
- **Verifikasi host:** CTest, simulator TCP Gurux, dan trace request/response untuk memeriksa pemrosesan protokol tanpa board.

### Batas tanggung jawab dan status integrasi

E2 menangani kontrak data antara sumber data meter dan protokol: objek/OBIS yang disediakan, format nilai, akses, dan perilaku respons. Pembacaan register ADE9000, kalkulasi/akurasi metrologi, kalibrasi, serta driver dan timing UART fisik menjadi tanggung jawab integrasi hardware/metrologi.

Katalog OBIS bukan bukti bahwa seluruh objek sudah tersedia di perangkat. Objek yang benar-benar dilayani harus cocok dengan model COSEM dan Association View yang diumumkan. Host tests memakai sample/snapshot, sehingga membuktikan perilaku kode yang diuji pada host saja—bukan integrasi ADE9000, UART, board, atau akurasi meter.

## Workflow pekerjaan E2: Spin 1 dan Spin 2

Tahapan berikut merangkum alur pekerjaan E2 untuk protokol dan data model. Spin 1 memakai dua fase untuk membangun dan mengintegrasikan fondasi; Spin 2 memakai dua fase untuk melengkapi fitur produk dan menyiapkan validasi. Fitur yang ditandai sebagai kebutuhan harus dikonfirmasi terhadap profil produk dan standar yang disepakati. Daftar ini adalah workflow, bukan klaim bahwa semua fitur sudah tersedia.

### Spin 1 — fondasi dan integrasi awal

#### Phase 1 — desain protokol dan implementasi host

**Alur kerja:** tetapkan profil komunikasi dan daftar objek → rancang kontrak data COSEM → implementasikan jalur protokol → verifikasi dengan test host dan simulator.

**Fitur/hasil yang dibutuhkan:**

- Profil DLMS/COSEM: Logical Name atau Short Name, jenis interface, alamat client/server, layanan yang didukung, serta aturan asosiasi dan autentikasi.
- Daftar OBIS produk dengan class/version, attribute, tipe data, skala/unit, hak akses, dan status dukungan. Cocokkan daftar tersebut dengan Association View.
- Jalur AARQ/AARE dan xDLMS yang termasuk scope, respons error yang tepat, serta kebijakan penolakan akses.
- HDLC framing dan kontrol link sesuai interface produk, termasuk validasi alamat, HCS/FCS, batas ukuran, dan penanganan frame tidak valid.
- Model objek read-only yang memang diperlukan, seperti register pengukuran, Clock, serta profile/log yang sudah disepakati; selective access hanya untuk selector yang benar-benar diimplementasikan.
- Host unit tests, simulator Gurux, dan test negatif untuk autentikasi, parser, state asosiasi, akses objek, serta kasus batas.

**Status repository:** baseline host Phase 1 untuk protokol/data model sudah didokumentasikan, dan regresi memeriksa bahwa setiap objek dalam Association View dapat menjawab GET. Profil E2 yang dipilih menetapkan LN, client SAP 16 dengan HLS5-GMAC/Suite 0, server 1/1, serta key-transfer jarak jauh berbasis KEK pabrik dengan otorisasi tambahan. Persetujuan formal dan backend produksi Secure-world/provisioning belum selesai; Phase 1 belum dapat dinyatakan tuntas. Katalog OBIS juga lebih luas daripada objek yang saat ini diumumkan/dilayani. Rincian dan gate ada di [docs/security-rules/PROTOCOL_PROFILE.md](docs/security-rules/PROTOCOL_PROFILE.md).

#### Phase 2 — integrasi awal board

**Alur kerja:** integrasikan firmware komunikasi pada target STM32U585VIT6 → hubungkan transport UART dan sumber snapshot dari jalur hardware → verifikasi kontrak data dan protokol secara end-to-end.

**Fitur/hasil yang dibutuhkan:**

- Integrasi transport board: driver UART/DMA, callback RX/TX, timeout, buffer ownership, sinkronisasi task/ISR, serta penanganan error link. Driver dan validasi UART menjadi tanggung jawab jalur hardware.
- Integrasi data: sepakati format snapshot dari jalur ADE9000/metrologi—nilai, satuan, skala, validitas, dan perilaku saat data tidak tersedia. E2 memvalidasi pemetaan snapshot ke objek COSEM, bukan kalkulasi atau akurasi pengukuran.
- Verifikasi SNRM/UA, pertukaran HDLC/APDU, pembacaan objek yang disepakati, disconnect/reconnect, dan perilaku saat komunikasi terputus.
- Pastikan profil autentikasi firmware digunakan; hasil simulator No-Auth tetap dipisahkan dari hasil board.
- Catat trace, konfigurasi, versi firmware, dan endpoint agar hasil host dan board tidak tercampur.

**Kriteria selesai:** client dapat berkomunikasi dengan board melalui transport yang diintegrasikan dan membaca objek dalam profil yang disepakati, dengan respons dan akses sesuai spesifikasi. Akurasi metrologi tetap diverifikasi oleh pemilik area metrologi.

### Spin 2 — pelengkapan produk dan kesiapan validasi

#### Phase 3 — pelengkapan fitur protokol/data model

**Alur kerja:** bekukan profil produk → tutup gap objek dan layanan → integrasikan kontrol keamanan produksi → perluas regression test.

**Fitur/hasil yang dibutuhkan sesuai profil produk:**

- Finalisasi OBIS dan Association View, termasuk register pascabayar 3-fasa, identitas meter, Clock, billing/load/event profile, alarm/tamper, dan power quality bila diwajibkan.
- Implementasi atribut, tipe data, skala/unit, hak akses, serta selective access yang konsisten untuk setiap objek yang dijanjikan.
- Block transfer untuk respons besar, misalnya profile/log, bila ukuran data dan profil komunikasi memerlukannya.
- Integrasi layanan keamanan produksi: autentikasi yang disetujui, backend kriptografi Secure-world, provisioning/penyimpanan kunci, validasi peer/SAP, serta invocation counter yang tahan reset bila diwajibkan desain.
- Pengujian negatif dan robustness: frame/APDU malformed, nilai batas, request sebelum asosiasi, replay/counter tidak valid, pembukaan asosiasi ulang, timeout, dan pemulihan sesi.

**Kriteria selesai:** tidak ada perbedaan antara profil yang disepakati, objek yang diumumkan, dan perilaku GET/akses aktual; dependensi keamanan produksi sudah terintegrasi atau dinyatakan sebagai blocker secara eksplisit.

#### Phase 4 — conformance, stabilisasi, dan handoff

**Alur kerja:** jalankan test suite terhadap endpoint dan profil final → analisis setiap kegagalan → perbaiki atau dokumentasikan pengecualian → lakukan regression/soak test → serahkan bukti validasi.

**Fitur/hasil yang dibutuhkan:**

- Gurux CTT untuk mode referencing, autentikasi, interface, dan layanan yang benar-benar didukung produk. Tes yang dikecualikan dicatat sebagai tidak diuji, bukan lulus.
- Pemeriksaan Association View dan GET terhadap seluruh objek dalam profil; investigasi trace untuk error seperti `UndefinedObject`, akses salah, atau attribute yang tidak didukung.
- Regression test host dan board untuk HDLC, asosiasi, GET/selective access, autentikasi, error handling, dan reconnect.
- Uji stabilitas komunikasi jangka panjang, reset/power-cycle untuk state dan counter keamanan, serta integrasi lintas task/buffer pada target.
- Dokumentasi hasil: konfigurasi test, endpoint, build/commit, trace penting, defect, pengecualian, dan status integrasi keamanan.

**Kriteria selesai:** setiap klaim dukungan memiliki bukti test pada endpoint yang relevan; hasil simulator, host, dan board dilaporkan terpisah. Sertifikasi metrologi, keselamatan, dan EMC berada di luar lingkup E2.

## Status dan batas kemampuan

- Firmware dan host simulator adalah dua target berbeda. Keberhasilan pengujian simulator tidak membuktikan perilaku firmware pada board.
- Pengujian host memakai snapshot/sample data. Pengujian tersebut memeriksa pemrosesan dan komunikasi data, bukan akurasi metrologi.
- `ami_obis_catalog.h` adalah katalog kode OBIS yang direncanakan/dirujuk proyek. Katalog tidak berarti semua objek sudah diimplementasikan atau diumumkan ke klien.
- Objek yang diumumkan ke klien berada di Association View dalam `ami_dlms_apdu.c`. Pastikan cakupan itu cocok dengan profil produk sebelum menyimpulkan hasil conformance.
- Firmware menolak autentikasi None dan LLS pada konfigurasi firmware. Simulator host dapat menggunakan No-Auth untuk pengujian pengembangan; jangan memakai hasilnya sebagai bukti autentikasi firmware.
- Integrasi produksi seperti penyimpanan kunci di Secure World, provisioning, dan jalur HLS produksi masih memerlukan integrasi target.
- Akuisisi UART dan perhitungan metrologi menjadi tanggung jawab integrasi board dan tidak divalidasi oleh host test di repository ini.

## Menjalankan host tests

Jalankan dari root repository di PowerShell:

```powershell
cmake -S tests -B tests/build -G Ninja
cmake --build tests/build --parallel
ctest --test-dir tests/build --output-on-failure
```

Host tests tidak memerlukan board STM32. Untuk cakupan setiap suite dan batas validasinya, lihat [tests/test_note.md](tests/test_note.md).

## Menjalankan simulator untuk Gurux DLMSDirector

Simulator pengembangan tersedia sebagai target host:

```powershell
cmake -S tests -B tests/build -G Ninja
cmake --build tests/build --target gurux_simulator
.\tests\build\host-tools\gurux_simulator.exe
```

Simulator mendengarkan raw HDLC-over-TCP di `127.0.0.1:4059`. Di Gurux DLMSDirector pilih koneksi Network/TCP, interface HDLC, alamat host dan port tersebut, Logical Name referencing, serta Authentication None untuk profil simulator ini. Ini bukan DLMS Wrapper dan bukan konfigurasi autentikasi produksi.

Untuk konfigurasi CTT serta cara membaca tes yang dikecualikan atau gagal, lihat bagian Gurux di [tests/test_note.md](tests/test_note.md). Untuk Docker test harness, lihat [docker_test/README.md](docker_test/README.md).

## Area tanggung jawab

Dokumentasi ini berfokus pada security dan protokol komunikasi. Hasil pengukuran, kalibrasi, akurasi metrologi, dan driver UART fisik harus diverifikasi oleh integrasi hardware/metrologi dan tidak dapat disimpulkan dari test simulator.
