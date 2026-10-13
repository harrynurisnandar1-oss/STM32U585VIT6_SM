# STM32U585VIT6 3-Phase Smart Meter (AMI Firmware)

Proyek firmware meteran listrik pintar (*3-phase postpaid Smart Meter*) berbasis mikrokontroler STM32U585VIT6 dan metrologi AFE 9000, mengimplementasikan tumpukan protokol DLMS/COSEM (IEC 62056 / SPLN D3.006:2021) dan arsitektur keamanan TrustZone.

## Fitur Utama Sprint 1 (Phase 1)

- **COSEM Data Model**: Pembacaan register metrologi 3 fasa (Class 3), Clock (Class 8), Profile Generic untuk Event Log & Billing History (Class 7) dengan selective access.
- **DLMS Association & APDU Engine**: Mendukung AARQ/AARE (No-Auth, LLS, HLS GMAC), otentikasi/dekripsi `GLO-GET-REQUEST` / `GLO-SET-REQUEST`, pencegahan serangan replay, serta isolasi Security Setup (Class 64).
- **HDLC Framing Engine**: ISO/IEC 13239 & IEC 62056-46 Frame Format Type 3 (`0xA0`), addressing multi-octet dinamis, dual-stage CRC-16 CCITT (HCS & FCS), stripping/auto-wrapping LLC sublayer (`0xE6 0xE6 0x00` / `0xE6 0xE7 0x00`), dan negosiasi parameter SNRM/UA.
- **Host Testing & Simulator**: Suite unit-test CTest host-native tanpa hardware fisik serta interactive HDLC-over-TCP host harness/simulator untuk pengujian langsung dengan DLMS client pihak ketiga.

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

Buka aplikasi **GXDLMSDirector**, buat atau konfigurasi device dengan parameter berikut:

| Parameter | Nilai Konfigurasi | Keterangan |
|---|---|---|
| **Media** | `Network` | Menggunakan koneksi TCP socket lokal |
| **Host Name / IP** | `127.0.0.1` | Local loopback address |
| **Port** | `4059` | Port standar IEC 62056-47 / DLMS |
| **Interface Type** | `HDLC` | Wajib HDLC (bukan WRAPPER) |
| **Client Address** | `16` (`0x10`) | Public/Management Client SAP |
| **Physical Server** | `1` | MAC Destination Address |
| **Logical Server** | `1` | COSEM Logical Device ID |
| **Authentication** | `None` | Profil development unauthenticated |

Klik tombol **Connect**. Setelah status terkoneksi (**Connected**), gunakan menu **Read Objects** untuk melakukan pembacaan register tegangan, arus, jam meter, dan profil riwayat.

---

## Menjalankan Unit Test (CTest)

Pengujian logika inti protokol dan model data pada PC host:

```powershell
cmake -S tests -B tests/build -G Ninja
cmake --build tests/build --parallel
ctest --test-dir tests/build --output-on-failure
```

