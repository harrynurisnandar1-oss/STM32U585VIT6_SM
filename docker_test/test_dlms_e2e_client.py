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
            else:
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

    # 5. Send DISC
    print("[5] Sending DISC (Disconnect)...")
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

