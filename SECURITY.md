# Security Policy

## Supported Versions

Use this section to review which versions of the STM32U585VIT6 Smart Meter firmware project are currently supported with security updates.

| Version      | Supported          | Notes |
| ------------ | ------------------ | ----- |
| 2.20.x       | :white_check_mark: | Active release branch (Hardware Spin 2 baseline, 4-pillar security verification) |
| 2.19.x       | :white_check_mark: | Critical security patches and bugfixes only |
| 2.18.x       | :white_check_mark: | Critical security patches and bugfixes only |
| 2.x (< 2.18) | :x:                | End of Life (EOL) - Discontinued support |
| < 2.0.0      | :x:                | Initial prototypes (Hardware Spin 1 development) |

## Reporting a Vulnerability

We take the security of our embedded Smart Meter firmware, cryptographic implementations (SPLN D5.002:2024 / NIST SP 800-38D), and ARM Cortex-M33 TrustZone-M hardware isolation very seriously.

If you believe you have discovered a security vulnerability or security bug, please follow the guidelines below to report it responsibly.

### 1. How to Report

To protect our users and metering infrastructure, **please do not disclose security vulnerabilities publicly** (such as through public GitHub Issues, pull requests, or public discussions).

Instead, report the issue through one of the following secure channels:

- **GitHub Private Vulnerability Reporting (Recommended)**:  
  Navigate to the **Security** tab of this repository, select **Advisories**, and click **"Report a vulnerability"** to open a confidential security advisory draft.
- **Direct Email**:  
  Send an encrypted or confidential email to `harrynurisnandar1@gmail.com` with the subject tag `[SECURITY VULNERABILITY] STM32U585VIT6_SM - <Brief Summary>`.

### 2. What to Include

To help us triage and resolve the issue quickly, please include:
- The firmware version(s) or git commit hash affected.
- The specific subsystem or component involved (e.g., TrustZone NSC gateway, AES-128-GCM ciphering, HDLC frame parser, ACSE state machine, or ADE9000 SPI driver).
- Step-by-step reproduction instructions or proof-of-concept (PoC) communication frames / APDU payloads.
- Potential impact assessment (e.g., Denial of Service, Replay Attack, Key Disclosure, Authentication Bypass, or Buffer Overflow).

### 3. Response SLA and Disclosure Process

- **Initial Acknowledgment**: You will receive an acknowledgment of your report within **48 hours**.
- **Triage & Assessment**: Within **5 business days**, our team will assess the report, verify the vulnerability, and determine its severity (CVSS score).
- **Remediation & Patching**: Confirmed security vulnerabilities will be patched in a private branch, verified against the comprehensive 16/16 CTest suite, LCOV code coverage analysis, and static analysis, and released in an updated version.
- **Public Disclosure**: A public GitHub Security Advisory will be published upon release of the fix, crediting the reporter (unless you prefer to remain anonymous).
