# Docker Host Test Harness

Direktori ini menjalankan host tests dan uji end-to-end simulator dalam container. Tujuannya adalah menguji logika protokol secara berulang tanpa board atau toolchain STM32.

## Cakupan

Docker memakai konfigurasi CMake terpisah di direktori ini. Runner menjalankan enam executable test: port, COSEM, security, association, APDU, dan HDLC; setelah itu runner menjalankan client E2E terhadap `gurux_simulator`. Test kebijakan autentikasi firmware yang ada di suite native `tests/` belum dimasukkan ke Docker harness.

E2E simulator menguji pertukaran protokol menggunakan data contoh. Ini bukan uji CTT penuh dan tidak menguji akurasi metrologi, UART fisik, timing board, atau keamanan produksi Secure-world.

Simulator pengembangan menggunakan No-Auth. Firmware target menolak No-Auth dan LLS, sehingga hasil simulator tidak boleh dianggap sebagai hasil autentikasi firmware.

## Menjalankan

Dari root repository:

```powershell
docker build -t ami-smart-meter-test -f docker_test/Dockerfile .
docker run --rm ami-smart-meter-test
```

Atau gunakan script Windows:

```powershell
.\docker_test\run_docker_tests.bat
```

Atau Docker Compose:

```powershell
docker compose -f docker_test/docker-compose.yml up --build
```

## Batas tanggung jawab

Container menguji kode host dan data sintetis. Validasi pengukuran/kalibrasi dan driver UART pada perangkat dilakukan melalui integrasi hardware terpisah. Untuk menjalankan Gurux DLMSDirector dan memahami CTT, lihat [panduan test](../tests/test_note.md).
