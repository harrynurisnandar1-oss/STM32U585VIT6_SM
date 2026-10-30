# STM32U585VIT6 3-Phase Smart Meter (AMI Firmware)

Proyek firmware meteran listrik pintar (*3-phase postpaid Smart Meter*) berbasis mikrokontroler STM32U585VIT6 dan metrologi AFE, mengimplementasikan tumpukan protokol DLMS/COSEM (IEC 62056 / SPLN D3.006:2021) dan arsitektur keamanan ARM Cortex-M33 TrustZone.

## Fitur Utama & Kematangan Arsitektur

- **COSEM Data Model & Katalog Lengkap SPLN D3.006:2021**:
  - Mengintegrasikan seluruh **276 kode OBIS** standar teknis PLN pascabayar (`NonSecure/AMI/Inc/ami_obis_catalog.h`) yang terbagi dalam 22 kategori fungsional.
  - Pembacaan register metrologi 3 fasa (Class 3), Clock (Class 8), Profile Generic untuk Event Log, Billing History, dan Load Profile (Class 7) dengan selective access (rentang waktu Clock & rentang entri/baris).
  - Objek Pemutus Aliran Listrik / Disconnect Control Breaker (`0.0.96.3.10.255`, Class 70).
- **DLMS Association & APDU Engine**:
  - State machine ACSE: negosiasi Conformance dinamis (`Get` + `SelectiveAccess`), max PDU size negotiation, silent drop saat unassociated sesuai standar DLMS CTT.
  - Otentikasi/dekripsi `GLO-GET-REQUEST` / `GLO-SET-REQUEST`, pencegahan replay attack, dan isolasi Security Setup (Class 64).
- **Core Cryptographic Engine & HLS Mechanism 5 (GMAC)**:
  - Cryptographic core mandiri AES-128 NIST FIPS 197, GHASH GF(2^128) NIST SP 800-38D, dan AES-GCM authenticated encryption/decryption dengan IV 96-bit dan Tag 128-bit.
  - Implementasi 4-pass mutual authentication **HLS 5 (GMAC)** OID `2.16.756.5.8.2.5` dengan constant-time tag verification dan volatile memory zeroization (`secure_zeroize`).
  - Terverifikasi 100% lulus Known Answer Test (KAT) NIST SP 800-38D (Test Case 1, 2, dan 4).
- **HDLC Framing Engine**:
  - ISO/IEC 13239 & IEC 62056-46 Frame Format Type 3 (`0xA0`), addressing multi-octet dinamis, dual-stage CRC-16 CCITT (HCS & FCS).
  - Resiliensi **transparent flag `0x7E`** pada payload data dan checksum FCS melalui verifikasi integritas HCS header.
  - Stripping/auto-wrapping LLC sublayer (`0xE6 0xE6 0x00` / `0xE6 0xE7 0x00`), otomasi negosiasi SNRM -> UA parameters, DISC -> UA, dan sequence rotation N(S)/N(R).
- **Hardware Porting Abstraction & FreeRTOS**:
  - Layer abstraksi port (`ami_dlms_port.c`) non-blocking zero-copy untuk ISR UART/DMA ring buffer.
  - Integrasi FreeRTOS worker task (`ami_dlms_freertos_start_task`) dengan CMSIS-RTOS v2 dan StreamBuffer.
- **Host Testing, CI/CD & Docker Containerization**:
  - 6/6 test suite CTest host-native tanpa hardware fisik (100% Passed).
  - Dedicated Docker test harness (`docker_test/`) Alpine multi-stage (23 MB, 0 CVE) dan automated end-to-end Python DLMS client.
  - Profil workspace Gurux DLMSDirector XML (`STM32U585VIT6_SM.gxc`) tersinkronisasi 276 objek PLN.

---

## Gurux DLMSDirector TCP Host Simulator

Untuk memvalidasi stack DLMS/COSEM dan framing HDLC tanpa board fisik, tersedia simulator host lokal yang mendengarkan paket raw HDLC-over-TCP pada port standar DLMS `4059`.

### 1. Build Simulator

Kompilasi target simulator menggunakan toolchain host (MSVC/Clang/GCC) via CMake:

```powershell
cmake -S tests -B tests/build -G Ninja
cmake --build tests/build --target gurux_simulator
```

### 2. Menjalankan Simulator

Jalankan executable simulator dari terminal PowerShell:

```powershell
.\tests\build\host-tools\gurux_simulator.exe
```

*Output saat standby:*
```text
Listening for raw HDLC-over-TCP on 127.0.0.1:4059
```

### 3. Koneksi via Gurux DLMSDirector

Buka aplikasi **GXDLMSDirector**, buka file profil `STM32U585VIT6_SM.gxc` atau konfigurasi device baru dengan parameter berikut:

| Parameter | Nilai Konfigurasi | Keterangan |
|---|---|---|
| **Media** | `Network` | Menggunakan koneksi TCP socket lokal |
| **Host Name / IP** | `127.0.0.1` | Local loopback address |
| **Port** | `4059` | Port standar IEC 62056-47 / DLMS |
| **Interface Type** | `HDLC` | Wajib HDLC (bukan WRAPPER) |
| **Client Address** | `16` (`0x10`) atau `1` (`0x01`) | Public Client (16) / Management Client (1) |
| **Physical Server** | `1` | MAC Destination Address |
| **Logical Server** | `1` | COSEM Logical Device ID |
| **Authentication** | `None` atau `HighGMAC` (HLS 5) | Mendukung No-Auth dan HLS5 GMAC |

Klik tombol **Connect**. Setelah status terkoneksi (**Connected**), gunakan menu **Read Objects** untuk melakukan pembacaan register tegangan, arus, jam meter, dan profil riwayat.

---

## Menjalankan Unit Test (CTest & Docker)

### Menjalankan CTest Host Native:

```powershell
cmake -S tests -B tests/build -G Ninja
cmake --build tests/build --parallel
ctest --test-dir tests/build --output-on-failure
```

### Menjalankan Docker Test Suite (Otomasi E2E):

```powershell
cd docker_test
docker compose up --build
```
