## Ringkasan Perubahan (Pull Request)

<!-- Berikan ringkasan singkat mengenai perubahan kode atau fitur yang diajukan -->

### Kategori Perubahan:
- [ ] Fitur Baru Protokol DLMS / Objek COSEM
- [ ] Peningkatan Keamanan / TrustZone / Kriptografi (Suite 0 / HLS5)
- [ ] Driver Hardware & Metrologi (ADE9000 / UART DMA / GPIO Relay)
- [ ] Pengujian Unit CTest / Simulator Gurux / Docker
- [ ] Dokumentasi & Refactoring

---

### Acuan Standar yang Dipatuhi:
- [ ] **SPLN D3.006:2021** (Spesifikasi Meter Listrik Cerdas Terpusat)
- [ ] **SPLN D5.002:2024** (Keamanan & Kriptografi AMI PLN)
- [ ] **IEC 62056** (DLMS/COSEM Suite)
- [ ] **NIST SP 800-38D / 800-38F** (GCM / Key Wrap)

---

### Checklist Kualitas Kode & Keamanan (Mandatori):
- [ ] **Bebas Kompilasi Peringatan & Error**: Cross-compilation ARM Cortex-M33 TrustZone (`STM32U585VIT6_SM_S.elf` dan `STM32U585VIT6_SM_NS.elf`) sukses dengan **0 error dan 0 warning**.
- [ ] **Validasi Batas Memori TrustZone**: Seluruh pointer NonSecure yang masuk ke gateway Secure World divalidasi dengan `cmse_check_address_range()`.
- [ ] **Zeroization Data Rahasia**: Buffer kerja plaintext kunci atau frame kripto dibersihkan dengan `secure_memzero()` sebelum fungsi kembali.
- [ ] **Proteksi Anti-Replay**: Verifikasi strictly monotonic counter (`rx_counter > last_rx_counter`) tetap ditegakkan tanpa ada bypass.
- [ ] **Unit Tests**: Seluruh suite CTest host (14/14 tests) lulus 100%.

---

### Bukti Pengujian (Test Results):
<!-- Tempelkan output eksekusi CTest atau screenshot simulasi Gurux jika relevan -->
```text
Test project .../tests/build
100% tests passed, 0 tests failed out of 14
```

