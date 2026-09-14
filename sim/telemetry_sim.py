import socket, struct, time, math

UDP_IP = "127.0.0.1"
UDP_PORT = 9001
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

packet_id = 0
start_time = time.time()
print(f"[SIM] Starting 50Hz telemetry targeting {UDP_IP}:{UDP_PORT}...", flush=True)

while True:
    t = time.time() - start_time
    pitch = 15.0 * math.sin(t * 1.5)
    roll = 25.0 * math.cos(t * 2.0)
    yaw = (t * 10.0) % 360.0
    voltage = 28.0 + 0.5 * math.sin(t)
    timestamp_ms = int(t * 1000) & 0xFFFFFFFF

    payload = struct.pack('<HHIffff', 0xAA55, packet_id & 0xFFFF, timestamp_ms, pitch, roll, yaw, voltage)
    crc = 0xFFFF
    for b in payload:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if (crc & 1) else (crc >> 1)
    
    packet = payload + struct.pack('<H', crc & 0xFFFF)
    sock.sendto(packet, (UDP_IP, UDP_PORT))
    packet_id += 1
    time.sleep(0.02)
