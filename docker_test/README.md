# GXDLMSDirector Connection Guide for Docker Simulator

The `stm32u585vit6sm` Docker container is a minimal multi-stage Alpine image (**< 10 MB**) dedicated to running the **Gurux DLMS/COSEM HDLC-over-TCP Simulator** on port `4059`.

Client testing tools such as **GXDLMSDirector** (or Python E2E scripts) run directly from your **Windows/Linux Host**, keeping the container lightweight.

---

## 1. Start the Simulator in Docker

From the repository root or this directory:
```powershell
.\docker_test\run_docker_tests.bat
```
Or via Docker Compose:
```bash
docker compose up -d
```
The simulator will start listening in the background on `127.0.0.1:4059`.

---

## 2. Configure GXDLMSDirector Connection (Host)

Open **GXDLMSDirector** on your host PC and create a new device with the following settings (or load `STM32U585VIT6_SM.gxc` from the repository root):

1. **General Settings**:
   - **Manufacturer**: `Generic` (or `Gurux`)
   - **Authentication**: `None` (for basic functional testing) or `High (GMAC)`
   - **Logical Name Referencing (LN)**: Checked (`True`)
   - **Interface Type**: `HDLC`
   - **Client Address**: `16` (Hex `0x10` — Management Client) or `1` (Public)
   - **Server Address**: `1` (Logical Address 1)

2. **Communication (Media)**:
   - **Media**: `Network` (TCP/IP)
   - **Host Name / IP Address**: `127.0.0.1` (or `localhost`)
   - **Port**: `4059`

3. **Connect**:
   - Click **Connect**.
   - Once the status changes to *Connected*, right-click the device and select **Read Objects / Association View**.
   - All **66 active OBIS objects**, the 23-channel Load Profile, and Tamper Logs can be read interactively.

---

## 3. Run the Python E2E Client from Host (Optional)

To execute the automated Python E2E test script from your host:
```powershell
python .\docker_test\test_dlms_e2e_client.py
```

---

## 4. Stop the Simulator

To stop and remove the simulator container:
```bash
docker compose down
```
