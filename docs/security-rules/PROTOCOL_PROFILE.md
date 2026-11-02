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
| **Data Services** | GET-Request-Normal dan selective access Class 7 (waktu & entri baris) |
| **Access Rights** | Association View read-only (12 objek aktif teruji via GET attribute 2) |
| **Host Simulator Auth** | No-Auth hanya untuk lingkungan development/simulator lokal |
| **Firmware Auth Policy** | Menolak No-Auth, LLS, HLS3/MD5, HLS4/SHA-1, dan challenge bukan 16 byte |
| **Data Source** | Snapshot sample data aman untuk verifikasi logic tanpa board ADE9000 |

---

## Objek Terdaftar dalam Association View (12 Objek Aktif)

Association View adalah daftar otoritatif yang diumumkan meter kepada DLMS Client/HES:

| Class ID | Logical Name (OBIS) | Deskripsi Objek Sesuai SPLN D3.006-2:2021 |
|---:|---|---|
| 15 | `0.0.40.0.0.255` | Association LN |
| 8 | `0.0.1.0.0.255` | Clock (Waktu real-time meter) |
| 3 | `1.0.32.7.0.255` | Tegangan sesaat Fase R (Phase A Voltage) |
| 3 | `1.0.52.7.0.255` | Tegangan sesaat Fase S (Phase B Voltage) |
| 3 | `1.0.72.7.0.255` | Tegangan sesaat Fase T (Phase C Voltage) |
| 3 | `1.0.31.7.0.255` | Arus sesaat Fase R (Phase A Current) |
| 3 | `1.0.51.7.0.255` | Arus sesaat Fase S (Phase B Current) |
| 3 | `1.0.71.7.0.255` | Arus sesaat Fase T (Phase C Current) |
| 3 | `1.0.1.8.0.255` | Energi aktif impor total (+A) |
| 3 | `1.0.2.8.0.255` | Energi aktif ekspor total (-A) |
| 7 | `0.0.99.98.0.255` | Event Profile (Log kejadian operasional & proteksi) |
| 7 | `1.0.98.1.0.255` | Billing Profile (Data penagihan historikal) |

*Catatan Objek:* Katalog lengkap mencakup 276 OBIS (`ami_obis_catalog.h`), namun objek yang diekspos pada Association View dibatasi pada objek yang fungsionalitas dan handler datanya telah teruji.

---

## Panduan Pengujian Conformance (DLMS CTT & Gurux)

1. Gunakan konfigurasi **Logical Name (LN)** dan antarmuka **HDLC** saat menguji dengan GXDLMSDirector.
2. Mode *Authentication None* hanya diperbolehkan pada simulator development host.
3. Seluruh 12 objek dalam Association View wajib merespons `GET` attribute 2 dengan status sukses.
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
