/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_timer.h"

/* Can use project configuration menu (idf.py menuconfig) to choose the GPIO to blink,
   or you can edit the following line and set a number here.
*/
const gpio_num_t BLINK_GPIO = gpio_num_t(CONFIG_BLINK_GPIO);
const gpio_num_t TRIG = gpio_num_t(22);
const gpio_num_t ECHO = gpio_num_t(23);

uint32_t sonar_pulse(void){
    //Send pulse
    gpio_set_level(TRIG, 0);
    vTaskDelay(0.01 / portTICK_PERIOD_MS);
    gpio_set_level(TRIG, 1);

    // 2ms delay before echo begins
    vTaskDelay(2 / portTICK_PERIOD_MS);

    //Wait for echo
    while(!gpio_get_level(ECHO));
    int64_t start = esp_timer_get_time();
    while(gpio_get_level(ECHO));
    int64_t stop = esp_timer_get_time();


    return (stop-start)*0.1715; // millimeters per microsecond
}



extern "C" void app_main(void)
{
    /* Configure the IOMUX register for pad BLINK_GPIO (some pads are
       muxed to GPIO on reset already, but some default to other
       functions and need to be switched to GPIO. Consult the
       Technical Reference for a list of pads and their default
       functions.)
    */
    gpio_reset_pin(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(TRIG, GPIO_MODE_OUTPUT);
    gpio_set_direction(ECHO, GPIO_MODE_INPUT);
    gpio_set_level(TRIG, 1);
    while(1) {
        uint32_t dist = sonar_pulse();
        printf("Object is %d mm away\n", dist);
        vTaskDelay(100 / portTICK_PERIOD_MS);

    }
}
