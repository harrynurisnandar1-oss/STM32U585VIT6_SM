# Kode Etik & Pedoman Perilaku Kontributor (Code of Conduct)
## Proyek Smart Meter 3-Fasa AMI (STM32U585VIT6 + ADE9000)

---

## 1. Komitmen Kami

Sebagai kontributor, pengembang, dan pengelola proyek **Smart Meter 3-Fasa AMI (STM32U585VIT6 + ADE9000)**, kami berkomitmen untuk menciptakan dan memelihara lingkungan kerja sama yang terbuka, aman, profesional, inklusif, dan bebas dari pelecehan atau diskriminasi bagi setiap orang, tanpa memandang usia, jenis kelamin, identitas gender, orientasi seksual, disabilitas, penampilan fisik, ras, etnis, agama, atau tingkat pengalaman teknis.

---

## 2. Standar Perilaku Positif

Contoh perilaku yang mendukung lingkungan proyek yang sehat dan produktif meliputi:

* **Saling Menghargai & Berkomunikasi Terbuka**: Menggunakan bahasa yang santun, profesional, dan empatik dalam setiap diskusi masalah (*issues*), ulasan kode (*code reviews*), dan rapat tim.
* **Fokus pada Fakta Teknis & Keselamatan**: Mengutamakan kualitas teknis, keselamatan kerja listrik tegangan tinggi, kepatuhan standar metrologi (SPLN / IEC), serta keterandalan keamanan siber.
* **Menerima Masukan Konstruktif Secara Elegan**: Terbuka terhadap kritik teknis yang membangun demi peningkatan mutu arsitektur dan keandalan sistem.
* **Kolaborasi Tanpa Ego**: Berorientasi pada keberhasilan tim, berbagi wawasan, membantu anggota tim lain yang sedang menghadapi kendala, serta menghargai kontribusi setiap peran (Hardware, Firmware, QA, Metrologi).

---

## 3. Perilaku yang Tidak Dapat Ditoleransi

Perilaku berikut dikategorikan sebagai pelanggaran dan dilarang keras dalam proyek ini:

* Penggunaan bahasa, gambar, atau konten bernuansa seksual, merendahkan, atau menyinggung SARA.
* *Trolling*, pelecehan verbal, komentar yang menghina (*derogatory*), serta serangan pribadi (*ad hominem*).
* Pelecehan publik atau pribadi dalam bentuk apa pun.
* Mempublikasikan data pribadi orang lain (seperti nomor kontak, alamat rumah, atau kredensial rahasia) tanpa izin eksplisit (*doxxing*).
* Tindakan tidak bertanggung jawab terkait keselamatan laboratorium listrik, memanipulasi data uji metrologi secara tidak jujur, atau membahayakan integritas sistem secara sengaja.
* Perilaku lain yang secara wajar dinilai tidak profesional dalam konteks rekayasa teknis.

---

## 4. Etika Khusus Pengembangan Sistem Kritis (Safety & Integrity)

Mengingat proyek ini melibatkan perangkat listrik 3-fasa tegangan tinggi ($230/400\text{ VAC}$) dan sistem penagihan utilitas kritis:

1. **Integritas Metrologi**: Dilarang sengaja memasukkan logika manipulasi pengukuran daya/energi yang melanggar standar legal metrologi (SPLN D3.006 / IEC 62053).
2. **Kepatuhan Keamanan Siber (*Fail-Closed*)**: Setiap kontribusi yang menyangkut kriptografi, TrustZone, dan manajemen kunci wajib mematuhi arsitektur keamanan SPLN D5.002:2024 dan tidak boleh menurunkan postur keamanan demi jalan pintas (*no hardcoded keys, no bypass*).
3. **Keselamatan Kerja di Laboratorium**: Setiap eksperimen tegangan tinggi wajib mengikuti aturan *two-person rule* dan mematuhi otorisasi *Safety Gate*.

---

## 5. Tanggung Jawab Pengelola (*Maintainers*)

Pengelola proyek bertanggung jawab untuk memperjelas dan menegakkan standar perilaku yang dapat diterima, serta mengambil tindakan korektif yang wajar dan proporsional terhadap setiap perilaku yang melanggar pedoman ini.

Pengelola memiliki hak dan tanggung jawab untuk menghapus, mengedit, atau menolak komentar, commit, kode, pull request, atau issue yang tidak sejalan dengan Pedoman Perilaku ini.

---

## 6. Pelaporan Pelanggaran & Penegakan

Insiden perilaku yang tidak pantas, intimidasi, atau pelecehan dapat dilaporkan secara tertutup kepada pimpinan program atau koordinator proyek. Semua laporan akan ditinjau secara adil, rahasia, dan secepatnya.

Tindakan penegakan dapat berupa:
1. **Peringatan / Klarifikasi**: Peringatan tertulis secara tertutup mengenai perilaku yang melanggar standar profesional.
2. **Tindakan Korektif**: Pembatalan atau penolakan kontribusi terkait.
3. **Pembatasan Sementara**: Penangguhan akses sementara dari repositori atau ruang diskusi proyek.
4. **Pemutusan Permanen**: Pencabutan hak akses dan pemblokiran permanen dari seluruh aktivitas proyek.

---

## 7. Referensi & Lisensi

Pedoman Perilaku ini diadaptasi dari **Contributor Covenant**, versi 2.1 (https://www.contributor-covenant.org/version/2/1/code_of_conduct.html), yang disesuaikan dengan kebutuhan rekayasa perangkat keras dan lunak tertanam (*embedded system*) utilitas kelistrikan.
