#!/usr/bin/env python3
"""
Automated End-to-End DLMS Client Test
Simulates an external client (like Gurux DLMSDirector) connecting to HDLC-over-TCP port 4059.
"""

import socket
import sys
import time

def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0x8408
            else:
                crc = crc >> 1
    return (~crc) & 0xFFFF

def build_hdlc_frame(dst_sap: int, src_sap: int, control: int, payload: bytes = b"") -> bytes:
    # 1-byte SAP address encoding (shifted by 1, bit 0 = 1)
    dst_encoded = bytes([(dst_sap << 1) | 1])
    src_encoded = bytes([(src_sap << 1) | 1])
    addr_field = dst_encoded + src_encoded

    total_len = len(addr_field) + 1 + (len(payload) + 2 if payload else 0) + 2 + 2
    hdr = bytes([0xA0 | ((total_len >> 8) & 0x07), total_len & 0xFF]) + addr_field + bytes([control])
    
    if payload:
        hcs = crc16_ccitt(hdr)
        hcs_bytes = bytes([hcs & 0xFF, (hcs >> 8) & 0xFF])
        body = hdr + hcs_bytes + payload
    else:
        body = hdr

    fcs = crc16_ccitt(body)
    fcs_bytes = bytes([fcs & 0xFF, (fcs >> 8) & 0xFF])
    return b"\x7E" + body + fcs_bytes + b"\x7E"

def read_hdlc_frame(sock: socket.socket, timeout: float = 3.0) -> bytes:
    sock.settimeout(timeout)
    buf = bytearray()
    started = False
    start_time = time.time()

    while time.time() - start_time < timeout:
        chunk = sock.recv(1)
        if not chunk:
            break
        b = chunk[0]
        if b == 0x7E:
            if not started:
                started = True
                buf = bytearray([0x7E])
            elif len(buf) > 1:
                buf.append(0x7E)
                return bytes(buf)
        elif started:
            buf.append(b)
    return bytes(buf)

def main():
    host = "127.0.0.1"
    port = 4059
    print(f"[*] Connecting to DLMS TCP Simulator at {host}:{port}...")

    # Retry connection for up to 5 seconds
    sock = None
    for attempt in range(10):
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.connect((host, port))
            break
        except ConnectionRefusedError:
            time.sleep(0.5)

    if not sock:
        print("[-] Error: Could not connect to simulator.")
        sys.exit(1)

    print("[+] Connected successfully.")

    # 1. Send SNRM
    print("[1] Sending SNRM connection request...")
    snrm_frame = build_hdlc_frame(dst_sap=1, src_sap=16, control=0x93)
    sock.sendall(snrm_frame)
    ua_resp = read_hdlc_frame(sock)
    assert len(ua_resp) > 0, "No UA response received"
    # Control byte in UA response should be 0x73 (or 0x63)
    assert 0x73 in ua_resp or 0x63 in ua_resp, f"Invalid UA response: {ua_resp.hex()}"
    print(f"    [PASS] UA received ({len(ua_resp)} bytes): {ua_resp.hex()}")

    # 2. Send AARQ (No-Auth)
    print("[2] Sending AARQ (Initiate DLMS Association)...")
    aarq_apdu = (
        b"\xE6\xE6\x00"  # LLC
        b"\x60\x1D"      # AARQ length 29
        b"\xA1\x09\x06\x07\x60\x85\x74\x05\x08\x01\x01"  # App Context LN
        b"\xBE\x10\x04\x0E\x01\x00\x00\x00\x06\x5F\x1F\x04\x00\x00\x00\x14\x01\xF4"  # InitiateRequest
    )
    aarq_frame = build_hdlc_frame(dst_sap=1, src_sap=16, control=0x10, payload=aarq_apdu)
    sock.sendall(aarq_frame)
    aare_resp = read_hdlc_frame(sock)
    assert len(aare_resp) > 0, "No AARE response received"
    assert b"\x61" in aare_resp, f"AARE tag 0x61 missing: {aare_resp.hex()}"
    print(f"    [PASS] AARE received ({len(aare_resp)} bytes), Association ESTABLISHED.")

    # 3. Send GET-Request-Normal for Voltage Phase R (1.0.32.7.0.255)
    print("[3] Reading Voltage Phase R (OBIS 1.0.32.7.0.255, Class 3, Attr 2)...")
    get_v_apdu = (
        b"\xE6\xE6\x00"  # LLC
        b"\xC0\x01\xC1"  # GET-Request-Normal, invoke ID 0xC1
        b"\x00\x03"      # Class 3
        b"\x01\x00\x20\x07\x00\xFF"  # 1.0.32.7.0.255
        b"\x02"          # Attribute 2
        b"\x00"          # No selective access
    )
    get_v_frame = build_hdlc_frame(dst_sap=1, src_sap=16, control=0x12, payload=get_v_apdu)
    sock.sendall(get_v_frame)
    get_v_resp = read_hdlc_frame(sock)
    assert len(get_v_resp) > 0, "No GET response received for Voltage"
    assert b"\xC4\x01\xC1\x00" in get_v_resp, f"Invalid GET response: {get_v_resp.hex()}"
    print(f"    [PASS] Voltage R Response received: {get_v_resp.hex()}")

    # 4. Send GET-Request-Normal for Clock (0.0.1.0.0.255)
    print("[4] Reading Meter Clock (OBIS 0.0.1.0.0.255, Class 8, Attr 2)...")
    get_clk_apdu = (
        b"\xE6\xE6\x00"
        b"\xC0\x01\xC2"
        b"\x00\x08"      # Class 8
        b"\x00\x00\x01\x00\x00\xFF"  # 0.0.1.0.0.255
        b"\x02"
        b"\x00"
    )
    get_clk_frame = build_hdlc_frame(dst_sap=1, src_sap=16, control=0x14, payload=get_clk_apdu)
    sock.sendall(get_clk_frame)
    get_clk_resp = read_hdlc_frame(sock)
    assert len(get_clk_resp) > 0, "No GET response received for Clock"
    assert b"\xC4\x01\xC2\x00" in get_clk_resp, f"Invalid GET response: {get_clk_resp.hex()}"
    print(f"    [PASS] Clock Response received: {get_clk_resp.hex()}")

    # 5. Send GET-Request-Normal for Instant Profile Capture Objects (1.96.98.128.0.255 Attr 3)
    print("[5] Reading Instant Profile Capture Objects (OBIS 1.96.98.128.0.255, Class 7, Attr 3)...")
    get_inst_cap = (
        b"\xE6\xE6\x00"
        b"\xC0\x01\xC3"
        b"\x00\x07"
        b"\x01\x60\x62\x80\x00\xFF"  # 1.96.98.128.0.255
        b"\x03"                      # Attribute 3
        b"\x00"
    )
    sock.sendall(build_hdlc_frame(dst_sap=1, src_sap=16, control=0x16, payload=get_inst_cap))
    inst_cap_resp = read_hdlc_frame(sock)
    assert len(inst_cap_resp) > 0, "No response for Instant Profile Capture Objects"
    assert b"\xC4\x01\xC3\x00" in inst_cap_resp, f"Failed reading Instant Profile Capture Objects: {inst_cap_resp.hex()}"
    print(f"    [PASS] Instant Profile Capture Objects received ({len(inst_cap_resp)} bytes)")

    # 6. Send GET-Request-Normal for Instant Profile Buffer (1.96.98.128.0.255 Attr 2)
    print("[6] Reading Instant Profile Buffer (OBIS 1.96.98.128.0.255, Class 7, Attr 2)...")
    get_inst_buf = (
        b"\xE6\xE6\x00"
        b"\xC0\x01\xC4"
        b"\x00\x07"
        b"\x01\x60\x62\x80\x00\xFF"  # 1.96.98.128.0.255
        b"\x02"                      # Attribute 2
        b"\x00"                      # All entries
    )
    sock.sendall(build_hdlc_frame(dst_sap=1, src_sap=16, control=0x18, payload=get_inst_buf))
    inst_buf_resp = read_hdlc_frame(sock)
    assert len(inst_buf_resp) > 0, "No response for Instant Profile Buffer"
    assert b"\xC4\x01\xC4\x00" in inst_buf_resp, f"Failed reading Instant Profile Buffer: {inst_buf_resp.hex()}"
    print(f"    [PASS] Instant Profile Buffer received ({len(inst_buf_resp)} bytes)")

    # 7. Read Event Log #1 Buffer (OBIS 0.0.99.98.0.255, Class 7, Attr 2) - should have 2 columns
    print("[7] Reading Event Log #1 Buffer (OBIS 0.0.99.98.0.255, Class 7, Attr 2)...")
    get_evt_buf = (
        b"\xE6\xE6\x00"
        b"\xC0\x01\xC5"
        b"\x00\x07"
        b"\x00\x00\x63\x62\x00\xFF"  # 0.0.99.98.0.255
        b"\x02"                      # Attribute 2
        b"\x00"
    )
    sock.sendall(build_hdlc_frame(dst_sap=1, src_sap=16, control=0x1A, payload=get_evt_buf))
    evt_buf_resp = read_hdlc_frame(sock)
    assert len(evt_buf_resp) > 0, "No response for Event Log #1 Buffer"
    assert b"\xC4\x01\xC5\x00" in evt_buf_resp, f"Failed reading Event Log #1 Buffer: {evt_buf_resp.hex()}"
    # Structure of 2 elements should be present: 0x02, 0x02 (Type Structure=0x02, length=2)
    assert b"\x02\x02" in evt_buf_resp, f"Event log #1 row does not have 2 columns: {evt_buf_resp.hex()}"
    print(f"    [PASS] Event Log #1 Buffer received with 2 columns ({len(evt_buf_resp)} bytes)")

    # 8. Read PQ Voltage Log Profile R Capture Objects (OBIS 1.0.99.14.2.255 Attr 3)
    print("[8] Reading PQ Voltage Log Profile R Capture Objects (OBIS 1.0.99.14.2.255, Class 7, Attr 3)...")
    get_pq_cap = (
        b"\xE6\xE6\x00"
        b"\xC0\x01\xC6"
        b"\x00\x07"
        b"\x01\x00\x63\x0E\x02\xFF"  # 1.0.99.14.2.255
        b"\x03"                      # Attribute 3
        b"\x00"
    )
    sock.sendall(build_hdlc_frame(dst_sap=1, src_sap=16, control=0x1C, payload=get_pq_cap))
    pq_cap_resp = read_hdlc_frame(sock)
    assert len(pq_cap_resp) > 0, "No response for PQ Voltage Log Profile R Capture Objects"
    assert b"\xC4\x01\xC6\x00" in pq_cap_resp, f"Failed reading PQ Voltage Capture Objects: {pq_cap_resp.hex()}"
    print(f"    [PASS] PQ Voltage Capture Objects received ({len(pq_cap_resp)} bytes)")

    # 9. Read PQ Voltage Log Profile R Buffer (OBIS 1.0.99.14.2.255 Attr 2)
    print("[9] Reading PQ Voltage Log Profile R Buffer (OBIS 1.0.99.14.2.255, Class 7, Attr 2)...")
    get_pq_buf = (
        b"\xE6\xE6\x00"
        b"\xC0\x01\xC7"
        b"\x00\x07"
        b"\x01\x00\x63\x0E\x02\xFF"  # 1.0.99.14.2.255
        b"\x02"                      # Attribute 2
        b"\x00"
    )
    sock.sendall(build_hdlc_frame(dst_sap=1, src_sap=16, control=0x1E, payload=get_pq_buf))
    pq_buf_resp = read_hdlc_frame(sock)
    assert len(pq_buf_resp) > 0, "No response for PQ Voltage Log Profile R Buffer"
    assert b"\xC4\x01\xC7\x00" in pq_buf_resp, f"Failed reading PQ Voltage Buffer: {pq_buf_resp.hex()}"
    print(f"    [PASS] PQ Voltage Buffer received ({len(pq_buf_resp)} bytes)")

    # 10. Read Activity Calendar Name Active (OBIS 0.0.13.0.0.255, Class 20, Attr 2) - Previously caused Invalid PDU
    print("[10] Reading Activity Calendar Name Active (OBIS 0.0.13.0.0.255, Class 20, Attr 2)...")
    get_cal_apdu = (
        b"\xE6\xE6\x00"
        b"\xC0\x01\xC8"
        b"\x00\x14"                  # Class 20
        b"\x00\x00\x0D\x00\x00\xFF"  # 0.0.13.0.0.255
        b"\x02"                      # Attribute 2
        b"\x00"
    )
    sock.sendall(build_hdlc_frame(dst_sap=1, src_sap=16, control=0x30, payload=get_cal_apdu))
    cal_resp = read_hdlc_frame(sock)
    assert len(cal_resp) > 0, "No response for Activity Calendar Attr 2"
    assert b"\xC4\x01\xC8\x00" in cal_resp, f"Failed reading Activity Calendar Name: {cal_resp.hex()}"
    assert b"SEASON_ACTIVE" in cal_resp, f"Calendar name SEASON_ACTIVE not found: {cal_resp.hex()}"
    print(f"    [PASS] Activity Calendar Name 'SEASON_ACTIVE' received ({len(cal_resp)} bytes)")

    # 11. Read Billing Scheduler Script (OBIS 0.0.15.0.2.255, Class 22, Attr 2)
    print("[11] Reading Billing Scheduler (OBIS 0.0.15.0.2.255, Class 22, Attr 2)...")
    get_sched_apdu = (
        b"\xE6\xE6\x00"
        b"\xC0\x01\xC9"
        b"\x00\x16"                  # Class 22
        b"\x00\x00\x0F\x00\x02\xFF"  # 0.0.15.0.2.255
        b"\x02"                      # Attribute 2
        b"\x00"
    )
    sock.sendall(build_hdlc_frame(dst_sap=1, src_sap=16, control=0x32, payload=get_sched_apdu))
    sched_resp = read_hdlc_frame(sock)
    assert len(sched_resp) > 0, "No response for Billing Scheduler Attr 2"
    assert b"\xC4\x01\xC9\x00" in sched_resp, f"Failed reading Billing Scheduler: {sched_resp.hex()}"
    print(f"    [PASS] Billing Scheduler received ({len(sched_resp)} bytes)")

    # 12. Send DISC
    print("[12] Sending DISC (Disconnect)...")
    disc_frame = build_hdlc_frame(dst_sap=1, src_sap=16, control=0x53)
    sock.sendall(disc_frame)
    disc_resp = read_hdlc_frame(sock)
    assert len(disc_resp) > 0, "No UA response received for DISC"
    print(f"    [PASS] Disconnected cleanly with UA response: {disc_resp.hex()}")

    sock.close()
    print("\n=============================================")
    print(">>> ALL END-TO-END DLMS TESTS PASSED (100%) <<<")
    print("=============================================\n")

if __name__ == "__main__":
    main()

