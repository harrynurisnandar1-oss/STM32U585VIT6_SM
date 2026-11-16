# E2 DLMS/COSEM Host Baseline & Standards Alignment

## Status

Dokumen ini mencatat profil protokol target E2, keselarasan terhadap standar teknis resmi PT PLN (Persero) pada folder `STANDAR_PLN/`, serta baseline implementasi yang saat ini aktif dan terverifikasi pada host test. Nilai-nilai di bawah ini merupakan acuan formal arsitektur sebelum validasi perangkat keras fisik target STM32U585.

## Standar Acuan Teknis PLN (`STANDAR_PLN/`)

Implementasi protokol dan arsitektur keamanan DLMS/COSEM diselaraskan secara ketat terhadap dokumen standar resmi:

1. **SPLN D3.006:2021** (*Meter Statik Arus Bolak-Balik Fase Tiga Pasca Bayar Kelas 0,2S, 0,5S, dan 1*):
   - Standar induk persyaratan teknis, metrologi, pengujian isolasi/dielektrik, EMC, dan telemetri meter pascabayar 3-fasa.
   - Dilengkapi dokumen suplemen: **SPLN D3.006:2021 SUP.1:2022** dan **SPLN D3.006:2021 SUP.2:2023**.
2. **SPLN D3.006-2:2021 / SPLN D3.006.2.2021.CSV** (*Struktur Tabel Kode OBIS dan Atribut Parameter Meter Pascabayar*):
   - Acuan pemetaan seluruh 276 kode OBIS (Object Identification System) pada `NonSecure/AMI/Inc/ami_obis_catalog.h`.
   - Dilengkapi suplemen: **SPLN D3.006.2:2021 SUP.1:2022** dan **SPLN D3.006.2:2021 SUP.2:2023**.
3. **SPLN D3.023:2021** (*Spesifikasi Pengujian Interoperabilitas dan Komunikasi Data Smart Meter AMI*):
   - Panduan pengujian kesesuaian protokol komunikasi HDLC, pembentukan frame, timing, dan integrasi Head-End System (HES).
4. **SPLN D5.002:2024** (*Spesifikasi Keamanan Siber Sistem Smart Grid dan Advanced Metering Infrastructure*):
   - Standar mutakhir tata kelola keamanan siber AMI PLN: isolasi hardware kriptografi, mitigasi brute-force, manajemen kunci (Key Lifecycle / KEK), penolakan mutlak transmisi password plaintext (LLS) pada jaringan terbuka, dan penegakan Invocation Counter anti-replay.
5. **IEC 62056 Series** (*DLMS/COSEM International Standards*):
   - IEC 62056-5-3 (Application Layer & Security Suite 0/1 AES-GCM).
   - IEC 62056-6-1 (OBIS Identification System).
   - IEC 62056-6-2 (COSEM Interface Classes).
   - IEC 62056-46 (Data Link Layer using HDLC Protocol).

---

## Selected E2 Target Profile (Firmware Policy)

| Parameter | Konfigurasi Target E2 | Dasar Kepatuhan Standar PLN |
|---|---|---|
| **Addressing Method** | Logical Name (LN) referencing | SPLN D3.006:2021 & IEC 62056 |
| **Client HDLC SAP** | `16` (`0x10`) | Dedicated client SAP profil E2 |
| **Server HDLC Address** | Physical `1`, Logical `1` (Server Address `1`) | IEC 62056-46 / SPLN D3.023:2021 |
| **Client Authentication** | **HLS 5 (GMAC)** OID `2.16.756.5.8.2.5` | SPLN D5.002:2024 & IEC 62056-5-3 |
| **Cipher Suite** | DLMS Security Suite 0 (AES-128-GCM / GMAC) | SPLN D3.006:2021 & SPLN D5.002:2024 |
| **Challenge Length** | Tepat 16 octet (Mutual 4-pass handshake) | IEC 62056-5-3 HLS Mechanism 5 |
| **Layanan Data Normal** | `GET-Request-Normal` & Selective Access Class 7 | SPLN D3.006:2021 (Read-Only Baseline) |
| **Kebijakan Akses** | Read-Only (menolak `SET` dan `ACTION` umum) | Proteksi integritas transaksi penagihan |
| **Manajemen Kunci** | Remote Key Transfer RFC 3394 AES Key Wrap via KEK | SPLN D5.002:2024 & Class 64 Security Setup |
| **Otorisasi Key Transfer** | Wajib otorisasi peer SAP & identitas terautentikasi | Mencegah pergantian kunci tanpa izin HES |

*Catatan Kebijakan:* Firmware target secara fail-closed menolak asosiasi No-Auth dan LLS. Penggunaan HLS5 pada SAP 16 memastikan tidak ada celah akses tanpa autentikasi pada saluran komunikasi data.

---

## Current Host Baseline

| Area | Baseline Terimplementasi & Teruji di Host |
|---|---|
| **Referencing** | Logical Name (LN) referencing terverifikasi |
| **Transport Harness** | Raw HDLC-over-TCP port `4059` pada loopback simulator |
| **Association State Machine** | AARQ/AARE, negosiasi conformance dinamis, mutual 4-pass HLS5 GMAC |
| **Anti-Brute-Force Lockout** | Tracking kegagalan HLS5, penalti waktu progresif (5s s.d. 40s), hard lockout 5x gagal (SPLN D5.002:2024) |
| **Block Transfer (GBT)** | xDLMS Datablock transfer (`0xC0 0x02` / `0xC4 0x02`) untuk payload profil & buffer besar |
| **Data Services** | GET-Request-Normal, Datablock Transfer, dan selective access Class 7 (Load Profile 23-Kanal 15-menit, Tamper Logbook 3 Capture Objects dengan 9 Modus Tamper PLN) |
| **Load Control Breaker** | State Machine Disconnect Control Class 70 (Disconnected, Connected, Ready) & Action Method 1/2 |
<<<<<<< HEAD
| **Access Rights** | Association View komprehensif (66 objek aktif teruji via GET attribute 2 & GBT) |
=======
| **Access Rights** | Association View komprehensif (49 objek aktif teruji via GET attribute 2 & GBT) |
>>>>>>> d4ee38296f41d70969e4a6ee09e5214586a74b3b
| **Host Simulator Auth** | No-Auth hanya untuk lingkungan development/simulator lokal |
| **Firmware Auth Policy** | Menolak No-Auth, LLS, HLS3/MD5, HLS4/SHA-1, dan challenge bukan 16 byte |
| **Data Source** | Snapshot sample data aman untuk verifikasi logic tanpa board ADE9000 |

---

<<<<<<< HEAD
## Objek Terdaftar dalam Association View (66 Objek Aktif)
=======
## Objek Terdaftar dalam Association View (49 Objek Aktif)
>>>>>>> d4ee38296f41d70969e4a6ee09e5214586a74b3b

Association View adalah daftar otoritatif yang diumumkan meter kepada DLMS Client/HES sesuai standar SPLN D3.002:2021, SPLN D3.006-2:2021, dan IEC 62056-6-2:

| No | Class ID | Logical Name (OBIS) | Deskripsi Objek Sesuai Standar PLN Pascabayar |
|---:|---:|---|---|
| 1 | 15 | `0.0.40.0.0.255` | Association LN |
| 2 | 8 | `0.0.1.0.0.255` | Clock (Waktu real-time meter) |
| 3 | 1 | `0.0.42.0.0.255` | Logical Device Name |
| 4 | 1 | `1.0.0.2.0.255` | Software / Firmware Version |
| 5 | 1 | `0.0.96.1.0.255` | Meter Serial Number (Meter ID) |
| 6 | 1 | `0.0.96.1.1.255` | Meter Type |
| 7 | 64 | `0.0.43.0.0.255` | Security Setup |
<<<<<<< HEAD
| 8 | 7 | `0.0.99.98.0.255` | Event Profile (Log kejadian operasional #1 - 2 kolom) |
| 9 | 7 | `1.0.98.1.0.255` | Billing Profile (Data penagihan historikal bulanan) |
| 10 | 7 | `1.0.99.1.0.255` | Load Profile 15-menit (23 kanal) |
| 11 | 7 | `1.96.98.128.0.255` | Instant Profile (11 kanal pengukuran sesaat) |
| 12 | 7 | `1.0.99.14.2.255` | Power Quality Voltage Log Profile Fase R (Sag/Swell R) |
| 13 | 7 | `1.0.99.14.3.255` | Power Quality Voltage Log Profile Fase S (Sag/Swell S) |
| 14 | 7 | `1.0.99.14.4.255` | Power Quality Voltage Log Profile Fase T (Sag/Swell T) |
| 15 | 1 | `1.135.32.0.0.255` | Power Quality Voltage Register Fase R |
| 16 | 1 | `1.135.52.0.0.255` | Power Quality Voltage Register Fase S |
| 17 | 1 | `1.135.72.0.0.255` | Power Quality Voltage Register Fase T |
| 18 | 3 | `1.0.32.7.0.255` | Tegangan sesaat Fase R (Phase A Voltage) |
| 19 | 3 | `1.0.52.7.0.255` | Tegangan sesaat Fase S (Phase B Voltage) |
| 20 | 3 | `1.0.72.7.0.255` | Tegangan sesaat Fase T (Phase C Voltage) |
| 21 | 3 | `1.0.31.7.0.255` | Arus sesaat Fase R (Phase A Current) |
| 22 | 3 | `1.0.51.7.0.255` | Arus sesaat Fase S (Phase B Current) |
| 23 | 3 | `1.0.71.7.0.255` | Arus sesaat Fase T (Phase C Current) |
| 24 | 3 | `1.0.1.8.0.255` | Energi aktif impor total (+A) |
| 25 | 3 | `1.0.2.8.0.255` | Energi aktif ekspor total (-A) |
| 26 | 3 | `1.0.3.8.0.255` | Energi reaktif impor total (+R) |
| 27 | 3 | `1.0.4.8.0.255` | Energi reaktif ekspor total (-R) |
| 28 | 3 | `1.0.1.8.1.255` | Energi aktif impor Tarif 1 (LWBP 1) |
| 29 | 3 | `1.0.1.8.2.255` | Energi aktif impor Tarif 2 (WBP) |
| 30 | 3 | `1.0.1.8.3.255` | Energi aktif impor Tarif 3 (LWBP 2) |
| 31 | 3 | `1.0.1.8.4.255` | Energi aktif impor Tarif 4 |
| 32 | 3 | `1.0.2.8.1.255` | Energi aktif ekspor Tarif 1 |
| 33 | 3 | `1.0.2.8.2.255` | Energi aktif ekspor Tarif 2 |
| 34 | 3 | `1.0.2.8.3.255` | Energi aktif ekspor Tarif 3 |
| 35 | 3 | `1.0.2.8.4.255` | Energi aktif ekspor Tarif 4 |
| 36 | 3 | `1.0.3.8.1.255` | Energi reaktif impor Tarif 1 |
| 37 | 3 | `1.0.3.8.2.255` | Energi reaktif impor Tarif 2 |
| 38 | 3 | `1.0.3.8.3.255` | Energi reaktif impor Tarif 3 |
| 39 | 3 | `1.0.3.8.4.255` | Energi reaktif impor Tarif 4 |
| 40 | 3 | `1.0.14.7.0.255` | Frekuensi Jaringan (Hz) |
| 41 | 3 | `1.0.13.7.0.255` | Power Factor Total |
| 42 | 3 | `1.0.33.7.0.255` | Power Factor Fase R (L1) |
| 43 | 3 | `1.0.53.7.0.255` | Power Factor Fase S (L2) |
| 44 | 3 | `1.0.73.7.0.255` | Power Factor Fase T (L3) |
| 45 | 3 | `1.0.12.7.124.255` | THD Tegangan Total (%) |
| 46 | 3 | `1.0.32.7.124.255` | THD Tegangan Fase R (L1) |
| 47 | 3 | `1.0.52.7.124.255` | THD Tegangan Fase S (L2) |
| 48 | 3 | `1.0.72.7.124.255` | THD Tegangan Fase T (L3) |
| 49 | 3 | `1.0.11.7.124.255` | THD Arus Total (%) |
| 50 | 3 | `1.0.31.7.124.255` | THD Arus Fase R (L1) |
| 51 | 3 | `1.0.51.7.124.255` | THD Arus Fase S (L2) |
| 52 | 3 | `1.0.71.7.124.255` | THD Arus Fase T (L3) |
| 53 | 3 | `1.0.1.7.0.255` | Daya Aktif Sesaat Total (+P, Watt) |
| 54 | 3 | `1.0.3.7.0.255` | Daya Reaktif Sesaat Total (+Q, var) |
| 55 | 71 | `0.0.17.0.3.255` | Demand Limiter Class 71 (Parameter Batas Daya) |
| 56 | 70 | `0.0.96.3.10.255` | Disconnect Control Class 70 (Kendali Pemutusan Beban) |
| 57 | 3 | `1.0.15.8.0.255` | Energi Aktif Total Gabungan (+A + -A) |
| 58 | 3 | `1.0.12.31.0.255` | Batas Tegangan Rendah (Low Voltage Limit) |
| 59 | 3 | `1.0.12.33.0.255` | Tunda Validasi Tegangan Rendah (Low Voltage Validation Delay) |
| 60 | 1 | `1.0.12.32.0.255` | Jumlah Kejadian Tegangan Rendah (Low Voltage Event Count) |
| 61 | 3 | `1.0.13.31.0.255` | Faktor Daya Batas Kompensasi |
| 62 | 6 | `0.0.14.0.0.255` | Tariff Register Activation (Aktivasi Register Tarif) |
| 63 | 11 | `0.0.11.0.0.255` | Special Days Table (Tabel Hari Libur/Khusus) |
| 64 | 20 | `0.0.13.0.0.255` | Activity Calendar (Kalender Aktivitas Tarif) |
| 65 | 22 | `0.1.15.0.4.255` | Single Action Schedule (Penjadwal Data Historikal) |
| 66 | 22 | `0.0.15.0.2.255` | Single Action Schedule (Penjadwal Penagihan) |

*Catatan Objek:* Katalog lengkap mencakup 276 OBIS (`ami_obis_catalog.h`). Seluruh 66 objek yang diekspos pada Association View telah memiliki pemetaan data model riil dan terverifikasi secara fungsional melalui test suite host. Respons object list yang besar secara otomatis disegmentasi via xDLMS DataBlock Transfer (GBT).
=======
| 8 | 7 | `0.0.99.98.0.255` | Event Profile (Log kejadian operasional & proteksi) |
| 9 | 7 | `1.0.98.1.0.255` | Billing Profile (Data penagihan historikal bulanan) |
| 10 | 7 | `1.0.99.1.0.255` | Load Profile 15-menit |
| 11 | 3 | `1.0.32.7.0.255` | Tegangan sesaat Fase R (Phase A Voltage) |
| 12 | 3 | `1.0.52.7.0.255` | Tegangan sesaat Fase S (Phase B Voltage) |
| 13 | 3 | `1.0.72.7.0.255` | Tegangan sesaat Fase T (Phase C Voltage) |
| 14 | 3 | `1.0.31.7.0.255` | Arus sesaat Fase R (Phase A Current) |
| 15 | 3 | `1.0.51.7.0.255` | Arus sesaat Fase S (Phase B Current) |
| 16 | 3 | `1.0.71.7.0.255` | Arus sesaat Fase T (Phase C Current) |
| 17 | 3 | `1.0.1.8.0.255` | Energi aktif impor total (+A) |
| 18 | 3 | `1.0.2.8.0.255` | Energi aktif ekspor total (-A) |
| 19 | 3 | `1.0.3.8.0.255` | Energi reaktif impor total (+R) |
| 20 | 3 | `1.0.4.8.0.255` | Energi reaktif ekspor total (-R) |
| 21 | 3 | `1.0.1.8.1.255` | Energi aktif impor Tarif 1 (LWBP 1) |
| 22 | 3 | `1.0.1.8.2.255` | Energi aktif impor Tarif 2 (WBP) |
| 23 | 3 | `1.0.1.8.3.255` | Energi aktif impor Tarif 3 (LWBP 2) |
| 24 | 3 | `1.0.1.8.4.255` | Energi aktif impor Tarif 4 |
| 25 | 3 | `1.0.2.8.1.255` | Energi aktif ekspor Tarif 1 |
| 26 | 3 | `1.0.2.8.2.255` | Energi aktif ekspor Tarif 2 |
| 27 | 3 | `1.0.2.8.3.255` | Energi aktif ekspor Tarif 3 |
| 28 | 3 | `1.0.2.8.4.255` | Energi aktif ekspor Tarif 4 |
| 29 | 3 | `1.0.3.8.1.255` | Energi reaktif impor Tarif 1 |
| 30 | 3 | `1.0.3.8.2.255` | Energi reaktif impor Tarif 2 |
| 31 | 3 | `1.0.3.8.3.255` | Energi reaktif impor Tarif 3 |
| 32 | 3 | `1.0.3.8.4.255` | Energi reaktif impor Tarif 4 |
| 33 | 3 | `1.0.14.7.0.255` | Frekuensi Jaringan (Hz) |
| 34 | 3 | `1.0.13.7.0.255` | Power Factor Total |
| 35 | 3 | `1.0.33.7.0.255` | Power Factor Fase R (L1) |
| 36 | 3 | `1.0.53.7.0.255` | Power Factor Fase S (L2) |
| 37 | 3 | `1.0.73.7.0.255` | Power Factor Fase T (L3) |
| 38 | 3 | `1.0.12.7.124.255` | THD Tegangan Total (%) |
| 39 | 3 | `1.0.32.7.124.255` | THD Tegangan Fase R (L1) |
| 40 | 3 | `1.0.52.7.124.255` | THD Tegangan Fase S (L2) |
| 41 | 3 | `1.0.72.7.124.255` | THD Tegangan Fase T (L3) |
| 42 | 3 | `1.0.11.7.124.255` | THD Arus Total (%) |
| 43 | 3 | `1.0.31.7.124.255` | THD Arus Fase R (L1) |
| 44 | 3 | `1.0.51.7.124.255` | THD Arus Fase S (L2) |
| 45 | 3 | `1.0.71.7.124.255` | THD Arus Fase T (L3) |
| 46 | 3 | `1.0.1.7.0.255` | Daya Aktif Sesaat Total (+P, Watt) |
| 47 | 3 | `1.0.3.7.0.255` | Daya Reaktif Sesaat Total (+Q, var) |
| 48 | 71 | `0.0.17.0.3.255` | Demand Limiter Class 71 (Parameter Batas Daya) |
| 49 | 70 | `0.0.96.3.10.255` | Disconnect Control Class 70 (Kendali Pemutusan Beban) |

*Catatan Objek:* Katalog lengkap mencakup 276 OBIS (`ami_obis_catalog.h`). Seluruh 49 objek yang diekspos pada Association View telah memiliki pemetaan data model riil dan terverifikasi secara fungsional melalui test suite host. Respons object list yang besar (1427 bytes) secara otomatis disegmentasi via xDLMS DataBlock Transfer (GBT).
>>>>>>> d4ee38296f41d70969e4a6ee09e5214586a74b3b

---

## Panduan Pengujian Conformance (DLMS CTT & Gurux)

1. Gunakan konfigurasi **Logical Name (LN)** dan antarmuka **HDLC** saat menguji dengan GXDLMSDirector.
2. Mode *Authentication None* hanya diperbolehkan pada simulator development host.
3. Seluruh 49 objek dalam Association View wajib merespons `GET` attribute 2 dengan status sukses.
4. Parameter `ExcludeBasicTests` pada CTT harus diset ke `False` untuk audit diagnostik penuh.
5. Dokumentasikan log pengujian secara terpisah antara simulator host dan binary firmware STM32.

---

## Syarat Kelulusan Menuju Fase Produksi (Security Completion Gates)

Sesuai standar SPLN D5.002:2024 dan arsitektur STM32U5 TrustZone, fase produksi mensyaratkan:

1. **Layanan Kriptografi Secure-World:** Integrasi hardware SAES/AES-GCM dan TRNG hardware STM32U585 yang terlindungi di Secure World.
2. **Provisi KEK Manufaktur:** Penyimpanan Master Key (KEK) unik per-meter pada Secure Flash/OTP tanpa plaintext pada image NonSecure.
3. **Atomic Wrapped Key Installation:** Pembongkaran kunci RFC 3394 dan instalasi kunci baru (GUEK/AK) secara atomik di Secure World.
4. **Otorisasi Peer Khusus:** Keputusan otorisasi transfer kunci harus memvalidasi identitas peer terdaftar dan SAP yang diizinkan.
5. **Durable Monotonic Counter:** Penyimpanan Invocation Counter yang tahan pemadaman listrik (*wear-leveling NVM*) guna mencegah replay attack dan *nonce reuse*.
6. **Uji Penetrasi Target STM32U5:** Verifikasi penolakan serangan manipulasi frame, tampered tag, pemadaman saat penulisan kunci, dan kegagalan TRNG.
