# 航電地面支援設備 (EGSE) 遙測監控與姿態控制系統
### Avionics EGSE Telemetry Monitoring & ADCS Verification System

這是一套整合嵌入式通訊、低延遲高效能解包、即時軌道態動態模擬器與 WebGL 數位孿生視覺化的航電地面支援驗證系統（Electrical Ground Support Equipment）。

---

## 專案核心目標 (Project Objectives)

在衛星發射升空前，軟硬體與姿態控制邏輯必須在地面完成 100% 閉迴路驗證。本系統主要達成兩大任務：

1. **軌態定位與運動追蹤 (Orbit & Motion Tracking)**：即時接收並計算衛星在 3D 空間中的運動座標與姿態角（Roll / Pitch / Yaw）。
2. **自主目標凝視與鏡頭指向 (Target Tracking & Gimbal Alignment)**：模擬並驗證衛星酬載（光學相機）在軌道高速運動情境下，透過姿態回授控制平滑修正光軸，持續鎖定地表特定目標點。

---

## 四大架構

* **低延遲 (Ultra-low Latency)**：捨棄傳統文字反序列化中介層，數據到達即刻供控制迴路與視覺化模組消費，時延壓至微秒等級。
* **零開銷記憶體管理 (Zero-copy & Zero Memory Overhead)**：採用 C/C++ 二進位結構對齊與記憶體映射（Memory-mapped Struct Casting），杜絕傳統字串解析（如 JSON）導致的動態記憶體分配（`malloc`/`new`）與記憶體碎片化。
* **資料完整性與正確性 (Deterministic Data Integrity)**：實作 ICD 通訊協定校驗（CRC Checksum）與狀態位元旗標（Status Bit-flags）遮罩，保證在高速傳輸下無封包錯位或資料污染。
* **穩定控制方向 (Stable Closed-Loop Control)**：借鑑無人機（Drone）成熟的 PID 姿態回授控制架構，轉換至微重力角動量守恆環境，動態消除角速度殘留，避免姿態震盪與漂移。

---

## 系統資料流架構 (Data Pipeline Architecture)

```text
[ 姿態與軌道感測 / 模擬源 (ESP32 / 物理引擎) ]
                 │
                 │ 1. 原始二進位位元流 (UART / TCP / UDP)
                 ▼
[ C/C++ 高性能後端解析模組 ]
  ├─ 靜態預配記憶體池 (Static Buffer Pool)
  ├─ 零拷貝記憶體映射解析 (Zero-copy Struct Casting)
  ├─ 旗標控制與狀態檢查 (Status Bit-flags)
  └─ 封包 CRC 完整性校驗
                 │
                 │ 2. 標準化工程參數 (WebSocket Stream)
                 ▼
[ 即時通訊轉發層 (Python / WebSocket Server) ]
                 │
                 │ 3. 姿態四元數 (Quaternion) & 3D 空間座標
                 ▼
[ Web 3D 監控儀表板 (Three.js / WebGL) ]
  ├─ 數位孿生 3D 模型實時姿態渲染
  ├─ 軌道位移與酬載光軸追蹤視覺化
  └─ 遙測波形圖與健康診斷面板
