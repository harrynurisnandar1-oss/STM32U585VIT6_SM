[![Architecture diagram of harrynurisnandar1-oss/stm32u585vit6_sm](https://gitdiagram.com/harrynurisnandar1-oss/stm32u585vit6_sm/diagram.png)](https://gitdiagram.com/harrynurisnandar1-oss/stm32u585vit6_sm?utm_source=readme&utm_medium=picture)


# 📐 Complete System Architecture — 3-Phase AMI Smart Meter
**Target: STM32U585VIT6 (ARM Cortex-M33 TrustZone) + ADE9000 AFE**  
*Standards Compliance: SPLN D3.006:2021, SPLN D5.002:2024, IEC 62056 (DLMS/COSEM)*

---

## 1. End-to-End System Block Diagram (Physical & Logical Topology)

```mermaid
flowchart TB
    subgraph OUTSIDE["🌐 EXTERNAL / FIELD INTERFACES"]
        HES["🏢 PLN Head-End System (HES) / MDMS"]
        HHU["📱 Handheld Unit (HHU) / P2TL Technician"]
        GRID["⚡ 3-Phase Mains Grid 230/400V AC"]
    end

    subgraph INTERFACE["🔌 PHYSICAL INTERFACES & SENSORS (SPIN 1 / SPIN 2)"]
        MODEM["📡 Modular Modem Bay\n(4G Cat-M1 / NB-IoT / RF Mesh)\nRJ-45 8-Pin RS-232 (19,200 bps)"]
        OPTICAL["🔦 Front Infrared Optical Port\nIEC 62056-21 (9,600 bps)\n4 kV Galvanic Isolation"]
        AFE["⚡ ADE9000 Analog Front-End\n7-Channel 24-bit ADC, Isolated SPI\nCT + Voltage Divider Sensing"]
        BREAKER["🔘 Contactor / Relay Breaker\nDisconnect Control (Class 70)"]
    end

    HES <-->|Cellular / Wireless| MODEM
    HHU <-->|900nm Infrared| OPTICAL
    GRID -->|Voltage & Current| AFE
    BREAKER -->|Disconnect / Reconnect Load| GRID

    subgraph MCU["🔲 STM32U585VIT6 MICROCONTROLLER (ARM Cortex-M33 TrustZone)"]
        
        subgraph NONSECURE["🟢 NON-SECURE WORLD (Meter Application & Protocol)"]
            subgraph DRIVERS["Hardware / BSP Drivers"]
                UART_DRV["UART & DMA Circular Buffer Driver\n(ami_bsp_uart.c)\nLPUART1 / USART2 & USART3"]
                ADE_DRV["ADE9000 SPI Metrology Driver\n(ami_ade9000.c)\nFail-Closed Snapshot Feeder"]
                RELAY_DRV["Disconnect Control Actuator Driver\n(ami_disconnect_control.c)"]
            end

            subgraph RTOS["FreeRTOS Kernel Tasks"]
                DLMS_TASK["DLMS/COSEM Protocol Task\n(ami_dlms_port.c)\nStreamBuffer & Message Queue"]
                MET_TASK["Metrology & Sampling Task\nPeriodic RMS, Power, Energy Readout"]
            end

            subgraph DLMS_STACK["DLMS/COSEM Protocol Engine"]
                HDLC["HDLC Type 3 Parser (0xA0)\nCRC-16 HCS/FCS, Transparent 0x7E\nZero-Heap O(1)"]
                ACSE["ACSE Association Machine\nAARQ/AARE, Max PDU, Dynamic Conformance"]
                ABF["Anti-Brute-Force & Lockout\nExponential Penalty (5s..40s)\nHard Lockout 5 Failures (SPLN D5.002)"]
                GBT["General Block Transfer (GBT)\nxDLMS Datablock (0xC0/0xC4 0x02)\nLarge Profile Segmentation"]
                COSEM["COSEM Object Model (66 PLN Objects)\n- 15-min Load Profile (23 Channels)\n- Tamper Logbook (9 PLN Modes)\n- Billing Profile (Class 7)\n- Multi-Tariff WBP/LWBP (Tariffs 1..4)\n- Demand Limiter (Class 71)"]
            end

            UART_DRV -->|Byte Stream IRQ| RTOS
            ADE_DRV -->|Snapshot Data| RTOS
            RTOS --> HDLC --> ACSE --> DLMS_STACK
            DLMS_STACK --> RELAY_DRV
        end

        subgraph GATEWAY["🔐 NON-SECURE CALLABLE (NSC) GATEWAY"]
            NSC_API["secure_nsc.c / secure_nsc.h\n- cmse_check_address_range()\n- Copy-then-Validate Anti-TOCTOU\n- Strict Pointer Validation"]
        end

        subgraph SECURE["🔴 SECURE WORLD (TrustZone Protected Domain)"]
            SAU["Security Attribution Unit (SAU)\nSRAM3 & Flash Memory Isolation"]
            CRYPTO["Suite 0 Cryptography Core\n- AES-128-GCM (glo-get/set/action/init)\n- HLS 5 GMAC (Mutual 4-Pass Auth)\n- RFC 3394 Key Unwrap (Master Key KEK)\n- Memory Zeroization (secure_zeroize)"]
            TRNG["Fail-Closed Hardware TRNG\n16-Byte Random Challenge Generator"]
            NVM_SEC["Wear-Leveling Anti-Replay Engine\n- Strictly Monotonic Invocation Counter\n- TAMP Backup Register / NVM\n- Saturation Guard (0xFFFFFFFF Fail-Closed)"]
        end

        DLMS_STACK <-->|Crypto / Auth / Key Requests| NSC_API
        NSC_API <-->|Isolated Execution| CRYPTO
        NSC_API <-->|Random Challenge| TRNG
        NSC_API <-->|Monotonic Counter Update| NVM_SEC
    end

    MODEM <--> UART_DRV
    OPTICAL <--> UART_DRV
    AFE <--> ADE_DRV
    RELAY_DRV <--> BREAKER
```

---

## 2. Protocol & HLS5 (GMAC) Authentication Data Flow

```mermaid
sequenceDiagram
    autonumber
    participant Client as 🏢 HES / DLMS Client (SAP 16)
    participant HDLC as 🟢 HDLC Layer (NonSecure)
    participant Port as 🟢 DLMS Port / ACSE
    participant NSC as 🔐 TrustZone NSC Gateway
    participant Secure as 🔴 Secure World (Crypto Core)

    Note over Client,HDLC: 1. HDLC Link Layer Handshake
    Client->>HDLC: SNRM Frame (Format 3, Client=16, Server=1)
    HDLC->>Client: UA Frame (Max Frame Size 512B Negotiation)

    Note over Client,Port: 2. ACSE Association Handshake
    Client->>HDLC: I-Frame: AARQ (Mechanism OID: HLS 5 GMAC)
    HDLC->>Port: Extract AARQ APDU
    Port->>NSC: SECURE_TRNG_GetBytes(16 bytes)
    NSC->>Secure: Read STM32U5 Hardware TRNG
    Secure-->>Port: 16-Byte Server Challenge (StoC)
    Port-->>HDLC: Build AARE APDU (Status: Auth_Pending, StoC)
    HDLC-->>Client: I-Frame: AARE (Carrying Server Challenge)

    Note over Client,Secure: 3. HLS 5 Mutual Proof Exchange
    Client->>HDLC: I-Frame: Action Request (reply_to_HLS, Client Proof CtoS)
    HDLC->>Port: Action Method 1 (Association LN)
    Port->>NSC: SECURE_DLMS_HLS5_VerifyAndGenerateProof()
    Note over Secure: Validate CtoS GMAC (Constant-time)<br/>If valid, generate StoC GMAC
    alt Authentication Fails
        Secure-->>Port: Error Auth Failed
        Note over Port: Trigger Anti-Brute-Force:<br/>failed_attempts++ (Penalty 5s..40s or Lockout)
        Port-->>Client: Permanent AARE Rejection
    else Authentication Succeeds
        Secure-->>Port: Success + Server Proof (StoC)
        Port-->>HDLC: Action Response (Server Proof)
        HDLC-->>Client: I-Frame: HLS 5 Session Fully Open
    end

    Note over Client,Secure: 4. Encrypted Transaction (Suite 0 AES-128-GCM)
    Client->>HDLC: GLO-GET-Request (0xC8, SC=0x30, Nonce, Ciphertext, Tag)
    HDLC->>Port: Forward to Cipher Engine
    Port->>NSC: SECURE_DLMS_AES_GCM_Process(Decrypt)
    Secure->>Secure: Check Monotonic RX Counter (Anti-Replay)
    Secure->>Secure: Decrypt AES-GCM & Verify 16-Byte Tag
    Secure-->>Port: Plaintext GET APDU
    Port->>Port: Read COSEM Object (e.g. 23-Ch Load Profile)
    Port->>NSC: SECURE_DLMS_AES_GCM_Process(Encrypt Response)
    Secure->>Secure: Allocate Monotonic TX Counter + Encrypt
    Secure-->>Port: GLO-GET-Response (0xCC)
    Port-->>HDLC: Wrap in HDLC I-Frame
    HDLC-->>Client: Encrypted Response Delivered to HES
```

---

## 3. Testing & CI/CD Pipeline Structure

```mermaid
flowchart LR
    subgraph DEV["💻 DEVELOPMENT WORKSPACE"]
        CODE["Source Code\n(NonSecure, Secure, Tests)"]
    end

    subgraph LOCAL_TEST["🧪 LOCAL HOST VERIFICATION (Ninja / CMake)"]
        direction TB
        C1["AmiCosemTest"]
        C2["AmiSecurityTest"]
        C3["AmiDlmsHdlcTest"]
        C4["AmiDlmsApduTest"]
        C5["AmiAde9000Test"]
        C6["AmiAntiBruteforceTest"]
        C7["AmiBlockTransferTest"]
        C8["AmiDisconnectControlTest"]
        C9["(Total 16/16 Suites PASS)"]
        SIM["Gurux TCP Simulator (Port 4059)\nGXDLMSDirector E2E Testing"]
    end

    subgraph DOCKER_BOX["🐳 SINGLE UNIFIED TEST CONTAINER (Alpine Linux)"]
        DK_BUILD["Stage 1: Multi-stage Build\n(GCC, CMake, Ninja, -O3 Release, 16 CTest Suites)"]
        DK_RUN["Stage 2: Runtime Runner\nStripped gurux_simulator (Port 4059)\n+ Host Python E2E Client Verification"]
        DK_BUILD --> DK_RUN
    end

    subgraph CI_PIPELINE["🚀 GITHUB ACTIONS PRODUCTION CI"]
        direction TB
        W1["Host Tests CI (Ubuntu Linux & Windows)"]
        W2["STM32U585 FreeRTOS Firmware CI\n(ARM GNU GCC Toolchain)"]
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