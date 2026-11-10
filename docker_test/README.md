# Panduan Koneksi GXDLMSDirector ke Docker Simulator

Container Docker `stm32u585vit6sm` kini berukuran sangat kecil (**< 10 MB**) dan berfungsi khusus menjalankan **Gurux DLMS/COSEM HDLC-over-TCP Simulator** pada port `4059`.

Seluruh aplikasi pengujian seperti **GXDLMSDirector** (atau skrip uji Python) dijalankan langsung dari **PC Windows Host Anda**, tanpa membebani container.

---

## 1. Menjalankan Simulator di Docker

Dari root repository atau folder ini:
```powershell
.\docker_test\run_docker_tests.bat
```
Atau via Docker Compose:
```bash
docker compose up -d
```
Simulator akan langsung aktif di latar belakang mendengarkan pada port `127.0.0.1:4059`.

---

## 2. Pengaturan Koneksi di GXDLMSDirector (Windows)

Buka aplikasi **GXDLMSDirector** di PC Windows Anda, lalu buat device baru dengan konfigurasi berikut:

1. **General Settings**:
   - **Manufacturer**: `Generic` (atau `Gurux`)
   - **Authentication**: `None` (untuk pengujian fungsional dasar) atau `High (GMAC)`
   - **Logical Name Referencing (LN)**: Centang (`True`)
   - **Interface Type**: `HDLC`
   - **Client Address**: `16` (Hex `0x10` - Management Client) atau `1` (Public)
   - **Server Address**: `1` (Logical Address 1)

2. **Communication (Media)**:
   - Pilih **Media**: `Network` (TCP/IP)
   - **Host Name / IP Address**: `127.0.0.1` (atau `localhost`)
   - **Port**: `4059`

3. **Koneksi**:
   - Klik tombol **Connect**.
   - Setelah status berubah menjadi *Connected*, klik kanan pada nama perangkat lalu pilih **Read Objects / Association View**.
   - Seluruh **49 Objek OBIS**, Load Profile 23 kanal, dan Tamper Log akan terbaca secara interaktif.

---

## 3. Menjalankan Uji Klien Python dari Windows Host (Opsional)

Jika Anda ingin menjalankan script automated test client dari Windows host:
```powershell
python .\docker_test\test_dlms_e2e_client.py
```

---

## 4. Menghentikan Simulator

Untuk menghentikan container simulator:
```bash
docker compose down
```
