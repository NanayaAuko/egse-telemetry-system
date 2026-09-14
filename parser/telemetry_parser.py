import socket, struct, threading, asyncio, json
import websockets

UDP_PORT = 9001
connected_clients = set()
loop = None

def calc_crc16(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if (crc & 1) else (crc >> 1)
    return crc & 0xFFFF

def udp_worker():
    global loop
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("0.0.0.0", UDP_PORT))
    print(f"[PARSER] UDP Listener running on port {UDP_PORT}", flush=True)

    while True:
        try:
            data, addr = sock.recvfrom(1024)
            # 正確總長度: 24 (Payload) + 2 (CRC) = 26 Bytes
            if len(data) == 26:
                payload = data[:24]
                received_crc = struct.unpack('<H', data[24:])[0]
                expected_crc = calc_crc16(payload)
                
                if expected_crc == received_crc:
                    header, pkt_id, ts, pitch, roll, yaw, volt = struct.unpack('<HHIffff', payload)
                    msg = json.dumps({
                        "id": pkt_id, "timestamp": ts, "pitch": round(pitch, 2),
                        "roll": round(roll, 2), "yaw": round(yaw, 2), "voltage": round(volt, 2)
                    })
                    if connected_clients and loop:
                        asyncio.run_coroutine_threadsafe(broadcast_msg(msg), loop)
                else:
                    print(f"[CRC MISMATCH] Expected {hex(expected_crc)}, got {hex(received_crc)}", flush=True)
            else:
                print(f"[PACKET LEN ERROR] Expected 26 bytes, got {len(data)}", flush=True)
        except Exception as e:
            print(f"[UDP ERROR] {e}", flush=True)

async def broadcast_msg(msg):
    if connected_clients:
        websockets.broadcast(connected_clients, msg)

async def ws_handler(websocket):
    connected_clients.add(websocket)
    print(f"[WS] Client connected! Active: {len(connected_clients)}", flush=True)
    try:
        await websocket.wait_closed()
    finally:
        connected_clients.remove(websocket)
        print(f"[WS] Client disconnected. Active: {len(connected_clients)}", flush=True)

async def main():
    global loop
    loop = asyncio.get_running_loop()
    threading.Thread(target=udp_worker, daemon=True).start()
    async with websockets.serve(ws_handler, "0.0.0.0", 8765):
        await asyncio.Future()

if __name__ == "__main__":
    asyncio.run(main())
