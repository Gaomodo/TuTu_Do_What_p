#!/bin/bash
# ===================================================
#  M1s BL808 Docker 编译脚本 (Plan B)
#  前提: 已安装 Docker Desktop for Windows
#
#  用法: 在 Git Bash 里执行:
#    cd /e/sipeed/M1S/Tutu_do_what
#    bash docker_build.sh
#
#  第一次运行会下载镜像 (~2GB)，之后秒开
# ===================================================

set -e

PROJECT_DIR="/e/sipeed/M1S/Tutu_do_what"
SDK_DIR="${PROJECT_DIR}/SDK"
TOOLCHAIN_DIR="${SDK_DIR}/toolchain/riscv/mingw64"

echo "========== M1s BL808 Docker Build =========="

# ① 检查 Docker 是否可用
if ! docker info > /dev/null 2>&1; then
    echo "ERROR: Docker 未运行! 请先启动 Docker Desktop"
    exit 1
fi

# ② 检查工具链是否已解压（需要从宿主机挂载进容器）
if [ ! -f "${TOOLCHAIN_DIR}/bin/riscv64-unknown-elf-gcc.exe" ]; then
    echo "ERROR: 工具链未找到: ${TOOLCHAIN_DIR}"
    echo "请先解压 Xuantie-900 工具链到该目录"
    exit 1
fi

# ③ 拉取/构建编译镜像
IMAGE_NAME="m1s_build:latest"

echo "[1/3] 准备 Docker 镜像..."
cat > /tmp/Dockerfile.m1s << 'DOCKERFILE'
FROM ubuntu:22.04

# 安装编译依赖
RUN apt-get update && apt-get install -y \
    make \
    python3 \
    python3-pip \
    && rm -rf /var/lib/apt/lists/*

# 工具链从宿主机挂载，不内置
WORKDIR /project
DOCKERFILE

docker build -t ${IMAGE_NAME} -f /tmp/Dockerfile.m1s /tmp 2>&1 | tail -3
echo "[1/3] 镜像就绪"

# ④ 编译
echo "[2/3] 开始编译..."

docker run --rm \
    -v "${PROJECT_DIR}":/project \
    -v "${TOOLCHAIN_DIR}":/toolchain \
    -e BL_SDK_PATH=/project/SDK \
    -e CONFIG_TOOLPREFIX=/toolchain/bin/riscv64-unknown-elf- \
    --workdir /project \
    ${IMAGE_NAME} \
    make CONFIG_CHIP_NAME=BL808 CPU_ID=D0 PROJECT_NAME=camera_bypass_lcd -j8

# ⑤ 检查结果
echo ""
echo "[3/3] 编译完成"
if [ -f "${PROJECT_DIR}/build_out/camera_bypass_lcd.bin" ]; then
    SIZE=$(wc -c < "${PROJECT_DIR}/build_out/camera_bypass_lcd.bin")
    echo "SUCCESS: build_out/camera_bypass_lcd.bin (${SIZE} bytes)"
else
    echo "FAILED: 检查上方错误信息"
    exit 1
fi
