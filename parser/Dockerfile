# 階段 1: 編譯環境
FROM ubuntu:24.04 AS builder
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    g++ \
    cmake \
    make \
    git \
    libssl-dev \
    zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
RUN git clone --depth 1 https://github.com/machinezone/IXWebSocket.git \
    && cd IXWebSocket \
    && mkdir build && cd build \
    && cmake -DUSE_TLS=0 .. \
    && make -j$(nproc) install

# 編譯 C++ 解析器（加入 -lz 解決壓縮函式庫符號鏈結）
COPY main.cpp .
RUN g++ -O3 -std=c++17 main.cpp -o telemetry_parser -lixwebsocket -lz -lpthread

# 階段 2: 運行環境 (極簡化)
FROM ubuntu:24.04
RUN apt-get update && apt-get install -y zlib1g && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY --from=builder /build/telemetry_parser .
CMD ["./telemetry_parser"]
