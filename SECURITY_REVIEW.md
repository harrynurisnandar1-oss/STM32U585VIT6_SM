# Tinjauan Keamanan — STM32U585VIT6_SM

**Tanggal tinjauan:** 3 November 2026
**Cakupan:** tinjauan statis source, konfigurasi build, dan dokumentasi.
**Batasan:** tidak ada perangkat atau jaringan nyata yang diuji; tidak ada
exploit yang dijalankan.

## Ringkasan

Project ini **belum dapat dinyatakan memenuhi CIA Triad atau siap produksi**.
Ada cacat implementasi pada jalur HLS dan otorisasi key-management, serta
kesenjangan pada penyimpanan kunci/counter, RNG, dan secure boot. Namun,
jalur serang jarak jauh pada firmware saat ini belum terbukti: task belum
terhubung ke transport fisik yang berfungsi dan cipher backend tidak terpasang
ke port dengan benar.

| Prinsip | Risiko utama | Status |
|---|---|---|
| **Confidentiality (Kerahasiaan)** | Kunci default/volatile; struktur kunci mentah di NonSecure | Belum siap produksi |
| **Integrity (Integritas)** | Callback HLS tidak memverifikasi proof; otorisasi NSC lemah; counter NVM rentan gagal terbuka | Temuan source-level, jalur remote belum terbukti |
| **Availability (Ketersediaan)** | Error handler berhenti tanpa pemulihan/watchdog; ketahanan DoS belum diuji | Risiko operasional, exploitability belum dibuktikan |

Severity menunjukkan dampak potensial **bila jalur terkait diaktifkan**.
Confidence menunjukkan keyakinan atas analisis source, bukan kepastian bahwa
serangan dapat dilakukan dari jaringan.

## Temuan

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

### Availability — Ketersediaan

Belum ada DoS remote yang terkonfirmasi dari tinjauan statis. Namun,
`Error_Handler()` masuk loop tanpa akhir dan watchdog/recovery tidak terlihat
di startup yang ditinjau ([`Secure/Core/Src/main.c`](./Secure/Core/Src/main.c:234),
[`NonSecure/Core/Src/main.c`](./NonSecure/Core/Src/main.c:173)). Kegagalan
inisialisasi dapat membuat layanan tidak pulih sampai reset. Batas resource,
timeout, dan ketahanan terhadap beban/protokol malformed perlu diuji terpisah.

## Status integrasi dan batas verifikasi

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

## Prioritas tindak lanjut

1. Hapus/gate HLS proof stub; gagal tertutup sampai verifier Secure yang benar
   terhubung. Pastikan aksi kontrol beban memerlukan autentikasi pesan yang
   valid dan otorisasi spesifik.
2. Hilangkan token provisioning publik, default operational keys, dan HLS
   global flag; ikat operasi Secure ke sesi/otorisasi tepercaya.
3. Simpan kunci dan counter secara persisten di storage terlindungi; tolak
   record corrupt, kegagalan tulis/init/bind, dan counter overflow tanpa
   fallback ke nilai rendah/RAM.
4. Hapus fallback LCG untuk operasi keamanan dan kembalikan error jika RNG
   tidak sehat.
5. Hapus raw-key API/struktur dari NonSecure production build; gunakan handle
   dan operasi Secure.
6. Tambahkan dan verifikasi secure boot, signature, serta anti-rollback
   sebagai bagian rantai boot.
7. Setelah perbaikan source, lakukan validasi hardware berizin untuk SRAM3,
   TAMP, DMA/CMSE, RNG, boot configuration, dan pemulihan error.
