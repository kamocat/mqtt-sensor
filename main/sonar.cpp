/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_timer.h"

static xQueueHandle sonar = NULL;

int64_t time;
static void sonar_pulse_handler(void * arg){
    time = esp_timer_get_time();
    xQueueSendFromISR(sonar, &time, NULL);
}

const gpio_num_t TRIG = gpio_num_t(22);
const gpio_num_t ECHO = gpio_num_t(23);

int16_t sonar_pulse(void){
    
    if(gpio_get_level(ECHO))
        return -3; // previous pulse did not finish
    //Send pulse
    gpio_set_level(TRIG, 0);
    vTaskDelay(0.01 / portTICK_PERIOD_MS);
    gpio_set_level(TRIG, 1);

    //Wait for echo
    int64_t start, stop;
    if(!xQueueReceive(sonar, &start, 3 / portTICK_PERIOD_MS))
        return -1;
    if(!xQueueReceive(sonar, &stop, 70 / portTICK_PERIOD_MS))
        return -2; // Object is too close in front of sonar

    double result = (stop-start)*0.1715; // millimeters per microsecond
    if(result > 10e3)
        return 10e3;
    else if(result < 0)
        return -4;
    return result;
}



extern "C" void app_main(void)
{
    /* Configure the IOMUX register (some pads are muxed to GPIO 
	on reset already, but some default to other
       functions and need to be switched to GPIO. Consult the
       Technical Reference for a list of pads and their default
       functions.)
    */
    sonar = xQueueCreate( 10, sizeof(int64_t));
    gpio_reset_pin(TRIG);
    gpio_reset_pin(ECHO);
    gpio_set_direction(TRIG, GPIO_MODE_OUTPUT);
    gpio_set_direction(ECHO, GPIO_MODE_INPUT);
    gpio_set_intr_type(ECHO, GPIO_INTR_ANYEDGE);
    gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1); // High priority interrupt
    gpio_isr_handler_add(ECHO, sonar_pulse_handler, (void*) NULL);
    gpio_set_level(TRIG, 1);
    while(1) {
        int16_t dist = sonar_pulse();
        printf("Object is %d mm away\n", dist);
        vTaskDelay(100 / portTICK_PERIOD_MS);

    }
}
