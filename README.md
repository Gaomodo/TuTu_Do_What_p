# TuTu Do What —— 基于 BL808 三核 SoC 的低功耗 WiFi 感应抓拍相机

> **A low-power WiFi camera on BL808 tri-core RISC-V SoC** — radar-triggered wakeup, auto capture and MQTT image upload, powered by a single 18650 cell for 2 days.
>
> 一块 18650 供电、可远程看图、雷达感应自动抓拍的嵌入式相机。用于长期无人值守的远程监控场景（例如远程看看家里的猫在做什么）。

![platform](https://img.shields.io/badge/SoC-BL808(RISC--V%20tri--core)-blue)
![rtos](https://img.shields.io/badge/RTOS-FreeRTOS-green)
![interface](https://img.shields.io/badge/Camera-MIPI--CSI-orange)
![protocol](https://img.shields.io/badge/Upload-MQTT-lightgrey)
![power](https://img.shields.io/badge/Power-18650%20%2F%202%20days-success)

---

## 1. 项目背景

家用/宠物的远程看护摄像头通常有两个痛点：

1. **功耗高**：WiFi 相机要持续推流，电池扛不住，必须插电；
2. **隐私与流量**：7×24 小时推流既费流量，也不适合放在卧室等私密空间。

本项目的思路是 **"事件触发 + 抓拍上传"**：平时整机深度休眠（微安级），由**毫米波存在感应雷达**检测移动目标；一旦触发，中断唤醒图像处理核，抓拍一帧并通过 MQTT 上传到服务器/手机，然后重新回到休眠。

这样把"持续推流"变成"按需抓拍"，配合硬件级低功耗设计，最终用一节 18650 实现了 **2 天** 的续航。

---

## 2. 硬件平台

| 部件 | 型号 / 规格 | 说明 |
|---|---|---|
| 主控 | **Sipeed M1s Dock（BL808）** | RISC-V 三核异构：C906(RV64, 480MHz) + E907(RV32, 320MHz) + E902(160MHz) |
| 摄像头 | **OV2685**（MIPI-CSI） | 1600×1200 采集，硬件缩放至 400×300 输出 |
| 显示屏 | SPI LCD（RGB565） | 本地实时预览 |
| 感应模块 | 毫米波存在感应雷达 | 检测移动目标，INT 引脚触发唤醒 |
| 存储 | 片外 64MB PSRAM | 图像帧缓冲 / 核间图像数据共享 |
| 电池 | 18650 锂电 + CW6305 充电管理 | 单节供电 |

**片上资源分布**

```
BL808
├── C906  (RV64, 480MHz)  → 图像处理：摄像头链路 / 编码 / 抓拍
├── E907  (RV32, 320MHz)  → WiFi 协议栈 + 低功耗值守 + MQTT
├── E902  (160MHz)        → 低功耗值守
├── MIPI-CSI  RX          → 摄像头输入
├── DSP2                  → 缩放 / YUV2RGB
├── H.264/MJPEG 编码器     → 硬件 JPEG 编码
└── 8 × CAM/DMA 通道      → 多路图像 pipeline 并行
```

---

## 3. 系统架构

```
                 ┌─────────────────────── 深度休眠（默认状态）───────────────────────┐
                 │                                                                  │
   移动目标 ──► 毫米波雷达 ──INT──► E907（值守/唤醒源）                                │
                 │                       │                                          │
                 │                       ├─ 唤醒 C906                                │
                 │                       │     │                                    │
                 │                       │     ▼                                    │
                 │                       │  MIPI-CSI 采集 ──► DSP2 缩放/YUV2RGB     │
                 │                       │     │                                    │
                 │                       │     ▼                                    │
                 │                       │  DVP2AXI DMA ──► PSRAM 帧缓冲             │
                 │                       │     │                                    │
                 │                       │     ▼                                    │
                 │                       │  MJPEG 硬件编码（CAM4）                   │
                 │                       │     │                                    │
                 │                       │     ▼  XRAM Ring Buffer + IPC 中断        │
                 │                       └────► E907：raw TCP 组装 MQTT 报文 ──► 公共 Broker ──► 手机
                 │                                                                  │
                 └──────────────────── 处理完成 → 重新进入休眠 ◄─────────────────────┘
```

**核间通信设计（双通道分离）**

| 通道 | 载体 | 用途 |
|---|---|---|
| 控制命令 | **片内 XRAM Ring Buffer（504 B @ 0x22020000）** | 命令、状态、长度等小数据 |
| 图像数据 | **片外 64 MB PSRAM（共享）** | 抓拍帧，避免大块数据走小 SRAM |

C906 处理完一帧后，向 XRAM 写控制块并通过 **IPC 硬件中断**通知 E907；E907 收到中断后从 PSRAM 取帧、组 MQTT 报文上传。

---

## 4. 核心功能

- [x] MIPI-CSI 摄像头采集 → RGB565 刷 LCD + MJPEG 硬件编码**双 pipeline 并行**
- [x] 三核异构分工 + XRAM/IPC 核间通信
- [x] 毫米波雷达中断唤醒式抓拍（事件触发，非持续推流）
- [x] 低功耗：halt + gate_clk + PSRAM 自刷新，休眠功耗 ↓30%，18650 续航 **2 天**
- [x] raw TCP 手写 MQTT 报文，跨互联网图传到手机
- [x] 冷启动红蓝反转疑难 bug 定位（MIPI D-PHY 模拟层）

---

## 5. 关键技术实现

### 5.1 摄像头图像链路（寄存器级打穿）

```
MIPI-CSI 物理层 → CSI 协议层 → TSRC 时序/时钟域转换 → DSP2 Scaler + YUV2RGB → DVP2AXI DMA → 内存
```

几个关键点：

- **DVP vs MIPI**：DVP 是并口（VSYNC/HSYNC/PCLK + 并行数据），MIPI 是串行差分（1 对时钟 lane + N 对数据 lane，LP/HS 双模）。本项目走 MIPI-CSI。
- **跨时钟域缓冲**：PIX_CLK（sensor 输出）与 DSP2_CLK 不同频，TSRC 内用 FIFO 做缓冲。FIFO 阈值按两路时钟频率与行宽计算：

  ```
  threshold = (DSP2_CLK - PIX_CLK) / 1000 * MIPI_WIDTH / (DSP2_CLK / 1000) + 10
  ```

- **DMA 三要素**：源地址 / 目的地址 / 传输长度。8 个 CAM 通道各对应一路独立 DMA，因此可以做到"刷屏"与"编码"两路 pipeline 完全并行、互不抢占。
- **sensor 配置**：通过 SCCB（I2C）写 OV2685 初始化寄存器表，完成分辨率/帧率/输出格式配置。

### 5.2 双 pipeline 并行

| Pipeline | CAM 通道 | 数据流 | 用途 |
|---|---|---|---|
| 预览 | CAM5 | CSI → DSP2(YUV2RGB) → RGB565 → LCD | 本地实时预览 |
| 抓拍 | CAM4 | CSI → DSP2 → MJPEG 硬件编码 → PSRAM | 远程上传 |

### 5.3 低功耗设计

三级手段叠加：

```c
/* 1) 挂起 CPU（关核） */
GLB_Halt_CPU(GLB_CORE_ID_D0);

/* 2) 时钟门控：关掉 DSP/MM 域 PLL，省 5~10 mA */
PDS_Force_Config(1, 0);      /* forceDspGateClk = 1 */

/* 3) PSRAM 自刷新：一行代码省 ~12 mA */
PDS_Force_Config(0, 1);      /* forceDspMemStby = 1 */
```

**实测效果：休眠功耗下降约 30%，18650 理论续航从半天提升到 2 天。**

> ⚠️ 踩坑记录：GPIO 唤醒引脚位于 **MM 电源域**、依赖 MM PLL 时钟。最初把 MM 域完全断电，导致唤醒失效（热启动卡死）。最终方案改为**时钟门控**而不是整域断电。

### 5.4 核间通信：XRAM Ring Buffer + IPC 中断

- **Ring Buffer 回绕处理**：写入前做两次判断——先判断逻辑剩余空间是否够写，再判断物理地址是否连续（不够则折返到 buffer 头部）。
- **IPC 硬中断链路**（从 API 追到寄存器）：

```
bl_irq_register → Interrupt_Handler_Register → g_irqvector[]
   → Default_IRQHandler（汇编） → jalr ISR
   → *(volatile uint32_t *)0x30005000 = bit   /* 触发对端中断 */
```

- **阻塞等待与唤醒**：`xram_plat_notify_wait → bl_os_task_wait`，ISR 中 `bl_os_task_notify_isr` 唤醒目标任务。

### 5.5 MQTT 远程图传（raw TCP 手写协议）

原计划使用 LWIP 内置 MQTT 库，但实测 `publish` 始终返回 `ERR_CONN(-1)`（库存在缺陷）。最终改为**基于 raw socket 手写 MQTT 报文**：

- 报文字节级格式：固定头（2 B）+ 可变头 + payload，长度字段用变长编码；
- 实现 CONNECT / PUBLISH / PINGREQ 三个报文，QoS0 发送；
- 长连接改造：短连接延迟约 30 s → 长连接 **1 s 内** 送达；
- 图片以二进制 payload 经公共 Broker 中转（解决 NAT 穿透），手机端订阅即可接收。

### 5.6 雷达感应唤醒

雷达模块输出 INT 信号到 BL808 的 GPIO，配置为边沿中断。E907 值守期间保持低功耗监听，中断触发后按"唤醒 C906 → 采集 → 上传"流程执行，完成后再回休眠，避免持续唤醒带来的功耗开销。

---

## 6. 目录结构

```
TuTu_Do_What_p/
├── project/                     # 本仓库主体：应用层与驱动代码
│   ├── app/
│   │   ├── main.c               # 入口：三核初始化与任务创建
│   │   ├── app_camera.c/.h      # 摄像头链路：CSI/TSRC/DSP2/DMA 配置
│   │   ├── app_lcd.c/.h         # RGB565 刷屏
│   │   ├── app_mjpeg.c/.h       # MJPEG 硬件编码
│   │   ├── app_ipc.c/.h         # XRAM Ring Buffer + IPC 核间通信
│   │   ├── app_mqtt_raw.c/.h    # raw TCP 手写 MQTT 报文
│   │   ├── app_power.c/.h       # 低功耗：halt / gate_clk / PSRAM 自刷新
│   │   └── app_radar.c/.h       # 雷达中断与唤醒流程
│   ├── board/                   # 板级配置（时钟、引脚、电源域）
│   └── config/                  # 编译配置
├── docs/
│   ├── architecture.md          # 架构与数据流说明
│   ├── camera_pipeline.md       # 摄像头链路寄存器级笔记
│   └── debugging_log.md         # 疑难问题排查记录
├── images/                      # 实物图 / 效果图
└── README.md
```

---

## 7. 编译与烧录

### 7.1 依赖：SDK 的获取与集成（重要）

本仓库**只包含应用层与板级代码**，不包含官方 SDK（Bouffalo SDK 体积大、含第三方组件，不属于本项目的开发成果，因此不随仓库分发）。这是嵌入式项目的常规做法。

请按下面步骤拉取官方 SDK 并集成：

```bash
# 1) 拉取官方 SDK（Bouffalo Lab 官方仓库）
git clone https://github.com/bouffalolab/bouffalo_sdk.git
cd bouffalo_sdk
git checkout <你使用的版本 tag，例如 v2.0.x>     # 建议固定版本，保证可复现

# 2) 把本仓库代码集成进 SDK 的 application 目录
cp -r <本仓库>/project  bouffalo_sdk/examples/TuTu_Do_What

# 3) 确认 SDK 侧改动（本仓库 docs/ 中列出的 patch）
#    - 中断向量表补注册（GPIO/IPC 中断）
#    - MQTT 相关 LWIP 选项关闭（改用 raw socket）
```

> 如需完全可复现，建议把 SDK 作为 **git submodule** 引入，或用脚本固定版本号（`scripts/fetch_sdk.sh`）。

### 7.2 编译

```bash
cd bouffalo_sdk/examples/TuTu_Do_What
make CHIP=bl808 BOARD=m1s_dock
# 产物：build/build_out/TuTu_Do_What_bl808.bin
```

### 7.3 烧录

```bash
# 通过串口/UART 烧录
make flash CHIP=bl808 COMX=/dev/ttyUSB0
# Windows 下可使用 Bouffalo Lab Dev Cube 图形工具烧录 .bin
```

---

## 8. 使用说明

1. 烧录固件后，设备上电先进入 WiFi 配网（默认进入 SoftAP 配置模式，或读 Flash 中已保存的 SSID/密码）；
2. 连接成功后设备进入**深度休眠**，等待雷达触发；
3. 雷达检测到移动目标 → 自动唤醒 → 抓拍 → MQTT 上传；
4. 手机/PC 端订阅对应 topic 即可实时收到图片；
5. 本地 SPI LCD 可用于现场预览与调试。

```
MQTT Topic 约定：
  上行图片:  tutu/cam/image      (payload = JPEG 二进制)
  上行状态:  tutu/cam/status     (JSON: 电压 / 唤醒次数 / 信号强度)
  下行命令:  tutu/cam/cmd        (capture / sleep / reboot)
```

---

## 9. 调试记录与疑难排查

> 这一节记录了开发过程中耗时最多的几个问题，也是本项目技术含量的主要来源。

### 9.1 冷启动概率性红蓝反转（最深的一个坑）

**现象**：冷启动时约 14% 概率出现画面红蓝反转（U/V 分量互换）；而整机断电重新 POR 后又完全正常。

**排查过程**：用"复位粒度对照实验"逐一排除 —— sensor 复位 / 电源、CAM 字节序、CSI 数字层、lane merge、image sensor 域、TSRC、DSP2 main 域，**数字层全部干净但现象依旧**。

**根因**：**MIPI D-PHY 模拟层的 lane 对齐残留**。数字软复位覆盖不到模拟层状态，只有整 MM 域复位（`forceDspPdsRst`）才能彻底清除。

**方法论沉淀**：`SOT_ERR` 等数字错误位探测不到模拟层错位 —— 当"数字层全部正常但现象仍在"时，应当把排查方向往**模拟/物理层**转移。

### 9.2 LWIP 内置 MQTT 库不可用

`publish` 持续返回 `ERR_CONN(-1)`，换连接参数、换 Broker 均无效。判断为库实现缺陷，改为 raw TCP 手写 MQTT 报文后问题消失，且可控性更好。

### 9.3 热启动卡死 / 唤醒失效

C906 使用 `hal_halt_cpu0` 后热启动卡死。排查发现 GPIO 唤醒引脚位于 MM 电源域且依赖 MM PLL 时钟，MM 域完全断电后唤醒链路失效。最终采用**冷启动 + 时钟门控**方案规避。

### 9.4 D0(C906) 核无法响应 GPIO 中断

BL808 的 GPIO 中断线在硬件上只路由到 E907/E902，D0 核拿到的是"假中断号"。解决方案是把 GPIO 值守放到 E907 核处理，再通过 IPC 通知 C906。

### 9.5 TCP 推流偶发卡死

`write()` 在发送缓冲区满时阻塞，导致上传流程停滞。通过设置 `SO_SNDTIMEO` 与分片发送解决。

---

## 10. 性能与实测数据

| 指标 | 数值 / 说明 |
|---|---|
| 采集分辨率 | 1600×1200（sensor）→ 400×300（DSP2 缩放输出） |
| 图像链路 | MIPI-CSI，RGB565 预览 + MJPEG 抓拍双 pipeline 并行 |
| 休眠功耗 | 相比纯 halt 方案 **↓ 约 30%** |
| PSRAM 自刷新收益 | 约 **12 mA** |
| 续航 | 18650 单节，理论 **半天 → 2 天** |
| 上传时延 | MQTT 长连接下 **< 1 s** |
| 触发方式 | 雷达中断唤醒（事件触发，非持续推流） |

> 说明：上表为实测/理论估算值，具体以实物测试环境为准。

---

## 11. Roadmap

- [ ] 引入**边缘 AI 识别**：在 C906 侧接入轻量 YOLO，实现"移动检测 → 目标分类"，区分宠物/人/误触发，减少无效上传；
- [ ] 支持多目标移动轨迹记录（YOLO + SORT）；
- [ ] 本地 TF 卡离线缓存 + 断网重传；
- [ ] 抓拍间隔与灵敏度可通过 MQTT 下发远程配置。

---

## 12. 参考

- Bouffalo Lab 官方 SDK：https://github.com/bouffalolab/bouffalo_sdk
- BL808 Reference Manual（Bouffalo Lab）
- MIPI CSI-2 / D-PHY 规范
- MQTT 3.1.1 规范：http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/

---

## 关于作者
Gadodo
