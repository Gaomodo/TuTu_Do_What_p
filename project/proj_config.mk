#
# M1s 追踪相机项目 — 编译配置文件
# 这里设置芯片型号、硬件功能开关、日志等级等
#
# 直接复制自原始 c906_app/proj_config.mk, 按需修改
#

# ====== 编译器配置 ======
# 直接指定工具链路径 (Windows 下 GNU make 自动检测的路径处理有问题)
# 用 = 而不是 := — 因为此时 BL_SDK_PATH 还没定义，延迟展开
CONFIG_TOOLPREFIX = $(BL_SDK_PATH)/toolchain/riscv/mingw64/bin/riscv64-unknown-elf-

# ====== Flash 大小 (MB) ======
CONFIG_BOARD_FLASH_SIZE := 2

# ====== WiFi 固件配置 ======
CONFIG_BL_IOT_FW_AP:=1
CONFIG_BL_IOT_FW_AMPDU:=0
CONFIG_BL_IOT_FW_AMSDU:=0
CONFIG_BL_IOT_FW_P2P:=0

# ====== 硬件功能开关 ======
CONFIG_ENABLE_PSM_RAM:=1         # 使能 PSRAM (64MB)
#CONFIG_ENABLE_CAMERA:=1         # 摄像头 (由组件自行控制)
#CONFIG_ENABLE_BLSYNC:=1         # 同步
#CONFIG_ENABLE_VFS_SPI:=1        # SPI 文件系统

CONFIG_ENABLE_VFS_ROMFS:=1       # ROM 文件系统
CONFIG_ENABLE_DBG_UARTID_0:=1    # 调试串口 UART0

CONFIG_ENABLE_ETHMAC:=0          # 以太网 MAC (不需要)
CONFIG_ENABLE_YUV_CAM:=1         # YUV 摄像头支持
CFLAGS += -DCONF_USER_YUV_CAM     # 直接加宏(确保子make也能看到)
CONFIG_CPU_C906:=1               # 目标核: C906 (D0 大核)
CFLAGS += -DXRAM_CPU_C906         # XRAM 组件需要这个宏来确定当前核

# ====== EasyFlash 参数存储 ======
CONFIG_ENABLE_PSM_EF_SIZE:=16K

# ====== FreeRTOS 配置 ======
CONFIG_FREERTOS_TICKLESS_MODE:=0

# ====== 蓝牙配置 (暂不需要) ======
CONFIG_BT:=0
CONFIG_BT_CENTRAL:=1
CONFIG_BT_OBSERVER:=1
CONFIG_BT_PERIPHERAL:=1
CONFIG_BT_STACK_CLI:=1
CONFIG_BLE_STACK_DBG_PRINT := 1
CONFIG_BT_STACK_PTS := 0

# ====== 日志配置 ======
# blog 启用的组件列表 — 控制哪些模块输出调试日志
LOG_ENABLED_COMPONENTS:=blog_testc hosal loopset looprt bloop
