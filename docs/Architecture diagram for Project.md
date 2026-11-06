[![Architecture diagram of harrynurisnandar1-oss/stm32u585vit6_sm](https://gitdiagram.com/harrynurisnandar1-oss/stm32u585vit6_sm/diagram.png)](https://gitdiagram.com/harrynurisnandar1-oss/stm32u585vit6_sm?utm_source=readme&utm_medium=picture)


# 📐 Diagram Arsitektur Lengkap Sistem Smart Meter 3-Fasa AMI
**Target: STM32U585VIT6 (ARM Cortex-M33 TrustZone) + ADE9000 AFE**  
*Kepatuhan Standar: SPLN D3.006:2021, SPLN D5.002:2024, IEC 62056 (DLMS/COSEM)*

---

## 1. Diagram Blok Sistem End-to-End (Topologi Fisik & Logika)

```mermaid
flowchart TB
    subgraph OUTSIDE["🌐 DUNIA LUAR / EKSTERNAL"]
        HES["🏢 PLN Head-End System (HES) / MDMS"]
        HHU["📱 Handheld Unit (HHU) / Teknisi P2TL"]
        GRID["⚡ Jala-Jala Listrik 3-Fasa 230/400V AC"]
    end

    subgraph INTERFACE["🔌 ANTARMUKA FISIK & SENSOR (SPIN 1 / SPIN 2)"]
        MODEM["📡 Kompartemen Modem Modular\n(4G Cat-M1 / NB-IoT / RF Mesh)\nRJ-45 8-Pin RS-232 (19.200 bps)"]
        OPTICAL["🔦 Port Optik Inframerah Depan\nIEC 62056-21 (9.600 bps)\nIsolasi Galvanis 4 kV"]
        AFE["⚡ Analog Front-End ADE9000\n7-Kanal 24-bit ADC, SPI Terisolasi\nCT + Voltage Divider Sensing"]
        BREAKER["🔘 Contactor / Relay Breaker\nDisconnect Control (Class 70)"]
    end

    HES <-->|Seluler / Nirkabel| MODEM
    HHU <-->|Inframerah 900nm| OPTICAL
    GRID -->|Tegangan & Arus| AFE
    BREAKER -->|Putus / Sambung Aliran Listrik| GRID

    subgraph MCU["🔲 MIKROKONTROLER STM32U585VIT6 (ARM Cortex-M33 TrustZone)"]
        
        subgraph NONSECURE["🟢 NON-SECURE WORLD (Aplikasi Meter & Protokol)"]
            subgraph DRIVERS["Hardware / BSP Drivers"]
                UART_DRV["Driver UART & DMA Circular Buffer\n(ami_bsp_uart.c)\nLPUART1 / USART2 & USART3"]
                ADE_DRV["Driver Metrologi ADE9000 SPI\n(ami_ade9000.c)\nFail-Closed Snapshot Feeder"]
                RELAY_DRV["Driver Aktuator Disconnect Control\n(ami_disconnect_control.c)"]
            end

            subgraph RTOS["FreeRTOS Kernel Tasks"]
                DLMS_TASK["Task Protokol DLMS/COSEM\n(ami_dlms_port.c)\nStreamBuffer & Message Queue"]
                MET_TASK["Task Metrologi & Sampling\nPeriodik Baca RMS, Daya, Energi"]
            end

            subgraph DLMS_STACK["DLMS/COSEM Protocol Engine"]
                HDLC["HDLC Parser Type 3 (0xA0)\nCRC-16 HCS/FCS, Transparent 0x7E\nZero-Heap O(1)"]
                ACSE["ACSE Association Machine\nAARQ/AARE, Max PDU, Dynamic Conformance"]
                ABF["Anti-Brute-Force & Lockout\nPenalti Eksponensial (5s..40s)\nHard Lockout 5x Gagal (SPLN D5.002)"]
                GBT["General Block Transfer (GBT)\nxDLMS Datablock (0xC0/0xC4 0x02)\nSegmentasi Data Profil Besar"]
                COSEM["COSEM Object Model (49 Objek PLN)\n- Load Profile 15-menit (23 Kanal)\n- Tamper Logbook (9 Modus PLN)\n- Billing Profile (Class 7)\n- Multi-Tarif WBP/LWBP (Tarif 1..4)\n- Demand Limiter (Class 71)"]
            end

            UART_DRV -->|Byte Stream IRQ| RTOS
            ADE_DRV -->|Snapshot Data| RTOS
            RTOS --> HDLC --> ACSE --> DLMS_STACK
            DLMS_STACK --> RELAY_DRV
        end

        subgraph GATEWAY["🔐 GERBANG NON-SECURE CALLABLE (NSC)"]
            NSC_API["secure_nsc.c / secure_nsc.h\n- cmse_check_address_range()\n- Atomic Single-Fetch Copy Parameter\n- Strict Pointer Validation"]
        end

        subgraph SECURE["🔴 SECURE WORLD (TrustZone Protected Domain)"]
            SAU["Security Attribution Unit (SAU)\nIsolasi Memori SRAM3 & Flash"]
            CRYPTO["Core Kriptografi Suite 0\n- AES-128-GCM (glo-get/set/action)\n- HLS 5 GMAC (Mutual 4-Pass Auth)\n- Key Unwrap RFC 3394 (Master Key KEK)\n- Memory Zeroization (secure_zeroize)"]
            TRNG["Hardware TRNG Fail-Closed\nGenerator Acak 16-Byte Challenge"]
            NVM_SEC["Wear-Leveling Anti-Replay Engine\n- Strictly Monotonic Invocation Counter\n- TAMP Backup Register / NVM\n- Saturation Guard (0xFFFFFFFF Fail-Closed)"]
        end

        DLMS_STACK <-->|Permintaan Kripto / Auth / Key| NSC_API
        NSC_API <-->|Eksekusi Terisolasi| CRYPTO
        NSC_API <-->|Nomor Acak Challenge| TRNG
        NSC_API <-->|Pencatatan Counter Monotonik| NVM_SEC
    end

    MODEM <--> UART_DRV
    OPTICAL <--> UART_DRV
    AFE <--> ADE_DRV
    RELAY_DRV <--> BREAKER
```

---

## 2. Diagram Aliran Data Protokol & Otentikasi HLS5 (GMAC)

```mermaid
sequenceDiagram
    autonumber
    participant Client as 🏢 HES / DLMS Client (SAP 16)
    participant HDLC as 🟢 HDLC Layer (NonSecure)
    participant Port as 🟢 DLMS Port / ACSE
    participant NSC as 🔐 TrustZone NSC Gateway
    participant Secure as 🔴 Secure World (Crypto Core)

    Note over Client,HDLC: 1. Handshake HDLC Link Layer
    Client->>HDLC: SNRM Frame (Format 3, Client=16, Server=1)
    HDLC->>Client: UA Frame (Negosiasi Max Frame Size 512B)

    Note over Client,Port: 2. Handshake Asosiasi ACSE
    Client->>HDLC: I-Frame: AARQ (Mechanism OID: HLS 5 GMAC)
    HDLC->>Port: Ekstrak APDU AARQ
    Port->>NSC: SECURE_TRNG_GetBytes(16 bytes)
    NSC->>Secure: Baca TRNG Hardware STM32U5
    Secure-->>Port: 16-Byte Server Challenge (StoC)
    Port-->>HDLC: Bentuk APDU AARE (Status: Auth_Pending, StoC)
    HDLC-->>Client: I-Frame: AARE (Membawa Server Challenge)

    Note over Client,Secure: 3. Pembuktian Bersama HLS 5 (Mutual Proof Exchange)
    Client->>HDLC: I-Frame: Action Request (reply_to_HLS, Client Proof CtoS)
    HDLC->>Port: Action Method 1 (Association LN)
    Port->>NSC: SECURE_DLMS_HLS5_VerifyAndGenerateProof()
    Note over Secure: Validasi GMAC CtoS (Constant-time)<br/>Jika valid, bangkitkan GMAC StoC
    alt Jika Otentikasi Gagal
        Secure-->>Port: Error Auth Failed
        Note over Port: Pemicu Anti-Brute-Force:<br/>failed_attempts++ (Penalti 5s..40s atau Lockout)
        Port-->>Client: Penolakan AARE Rejection Permanent
    else Jika Otentikasi Sukses
        Secure-->>Port: Success + Server Proof (StoC)
        Port-->>HDLC: Action Response (Server Proof)
        HDLC-->>Client: I-Frame: HLS 5 Sesi Terbuka Penuh!
    end

    Note over Client,Secure: 4. Transaksi Terenkripsi (Suite 0 AES-128-GCM)
    Client->>HDLC: GLO-GET-Request (0xC8, SC=0x30, Nonce, Ciphertext, Tag)
    HDLC->>Port: Teruskan ke Cipher Engine
    Port->>NSC: SECURE_DLMS_AES_GCM_Process(Decrypt)
    Secure->>Secure: Cek Monotonik RX Counter (Anti-Replay)
    Secure->>Secure: Dekripsi AES-GCM & Verifikasi Tag 16-Byte
    Secure-->>Port: Plaintext APDU GET
    Port->>Port: Baca Objek COSEM (misal Load Profile 23-Kanal)
    Port->>NSC: SECURE_DLMS_AES_GCM_Process(Encrypt Respons)
    Secure->>Secure: Alokasi TX Monotonic Counter + Enkripsi
    Secure-->>Port: GLO-GET-Response (0xCC)
    Port-->>HDLC: Bungkus I-Frame HDLC
    HDLC-->>Client: Respons Terenkripsi Sampai di HES
```

---

## 3. Diagram Struktur Pengujian & CI/CD Pipeline

```mermaid
flowchart LR
    subgraph DEV["💻 WORKSPACE DEVELOPMENT"]
        CODE["Source Code\n(NonSecure, Secure, Tests)"]
    end

    subgraph LOCAL_TEST["🧪 VERIFIKASI HOST LOKAL (Ninja / CMake)"]
        direction TB
        C1["AmiCosemTest"]
        C2["AmiSecurityTest"]
        C3["AmiDlmsHdlcTest"]
        C4["AmiDlmsApduTest"]
        C5["AmiAde9000Test"]
        C6["AmiAntiBruteforceTest"]
        C7["AmiBlockTransferTest"]
        C8["AmiDisconnectControlTest"]
        C9["(Total 14/14 Suites PASS)"]
        SIM["Gurux Simulator TCP (Port 4059)\nUji GXDLMSDirector E2E"]
    end

    subgraph DOCKER_BOX["🐳 SINGLE UNIFIED TEST CONTAINER (Alpine Linux)"]
        DK_BUILD["Stage 1: Multi-stage Build\n(GCC 15, CMake, Ninja, -O3 Release)"]
        DK_RUN["Stage 2: Runtime Runner\n1. Jalankan 14 CTest Suites (100%)\n2. Jalankan Background Simulator\n3. Jalankan Python E2E Client Verification"]
        DK_BUILD --> DK_RUN
    end

    subgraph CI_PIPELINE["🚀 GITHUB ACTIONS PRODUCTION CI"]
        direction TB
        W1["Host Tests CI (Ubuntu Linux & Windows)"]
        W2["STM32U585 FreeRTOS Firmware CI\n(ARM GNU GCC 13.3.Rel1 Toolchain)"]
        W3["CodeQL Static Analysis (C/C++ Security SAST)"]
        ARTIFACTS["📦 Firmware Artifacts\n(.elf, .bin, .hex, .map)"]
        W2 --> ARTIFACTS
    end

    CODE --> LOCAL_TEST
    CODE --> DOCKER_BOX
    CODE --> CI_PIPELINE
```


# harrynurisnandar1-oss/stm32u585vit6_sm architecture

> Architecture diagram of the GitHub repository harrynurisnandar1-oss/stm32u585vit6_sm, generated by GitDiagram from its file tree, README and a sample of source files (last updated 2026-11-05). It is an AI-made overview: check details against the source.

- Interactive diagram: https://gitdiagram.com/harrynurisnandar1-oss/stm32u585vit6_sm
- Repository: https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm

## Overview

This repository is firmware for a three-phase STM32U585/ADE9000 smart meter. Its main user workflow is a DLMS client sending HDLC frames: the non-secure communications stack handles framing and association, processes xDLMS requests against the COSEM meter model, and returns readings or controls the breaker. Security services run across the TrustZone boundary, while metrology supplies meter data. The README documents these product capabilities; sampled startup code confirms secure boot and non-secure scheduler startup, but most application-module call sites were not sampled, so those links rely on README evidence.

## Diagram (Mermaid)

```mermaid
flowchart TD

subgraph group_runtime["Meter runtime"]
  node_ns_entry["Non-secure startup<br/>[main.c]"]
  node_rtos["FreeRTOS tasks<br/>[app_freertos.c]"]
end

subgraph group_protocol["DLMS protocol"]
  node_port["DLMS port<br/>[ami_dlms_port.c]"]
  node_hdlc["HDLC framing<br/>[ami_dlms_hdlc.c]"]
  node_association["ACSE association"]
  node_apdu["xDLMS APDU<br/>[ami_dlms_apdu.c]"]
  node_block["Block transfer"]
end

subgraph group_domain["Meter capabilities"]
  node_cosem["COSEM model<br/>[ami_cosem_model.c]"]
  node_obis["OBIS catalog<br/>[ami_obis_catalog.h]"]
  node_ade["ADE9000 metrology<br/>[ami_ade9000.c]"]
  node_disconnect["Disconnect control"]
end

subgraph group_security["TrustZone security"]
  node_security["DLMS cryptography<br/>[ami_security.c]"]
  node_brute["Brute-force lockout"]
  node_counter["NVM counters<br/>[ami_nvm_counter.c]"]
  node_nsc["Secure gateway API<br/>[secure_nsc.h]"]
  node_secure_entry["Secure boot<br/>[main.c]"]
  node_secure_services["Secure services<br/>[secure_nsc.c]"]
end

subgraph group_platform["Hardware platform"]
  node_cmsis_device["STM32 device layer<br/>[stm32u585xx.h]"]
  node_cmsis_core["CMSIS core support<br/>[core_cm33.h]"]
  node_hal["STM32 HAL<br/>[stm32u5xx_hal.c]"]
end

node_hes(("DLMS client"))

node_hes -->|"exchanges frames"| node_port
node_port -->|"handles framing"| node_hdlc
node_hdlc -->|"opens association"| node_association
node_association -->|"negotiates requests"| node_apdu
node_apdu -->|"accesses objects"| node_cosem
node_apdu -->|"segments APDUs"| node_block
node_cosem -->|"uses identifiers"| node_obis
node_ade -->|"supplies readings"| node_cosem
node_ade -->|"triggers tamper log"| node_cosem
node_cosem -->|"exposes breaker control"| node_disconnect
node_association -->|"authenticates client"| node_security
node_security -->|"enforces lockout"| node_brute
node_security -->|"checks invocation counters"| node_counter
node_security -.->|"uses secure APIs"| node_nsc
node_nsc -->|"exposes services"| node_secure_services
node_secure_entry -->|"verifies image"| node_secure_services
node_secure_entry -->|"jumps after verification"| node_ns_entry
node_ns_entry -->|"starts scheduler"| node_rtos
node_ns_entry -.->|"initializes HAL"| node_hal
node_hal -->|"uses device definitions"| node_cmsis_device
node_cmsis_device -.->|"targets Cortex-M33"| node_cmsis_core

click node_ns_entry "https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm/blob/main/NonSecure/Core/Src/main.c"
click node_rtos "https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm/blob/main/NonSecure/Core/Src/app_freertos.c"
click node_port "https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm/blob/main/NonSecure/AMI/Src/ami_dlms_port.c"
click node_hdlc "https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm/blob/main/NonSecure/AMI/Src/ami_dlms_hdlc.c"
click node_association "https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm/blob/main/NonSecure/AMI/Src/ami_dlms_association.c"
click node_apdu "https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm/blob/main/NonSecure/AMI/Src/ami_dlms_apdu.c"
click node_block "https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm/blob/main/NonSecure/AMI/Src/ami_dlms_block_transfer.c"
click node_cosem "https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm/blob/main/NonSecure/AMI/Src/ami_cosem_model.c"
click node_obis "https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm/blob/main/NonSecure/AMI/Inc/ami_obis_catalog.h"
click node_ade "https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm/blob/main/NonSecure/AMI/Src/ami_ade9000.c"
click node_disconnect "https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm/blob/main/NonSecure/AMI/Src/ami_disconnect_control.c"
click node_security "https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm/blob/main/NonSecure/AMI/Src/ami_security.c"
click node_brute "https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm/blob/main/NonSecure/AMI/Src/ami_anti_bruteforce.c"
click node_counter "https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm/blob/main/NonSecure/AMI/Src/ami_nvm_counter.c"
click node_nsc "https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm/blob/main/Secure_nsclib/secure_nsc.h"
click node_secure_entry "https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm/blob/main/Secure/Core/Src/main.c"
click node_secure_services "https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm/blob/main/Secure/Core/Src/secure_nsc.c"
click node_cmsis_device "https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm/blob/main/Drivers/CMSIS/Device/ST/STM32U5xx/Include/stm32u585xx.h"
click node_cmsis_core "https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm/blob/main/Drivers/CMSIS/Include/core_cm33.h"
click node_hal "https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm/blob/main/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal.c"

classDef toneNeutral fill:#f8fafc,stroke:#334155,stroke-width:1.5px,color:#0f172a
classDef toneBlue fill:#dbeafe,stroke:#2563eb,stroke-width:1.5px,color:#172554
classDef toneAmber fill:#fef3c7,stroke:#d97706,stroke-width:1.5px,color:#78350f
classDef toneMint fill:#dcfce7,stroke:#16a34a,stroke-width:1.5px,color:#14532d
classDef toneRose fill:#ffe4e6,stroke:#e11d48,stroke-width:1.5px,color:#881337
classDef toneIndigo fill:#e0e7ff,stroke:#4f46e5,stroke-width:1.5px,color:#312e81
classDef toneTeal fill:#ccfbf1,stroke:#0f766e,stroke-width:1.5px,color:#134e4a
class node_ns_entry,node_rtos,node_hes toneBlue
class node_port,node_hdlc,node_association,node_apdu,node_block toneAmber
class node_cosem,node_obis,node_ade,node_disconnect toneMint
class node_security,node_brute,node_counter,node_nsc,node_secure_entry,node_secure_services toneRose
class node_cmsis_device,node_cmsis_core,node_hal toneIndigo
```

## Components

### Meter runtime

- [Non-secure startup](https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm/blob/main/NonSecure/Core/Src/main.c): `NonSecure/Core/Src/main.c`
- [FreeRTOS tasks](https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm/blob/main/NonSecure/Core/Src/app_freertos.c): `NonSecure/Core/Src/app_freertos.c`

### DLMS protocol

- [DLMS port](https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm/blob/main/NonSecure/AMI/Src/ami_dlms_port.c): `NonSecure/AMI/Src/ami_dlms_port.c`
- [HDLC framing](https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm/blob/main/NonSecure/AMI/Src/ami_dlms_hdlc.c): `NonSecure/AMI/Src/ami_dlms_hdlc.c`
- [ACSE association](https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm/blob/main/NonSecure/AMI/Src/ami_dlms_association.c): `NonSecure/AMI/Src/ami_dlms_association.c`
- [xDLMS APDU](https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm/blob/main/NonSecure/AMI/Src/ami_dlms_apdu.c): `NonSecure/AMI/Src/ami_dlms_apdu.c`
- [Block transfer](https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm/blob/main/NonSecure/AMI/Src/ami_dlms_block_transfer.c): `NonSecure/AMI/Src/ami_dlms_block_transfer.c`

### Meter capabilities

- [COSEM model](https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm/blob/main/NonSecure/AMI/Src/ami_cosem_model.c): `NonSecure/AMI/Src/ami_cosem_model.c`
- [OBIS catalog](https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm/blob/main/NonSecure/AMI/Inc/ami_obis_catalog.h): `NonSecure/AMI/Inc/ami_obis_catalog.h`
- [ADE9000 metrology](https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm/blob/main/NonSecure/AMI/Src/ami_ade9000.c): `NonSecure/AMI/Src/ami_ade9000.c`
- [Disconnect control](https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm/blob/main/NonSecure/AMI/Src/ami_disconnect_control.c): `NonSecure/AMI/Src/ami_disconnect_control.c`

### TrustZone security

- [DLMS cryptography](https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm/blob/main/NonSecure/AMI/Src/ami_security.c): `NonSecure/AMI/Src/ami_security.c`
- [Brute-force lockout](https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm/blob/main/NonSecure/AMI/Src/ami_anti_bruteforce.c): `NonSecure/AMI/Src/ami_anti_bruteforce.c`
- [NVM counters](https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm/blob/main/NonSecure/AMI/Src/ami_nvm_counter.c): `NonSecure/AMI/Src/ami_nvm_counter.c`
- [Secure gateway API](https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm/blob/main/Secure_nsclib/secure_nsc.h): `Secure_nsclib/secure_nsc.h`
- [Secure boot](https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm/blob/main/Secure/Core/Src/main.c): `Secure/Core/Src/main.c`
- [Secure services](https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm/blob/main/Secure/Core/Src/secure_nsc.c): `Secure/Core/Src/secure_nsc.c`

### Hardware platform

- [STM32 device layer](https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm/blob/main/Drivers/CMSIS/Device/ST/STM32U5xx/Include/stm32u585xx.h): `Drivers/CMSIS/Device/ST/STM32U5xx/Include/stm32u585xx.h`
- [CMSIS core support](https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm/blob/main/Drivers/CMSIS/Include/core_cm33.h): `Drivers/CMSIS/Include/core_cm33.h`
- [STM32 HAL](https://github.com/harrynurisnandar1-oss/stm32u585vit6_sm/blob/main/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal.c): `Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal.c`

### Other components

- **DLMS client**

## Connections

- DLMS client → DLMS port: exchanges frames (evidence: `README.md`)
- DLMS port → HDLC framing: handles framing (evidence: `README.md`)
- HDLC framing → ACSE association: opens association (evidence: `README.md`)
- ACSE association → xDLMS APDU: negotiates requests (evidence: `README.md`)
- xDLMS APDU → COSEM model: accesses objects (evidence: `README.md`)
- xDLMS APDU → Block transfer: segments APDUs (evidence: `README.md`)
- COSEM model → OBIS catalog: uses identifiers (evidence: `NonSecure/AMI/Src/ami_cosem_model.c`)
- ADE9000 metrology → COSEM model: supplies readings (evidence: `README.md`)
- ADE9000 metrology → COSEM model: triggers tamper log (evidence: `README.md`)
- COSEM model → Disconnect control: exposes breaker control (evidence: `README.md`)
- ACSE association → DLMS cryptography: authenticates client (evidence: `README.md`)
- DLMS cryptography → Brute-force lockout: enforces lockout (evidence: `README.md`)
- DLMS cryptography → NVM counters: checks invocation counters (evidence: `README.md`)
- DLMS cryptography → Secure gateway API: uses secure APIs
- Secure gateway API → Secure services: exposes services (evidence: `Secure/Core/Src/secure_nsc.c`)
- Secure boot → Secure services: verifies image (evidence: `Secure/Core/Src/main.c`)
- Secure boot → Non-secure startup: jumps after verification (evidence: `Secure/Core/Src/main.c`)
- Non-secure startup → FreeRTOS tasks: starts scheduler (evidence: `NonSecure/Core/Src/main.c`)
- Non-secure startup → STM32 HAL: initializes HAL
- STM32 HAL → STM32 device layer: uses device definitions
- STM32 device layer → CMSIS core support: targets Cortex-M33