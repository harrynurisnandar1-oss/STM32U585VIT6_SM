# Docker Automated Test Environment for AMI Smart Meter (Phase 2)

Direktori ini menyediakan lingkungan pengujian terisolasi berbasis **Docker** untuk memverifikasi logika komunikasi protokol DLMS/COSEM, HDLC framing, dan lapisan abstraksi hardware/porting (Phase 2) secara deterministik pada sistem operasi Linux tanpa bergantung pada toolchain atau OS host.

Folder ini terpisah dari folder `tests/` agar tidak mengubah maupun menimpa pengujian native host yang sudah ada.

---

## Cakupan Pengujian di Docker

1. **Unit Testing CTest (6 Test Suites)**:
   - `AmiDlmsPortTest`: Pengujian *porting layer* (`ami_dlms_port.c`), validasi parsing stream byte UART, dispatching frame SNRM/DISC, dan respon AARQ/GET.
   - `AmiCosemTest`: Pengujian pemodelan data objek COSEM (Class 3, Class 8, Class 7 Profile Generic).
   - `AmiSecurityTest`: Pengujian batas kriptografi dan sanitasi memori.
   - `AmiDlmsAssociationTest`: Pengujian state machine asosiasi DLMS AARQ/AARE.
   - `AmiDlmsApduTest`: Pengujian xDLMS GET/SET/Action requests dan ciphered APDU.
   - `AmiDlmsHdlcTest`: Pengujian HDLC framing (Type 3 `0xA0`, CRC-16 CCITT HCS/FCS, dan addressing).

2. **End-to-End (E2E) DLMS Client Simulation (`test_dlms_e2e_client.py`)**:
   - Menjalankan TCP simulator (`gurux_simulator`) pada port `4059`.
   - Menjalankan automated client script yang mensimulasikan software eksternal (seperti Gurux DLMSDirector):
     - Pertukaran handshake HDLC `SNRM` -> `UA`.
     - Inisiasi asosiasi DLMS `AARQ` (No-Auth) -> `AARE`.
     - Pembacaan register Tegangan Fasa R (`1.0.32.7.0.255`, Class 3) -> verifikasi data 230V.
     - Pembacaan Jam Meter (*Clock* `0.0.1.0.0.255`, Class 8).
     - Pemutusan koneksi bersih `DISC` -> `UA`.

---

## Cara Menjalankan

### Cara 1: Menggunakan Script Otomatis (Windows PowerShell / CMD)

Jalankan script batch dari folder `docker_test/`:

```powershell
.\docker_test\run_docker_tests.bat
```

### Cara 2: Menggunakan Docker CLI Manual

Dari root direktori repositori:

```bash
# 1. Build image Docker
docker build -t ami-smart-meter-test -f docker_test/Dockerfile .

# 2. Jalankan container
docker run --rm ami-smart-meter-test
```

### Cara 3: Menggunakan Docker Compose

Dari folder `docker_test/`:

```bash
docker compose up --build
```

---

## Arsitektur Porting Hardware (Phase 2 Readiness)

Lapisan abstraksi hardware disiapkan di [`NonSecure/AMI/Inc/ami_dlms_port.h`](../NonSecure/AMI/Inc/ami_dlms_port.h) dan [`NonSecure/AMI/Src/ami_dlms_port.c`](../NonSecure/AMI/Src/ami_dlms_port.c):

- **FreeRTOS Integration**: Menyediakan FreeRTOS StreamBuffer dan task dedicated (`AmiDlmsTask`) yang siap dihubungkan langsung ke peripheral UART fisik saat PCB tiba.
- **Interrupt Service Routine (ISR)**: Fungsi aman interupsi `ami_dlms_uart_rx_isr()` / `ami_dlms_uart_rx_buffer_isr()` untuk menerima data dari UART DMA atau RX interrupt tanpa overhead blocking.
- **Hardware Agnostic**: Memungkinkan testing 100% pada simulator dan container Docker sebelum di-flash ke mikrokontroler STM32U585.

