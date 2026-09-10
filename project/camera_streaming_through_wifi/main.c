//this is  the beginning for my first project about freeRTOS and camera streaming through wifi on c906 m1s board. I will use the m1s_c906_xram_wifi.h sdk to connect to wifi and upload the camera stream to a server.




#include <stdbool.h>
#include <stdio.h>

/* FreeRTOS */
#include <FreeRTOS.h>
#include <task.h>

/* bl808 c906 std driver */
#include <bl808_glb.h>
#include <bl_cam.h>

#include <m1s_c906_xram_wifi.h>

void main()
{
    vTaskDelay(1);
    bl_cam_mipi_mjpeg_init();
    m1s_xram_wifi_init();
    m1s_xram_wifi_connect("liuxo_desktop", "123456789810");
    m1s_xram_wifi_upload_stream("192.168.201.3", 8888);
}