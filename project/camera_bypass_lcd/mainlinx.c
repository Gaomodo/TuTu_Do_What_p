#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* FreeRTOS */
#include <FreeRTOS.h>
#include <task.h>

/* lcd */
#include <lcd.h>

/* bl808 c906 std driver */
#include <bl808_glb.h>
#include <bl808_spi.h>

/* bl808 c906 hosal driver */
#include <bl_cam.h>

/* sipeed utils */
#include <imgtool/bilinear_interpolation.h>

/* VFS / SD card */
#include <vfs.h>
#include <fatfs.h>
#include <aos/kernel.h>
#include <sys/fcntl.h>

#include "m1s_c906_xram_pwm.h"

#define PWM_PORT (0)
#define PWM_PIN (11)

void backlight_init(void)
{
    m1s_xram_pwm_init(PWM_PORT, PWM_PIN, 2000, 25);
    m1s_xram_pwm_start(PWM_PORT, PWM_PIN);
}

static uint16_t draw_buf[280 * 240];

void main()
{
    printf("\r\n===== camera_bypass_lcd start =====\r\n");

    /* 挂载 SD 卡 */
    int ret = fatfs_register();
    printf("fatfs_register ret=%d\r\n", ret);
    printf("fatfs mounted\r\n");
    // ★ 加一个不依赖摄像头的 SD 卡读写测试
  int test_fd = aos_open("/sdcard/test.txt", O_RDWR | O_CREAT | O_TRUNC);
  printf("sd test open ret=%d\r\n", test_fd);
  if (test_fd >= 0) {
      aos_write(test_fd, "hello_sd", 8);
      aos_close(test_fd);
      printf("sd test write OK\r\n");
  }

    backlight_init();
    printf("backlight done\r\n");

    st7789v_spi_init();
    st7789v_spi_set_dir(1, 0);
    printf("lcd init done\r\n");

    /* 初始化摄像头 (RGB565 给 LCD) */
    if (0 != bl_cam_mipi_rgb565_init()) {
        printf("bl cam rgb565 init failed!\r\n");
        return;
    }

     //初始化 MJPEG 编码器 (JPEG 给 SD 卡) 
    if (0 != bl_cam_mipi_mjpeg_init()) {
        printf("bl cam mjpeg init failed!\r\n");
        return;
    }

    uint16_t *picture = NULL;
    uint32_t length = 0;
    uint8_t *jpeg_data = NULL;
    uint32_t jpeg_len = 0;
    int frame_num = 0;

    while (1) {
        /* 1. 获取 RGB565 帧 → 刷 LCD */
        uint64_t curr_cnt = CPU_Get_MTimer_US();
        while (0 != bl_cam_mipi_rgb565_frame_get((uint8_t **)&picture, &length)) {
            vTaskDelay(1);
        }
        printf("[%8d] get rgb565 frame 0x%08x size %ubytes cost %fms ",
               frame_num, (uint32_t)picture, length,
               (float)(CPU_Get_MTimer_US() - curr_cnt) / 1000);

        /* 2. 缩放到 LCD 尺寸并显示 */
        BilinearInterpolation_RGB565(picture, 400, 300, draw_buf, 280, 240);
        curr_cnt = CPU_Get_MTimer_US();
        for (int i = 0; i < 280 * 240; i++) {
            draw_buf[i] = __builtin_bswap16(draw_buf[i]);
        }
        st7789v_spi_draw_picture_nonblocking(0, 0, 279, 239, draw_buf);
        printf("lcd_flush cost %fms\r\n",
               (float)(CPU_Get_MTimer_US() - curr_cnt) / 1000);

        /* 3. 释放 RGB565 帧 */
        bl_cam_mipi_rgb565_frame_pop();

        // 4. 获取 MJPEG 帧 → 存 SD 卡 (真正的 JPEG 格式!)
        int mjpeg_ret = bl_cam_mjpeg_get(&jpeg_data, &jpeg_len);
          if (mjpeg_ret != 0 || frame_num % 30 == 0) {
            printf("mjpeg_get: ret=%d jpeg_len=%d frame=%d\r\n", mjpeg_ret, jpeg_len, frame_num);
            }
        if (0 == mjpeg_ret) {
            char file_name[32];
            memset(file_name, 0, sizeof(file_name));
            sprintf(file_name, "/sdcard/%d.jpeg", frame_num);

            int fd = aos_open(file_name, O_RDWR | O_CREAT | O_TRUNC);
            printf("fd=%d \r\n", fd);
            if (fd >= 0) {
                aos_write(fd, jpeg_data, jpeg_len);
                aos_close(fd);
                printf("[%d] saved %s, %d bytes\r\n", frame_num, file_name, jpeg_len);
            }
            else {
                printf("aos_open(%s) FAILED, fd=%d\r\n", file_name, fd);
                    }
            bl_cam_mjpeg_pop();
        }

        frame_num++;
        vTaskDelay(1);
    }

    bl_cam_mipi_rgb565_deinit();
}
