#include <iostream>
#include <vector>
#include <mutex>
#include <set>
#include <cstring>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <thread>
#include <memory>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ixwebsocket/IXWebSocketServer.h>

#define UDP_PORT 9001
#define WS_PORT 8765

#pragma pack(push, 1)
struct TelemetryPacket {
    uint16_t header;       // 0xAA55 (2 Bytes)
    uint16_t packet_id;    // 封包編號 (2 Bytes)
    uint32_t timestamp_ms; // 時間戳記 (4 Bytes)
    float pitch;           // 俯仰角 (4 Bytes)
    float roll;            // 滾轉角 (4 Bytes)
    float yaw;             // 偏航角 (4 Bytes)
    float voltage;         // 電池電壓 (4 Bytes)
    uint16_t crc16;        // CRC-16 校驗碼 (2 Bytes)
};
#pragma pack(pop)

static_assert(sizeof(TelemetryPacket) == 26, "Packet size must be exactly 26 bytes!");

uint16_t calculate_crc16(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc = crc >> 1;
            }
        }
    }
    return crc;
}

std::mutex clients_mutex;
std::set<std::shared_ptr<ix::WebSocket>> connected_clients;

void broadcast_telemetry(const std::string& msg) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (auto it = connected_clients.begin(); it != connected_clients.end(); ) {
        if ((*it)->getReadyState() == ix::ReadyState::Open) {
            (*it)->sendText(msg);
            ++it;
        } else {
            it = connected_clients.erase(it);
        }
    }
}

void udp_worker() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "[C++ PARSER ERROR] Failed to create socket!" << std::endl;
        return;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(UDP_PORT);

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "[C++ PARSER ERROR] Failed to bind port " << UDP_PORT << "!" << std::endl;
        close(sockfd);
        return;
    }

    std::cout << "[C++ PARSER] UDP Listener bound on 0.0.0.0:" << UDP_PORT << std::endl;

    uint8_t buffer[1024];
    while (true) {
        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);
        ssize_t bytes_received = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr, &addr_len);

        if (bytes_received == sizeof(TelemetryPacket)) {
            const TelemetryPacket* pkt = reinterpret_cast<const TelemetryPacket*>(buffer);
            
            uint16_t expected_crc = calculate_crc16(buffer, 24);
            if (expected_crc == pkt->crc16 && pkt->header == 0xAA55) {
                std::ostringstream ss;
                ss << std::fixed << std::setprecision(2);
                ss << "{"
                   << "\"id\":" << pkt->packet_id << ","
                   << "\"timestamp\":" << pkt->timestamp_ms << ","
                   << "\"pitch\":" << pkt->pitch << ","
                   << "\"roll\":" << pkt->roll << ","
                   << "\"yaw\":" << pkt->yaw << ","
                   << "\"voltage\":" << pkt->voltage
                   << "}";

                broadcast_telemetry(ss.str());

                if (pkt->packet_id % 50 == 0) {
                    std::cout << "[C++ PARSER] Decoded Pkt #" << pkt->packet_id
                              << " [Pitch: " << pkt->pitch << " Roll: " << pkt->roll << "]" << std::endl;
                }
            } else {
                std::cerr << "[C++ PARSER] CRC Mismatch! Expected: " << expected_crc << " Got: " << pkt->crc16 << std::endl;
            }
        } else if (bytes_received > 0) {
            std::cerr << "[C++ PARSER] Invalid packet length: " << bytes_received << " bytes." << std::endl;
        }
    }
    close(sockfd);
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Starting High-Performance C++ Parser  " << std::endl;
    std::cout << "========================================" << std::endl;

    ix::WebSocketServer server(WS_PORT, "0.0.0.0");
    
    // 符合最新版 IXWebSocket 回呼簽章：weak_ptr 與 shared_ptr<ConnectionState>
    server.setOnConnectionCallback([](std::weak_ptr<ix::WebSocket> wsWeak, std::shared_ptr<ix::ConnectionState> connectionState) {
        auto webSocket = wsWeak.lock();
        if (!webSocket) return;

        webSocket->setOnMessageCallback([wsWeak](const ix::WebSocketMessagePtr& msg) {
            auto ws = wsWeak.lock();
            if (!ws) return;

            if (msg->type == ix::WebSocketMessageType::Open) {
                std::lock_guard<std::mutex> lock(clients_mutex);
                connected_clients.insert(ws);
                std::cout << "[WS] Client connected! Total: " << connected_clients.size() << std::endl;
            } else if (msg->type == ix::WebSocketMessageType::Close) {
                std::lock_guard<std::mutex> lock(clients_mutex);
                connected_clients.erase(ws);
                std::cout << "[WS] Client disconnected. Total: " << connected_clients.size() << std::endl;
            }
        });
    });

    auto res = server.listen();
    if (!res.first) {
        std::cerr << "[WS ERROR] " << res.second << std::endl;
        return 1;
    }
    server.start();
    std::cout << "[C++ PARSER] WebSocket Server listening on 0.0.0.0:" << WS_PORT << std::endl;

    std::thread udp_thread(udp_worker);
    udp_thread.join();

    return 0;
}
