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
#include <hal_sdh.h>

#include "m1s_c906_xram_pwm.h"
#include <bl808_cam.h>

#include "m1s_c906_xram_wifi.h"

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


          /* ★ 加回 MJPEG 编码器初始化 */
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
        
        if(frame_num % 200 == 0){
            m1s_xram_wifi_capture_trigger(2, 400);
        }

        /* 3. 释放 RGB565 帧 */
        bl_cam_mipi_rgb565_frame_pop();

        // // 4. 获取 MJPEG 帧
        // int mjpeg_ret = (&jpeg_data, &jpeg_len);
        //   if (0== bl_cam_mjpeg_get(&jpeg_data, &jpeg_len)) {
        //     printf("mjpeg_get: ret=%d jpeg_len=%d frame=%d\r\n", mjpeg_ret, jpeg_len, frame_num);
            
        //      bl_cam_mjpeg_pop();
        //   }

        

        frame_num++;
        vTaskDelay(1);
    }

    bl_cam_mipi_rgb565_deinit();
}
