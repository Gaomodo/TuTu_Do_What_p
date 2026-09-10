#
# M1s 追踪相机项目 — 顶层 Makefile
# 用法:
#   1. 打开 Git Bash (不是 PowerShell/CMD!)
#   2. cd /e/sipeed/M1S/Tutu_do_what
#   3. export BL_SDK_PATH=/e/sipeed/M1S/Tutu_do_what/SDK
#   4. make CONFIG_CHIP_NAME=BL808 CPU_ID=D0 PROJECT_NAME=camera_bypass_lcd -j8
#
# 输出: build_out/camera_bypass_lcd.bin
#
# ====== Makefile 基础概念 ======
# 这个文件不直接编译 .c 文件。它做三件事:
#   1) 指定项目名、项目路径
#   2) 列出需要哪些 SDK 组件(每个组件 = 一个子目录, 里面有自己的 bouffalo.mk)
#   3) include SDK 的 project_common.mk, 把编译流程交给 SDK 统一编排
#
# 真实编译命令由 SDK 的 make_scripts_riscv/*.mk 自动生成,
# 最终调用 riscv64-unknown-elf-gcc 编译每个 .c 文件, 再链接成 .elf, 再转 .bin
#

# ====== 项目基本设置 ======
# PROJECT_NAME:  编译时通过命令行传入 (make PROJECT_NAME=xxx)
# PROJECT_PATH:  项目根目录 (自动取当前目录的绝对路径)
# PROJECT_BOARD: 板级配置
PROJECT_NAME ?= camera_bypass_lcd
PROJECT_PATH  = $(abspath .)
PROJECT_BOARD := evb
export PROJECT_PATH PROJECT_BOARD

# ====== 引入项目配置文件 ======
# proj_config.mk 里设置芯片型号、功能开关、LOG等级等
-include ./proj_config.mk

# ====== SDK 路径检查 ======
# BL_SDK_PATH 必须指向 SDK 根目录 (里面要有 make_scripts_riscv/ 和 components/)
ifeq ($(origin BL_SDK_PATH), undefined)
$(error   BL_SDK_PATH 未设置! 请先执行: export BL_SDK_PATH=/e/sipeed/M1S/Tutu_do_what/SDK)
endif

# ====== 组件清单 ======
# 每个 INCLUDE_COMPONENTS += xxx 表示"把 xxx 这个组件编进来"
# 组件就是 SDK/components/ 下的子目录, 每个目录里必须有 bouffalo.mk

# --- 核心组件 (缺了编不过) ---
INCLUDE_COMPONENTS += bl808_c906_freertos   # FreeRTOS 内核 (D0核)
INCLUDE_COMPONENTS += bl808                 # BL808 芯片基础
INCLUDE_COMPONENTS += bl808_std             # BL808 标准外设驱动 (GPIO/SPI/I2C/CAM...)
INCLUDE_COMPONENTS += newlibc               # C 标准库 (malloc/printf/string...)
INCLUDE_COMPONENTS += hosal                 # 硬件抽象层 (bl_cam_init 等)
INCLUDE_COMPONENTS += freetype              # FreeType 字体库 (DSP2 OSD 依赖)

# --- 图像/摄像头相关 ---
INCLUDE_COMPONENTS += dsp2                  # DSP2 图像处理子系统
INCLUDE_COMPONENTS += dsp2_cli_demo         # DSP2 演示代码
INCLUDE_COMPONENTS += blai_nn               # AI 推理库
INCLUDE_COMPONENTS += blai_npu_encoder      # NPU 编码器
INCLUDE_COMPONENTS += sensor                # 摄像头 sensor 驱动 (OV2685/GC0308...)
INCLUDE_COMPONENTS += bl_mm                 # 多媒体框架
INCLUDE_COMPONENTS += venc_device           # 视频编码设备
INCLUDE_COMPONENTS += venc_framework        # 视频编码框架

# --- 文件系统/SD卡 ---
INCLUDE_COMPONENTS += vfs                   # 虚拟文件系统
INCLUDE_COMPONENTS += fatfs                 # FAT32 文件系统
INCLUDE_COMPONENTS += sdh_helper            # SD 卡读写封装

# --- 网络 (WiFi用) ---
INCLUDE_COMPONENTS += lwip                  # LWIP TCP/IP 协议栈
INCLUDE_COMPONENTS += mjpeg_sender_bl808    # MJPEG 推流库
INCLUDE_COMPONENTS += rtsp_server           # RTSP 服务器

# --- 工具/杂项 ---
INCLUDE_COMPONENTS += yloop                 # 事件循环
INCLUDE_COMPONENTS += cli                   # 命令行接口
INCLUDE_COMPONENTS += utils                 # 工具函数
INCLUDE_COMPONENTS += blog                  # 日志系统
INCLUDE_COMPONENTS += freertos_posix        # FreeRTOS POSIX 兼容层
INCLUDE_COMPONENTS += blfdt                 # 设备树
INCLUDE_COMPONENTS += romfs                 # ROM 文件系统
INCLUDE_COMPONENTS += bl808_ring            # 环形缓冲
INCLUDE_COMPONENTS += bl808_xram            # 核间通信 (XRAM)
INCLUDE_COMPONENTS += bl_os_adapter         # OS 适配层

# --- Sipeed M1s 专用组件 ---
INCLUDE_COMPONENTS += m1s_start             # M1s 启动代码
INCLUDE_COMPONENTS += m1s_model_runner      # 模型推理
INCLUDE_COMPONENTS += m1s_tools             # Sipeed 工具库 (imgtool/bilinear...)
INCLUDE_COMPONENTS += lfs                   # LittleFS
INCLUDE_COMPONENTS += m1s_lfs_c906          # C906 LittleFS 适配
INCLUDE_COMPONENTS += m1s_common_xram       # 核间通信公共部分
INCLUDE_COMPONENTS += m1s_c906_xram         # C906 核间通信 (PWM/GPIO 远程调用)

# --- 你自己的应用代码 ---
# 构建系统会在 $(PROJECT_PATH)/$(PROJECT_NAME) 目录下找你的代码
# 也就是 Tutu_do_what/camera_bypass_lcd/
INCLUDE_COMPONENTS += $(PROJECT_NAME)

# ====== 全局编译选项 ======
CFLAGS += -DROMFS_STATIC_ROOTADDR=0x582f0000

# ====== 引入 SDK 构建引擎 ======
# 这一行之后, SDK 接管所有编译逻辑:
#   - 扫描所有组件目录
#   - 为每个组件生成编译规则
#   - 调用 riscv64-unknown-elf-gcc 编译 .c → .o
#   - 打包 .o → .a (静态库)
#   - 链接所有 .a → .elf
#   - objcopy 转 .bin (可烧录)
include $(BL_SDK_PATH)/make_scripts_riscv/project_common.mk
