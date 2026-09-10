import socket
import numpy as np
import cv2 as cv

if __name__ == '__main__':
    # 创建tcp服务端套接字
    tcp_server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    # 设置端口号复用，程序退出后端口立即释放
    tcp_server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, True)

    # 绑定本机所有网卡的 8888 端口
    tcp_server.bind(("0.0.0.0", 8888))

    # 设置监听
    tcp_server.listen(128)

    # ===== 修复1: 加提示，让用户知道脚本在等待连接 =====
    print("等待 M1s 连接... (请确保 M1s 已开机并连接 WiFi)")
    print("本机监听地址: 0.0.0.0:8888")
    print("如果持续卡住，请检查 Windows 防火墙是否放行了 8888 端口")

    # 等待客户端连接（M1s 连上来后才继续）
    tcp_client, tcp_client_address = tcp_server.accept()

    # 连接成功
    print("M1s 已连接! 客户端地址:", tcp_client_address)
    print("按 'q' 键退出")

    while True:
        # 接收 4 字节长度头
        recv_data = tcp_client.recv(4)
        if len(recv_data) == 0:
            print("连接断开")
            break
        mjpeg_len = int.from_bytes(recv_data, 'little')
        tcp_client.send(recv_data)

        # 接收 MJPEG 数据
        recv_data_mjpeg = b''
        remained_bytes = mjpeg_len
        while remained_bytes > 0:
            chunk = tcp_client.recv(remained_bytes)
            if len(chunk) == 0:
                print("接收中断")
                break
            recv_data_mjpeg += chunk
            remained_bytes = mjpeg_len - len(recv_data_mjpeg)

        # 校验 JPEG 头尾
        if recv_data_mjpeg[:2] != b'\xff\xd8' or recv_data_mjpeg[-2:] != b'\xff\xd9':
            continue

        # 解码并显示
        mjpeg_data = np.frombuffer(recv_data_mjpeg, 'uint8')
        img = cv.imdecode(mjpeg_data, cv.IMREAD_COLOR)
        if img is not None:
            cv.imshow('M1s Camera Stream', img)

        # ===== 修复2: waitKey 返回 int，和 ord('q') 比较 =====
        if cv.waitKey(1) & 0xFF == ord('q'):
            print("用户退出")
            break

    tcp_client.close()
    cv.destroyAllWindows()
    print("程序结束")
