# Unit Testing AMI Smart Meter

Dokumen ini menjelaskan pengujian host-native untuk model DLMS/COSEM dan batas keamanan pada proyek STM32U585. Test dikompilasi untuk PC host melalui CMake, lalu dijalankan dengan CTest. Test ini tidak membutuhkan board, FreeRTOS runtime, Docker, atau CMock.

## Cakupan test

| Suite CTest | File test | Yang diverifikasi |
|---|---|---|
| `AmiCosemTest` | `test_ami_cosem.c` | Pembacaan tegangan/arus/energi Class 3, OBIS, scaler dan unit, validity mask, error object/attribute, serta Clock Class 8. |
| `AmiSecurityTest` | `test_ami_security.c` | Validasi backend, RNG dan pembersihan buffer, batas nonce/tag AES-GCM, kegagalan backend, serta zeroization saat autentikasi gagal. |
| `AmiDlmsAssociationTest` | `test_ami_dlms_association.c` | Penerimaan AARQ logical-name no-auth, pembentukan AARE, penolakan AARQ malformed/ber-auth, state transition, dan reset association. |
| `AmiDlmsApduTest` | `test_ami_dlms_apdu.c` | Get-Request-Normal untuk tegangan OBIS `1.0.32.7.0.255`, encoding Get-Response-Normal dengan nilai `230000` mV, access error untuk OBIS tidak dikenal, penolakan plaintext setelah HLS, pemrosesan global-ciphered GET/SET, tag rusak, dan anti-replay. |

Test model dan protokol menggunakan data snapshot development yang diberikan langsung oleh test. Test tidak membuktikan perilaku metrologi hardware maupun komunikasi UART/HDLC.

## Menjalankan test

Dari PowerShell pada root proyek:

```powershell
cd D:\STM32U585VIT6_SM
cmake -S tests -B tests/build -G Ninja
cmake --build tests/build --parallel
ctest --test-dir tests/build --output-on-failure
```

Untuk menjalankan satu suite saja:

```powershell
ctest --test-dir tests/build -R AmiDlmsAssociationTest --verbose
ctest --test-dir tests/build -R AmiDlmsApduTest --verbose
ctest --test-dir tests/build -R AmiCosemTest --verbose
ctest --test-dir tests/build -R AmiSecurityTest --verbose
```

Test host terpisah dari build firmware CubeMX. Build firmware tetap dijalankan dari preset/folder build STM32, bukan dari `tests/build`.

## Batas cakupan protokol Sprint 1

Implementasi saat ini mencakup AARQ/AARE no-auth, LLS, configurable HLS, dan xDLMS Get-Request-Normal tanpa selective access. Belum mencakup framing/alamat HDLC, negosiasi InitiateRequest/InitiateResponse, atau integrasi ke task FreeRTOS/UART. Ciphering APDU bergantung pada callback AES-GCM Secure-world dan kebijakan invocation counter yang harus disediakan oleh profil produk; test host memakai fake backend dan bukan validasi kriptografi produksi. Karena itu hasil test adalah verifikasi unit host, bukan klaim kesesuaian penuh IEC 62056 atau validasi perangkat akhir.

## GitHub Actions

Workflow `../.github/workflows/ci.yml` berjalan otomatis saat ada push atau pull request, dan dapat dijalankan manual melalui `workflow_dispatch`. Workflow menggunakan Ubuntu runner, membangun target test host, lalu menjalankan seluruh suite CTest. Workflow ini tidak membangun firmware ARM dan tidak memerlukan Docker.


